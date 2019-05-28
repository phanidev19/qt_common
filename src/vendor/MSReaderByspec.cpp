#include "MSReaderByspec.h"
#include "MzCalibration.h"
#include "PathUtils.h"
#include "PicoLocalDecompressQtUse.h"
#include "PlotBase.h"
#include "ProgressBarInterface.h"
#include "QtSqlUtils.h"
#include "pmi_common_ms_debug.h"
#include "VendorPathChecker.h"

#include <PmiQtCommonConstants.h>

#include <plot_utils.h>
#include <qt_string_utils.h>
#include <qt_utils.h>

#include <CacheFileManager.h>

#define CONVERT_WAIT_MILLISECOND 197 //note: prime number helps progress bar look not synchonized

_PMI_BEGIN

static const QLatin1String CACHE_SUFFIX(".byspec2");
static const QLatin1String CHROMATOGRAM_SUFFIX(".chromatogram_only.byspec2");

/// If CompressionInfo table is empty, populate it.
PMI_COMMON_MS_EXPORT Err bugPatchSchema_CompressionInfo(QSqlDatabase & db)
{
    Err e = kNoErr;
    QStringList tableNames;
    QSqlQuery q = makeQuery(db,true);

    //There's a bug where the Peaks_MS1Centroided table contains CompressionInfoId, but the CompressionInfo does not exists or is empty.
    //This happened because when making the Peaks_MS1Centroided, the CompressionInfo table is not copied/translated.
    e = GetSQLiteTableNames(db, tableNames); eee;
    if (!tableNames.contains(kCompressionInfo)) {
        e = QEXEC_CMD(q, "CREATE TABLE IF NOT EXISTS CompressionInfo(Id INTEGER PRIMARY KEY, MzDict BLOB, IntensityDict BLOB, Property TEXT, Version TEXT)"); eee;
    }
    e = QEXEC_CMD(q, "SELECT COUNT(Id) FROM CompressionInfo"); eee;
    if (q.next()) {
        int count = q.value(0).toInt();
        if (count == 0) {
            //For now, CompressionInfo always have the same two entries; see pwiz_/.../Serializer_byspec.cpp
            e = QEXEC_CMD(q, "INSERT INTO CompressionInfo(Id, MzDict, IntensityDict, Property, Version) VALUES (1,NULL,NULL,'pico:local:bkr', '0.1')"); eee;
            e = QEXEC_CMD(q, "INSERT INTO CompressionInfo(Id, MzDict, IntensityDict, Property, Version) VALUES (2,NULL,NULL,'pico:local:centroid', '1.0')"); eee;
        }
    } else {
        e = kBadParameterError; eee;
    }

error:
    return e;
}

_MSREADER_BEGIN

////////////////////////////////////////////////////////
//
// class MSReaderByspec
//
////////////////////////////////////////////////////////

static Err findProxyFile(const QString &inputFilename,
                         const CacheFileManagerInterface &cacheFileManager,
                         MSConvertOption convert_options, QString *proxyFile)
{
    Err e = kNoErr;

    if (inputFilename.endsWith(".byspec2", Qt::CaseInsensitive)) {
        *proxyFile = inputFilename;

        return e;
    }

    if (convert_options == ConvertChronosOnly) {
        e = cacheFileManager.findOrCreateCachePath(CHROMATOGRAM_SUFFIX, proxyFile); ree;

        return e;
    }

    e = cacheFileManager.findOrCreateCachePath(CACHE_SUFFIX, proxyFile); ree;

    return e;
}

Err ContainsCentroidedMS1(QString byspecProxyFilename, int *has_centroided_ms1)
{
    Err e = kNoErr;
    *has_centroided_ms1 = 0;

    {
        int centroid_count = 0, profile_count = 0;
        QSqlDatabase db = QSqlDatabase::addDatabase(kQSQLITE, "check_centroid");
        db.setDatabaseName(byspecProxyFilename);
        QSqlQuery q;
        debugMs() << "opening file:" << byspecProxyFilename << endl;
        if (!db.open()) {
            warningMs() << "Could not open file:" << byspecProxyFilename << endl;
            warningMs() << db.lastError().text();
            e = kBadParameterError; eee;
        }
        q = makeQuery(db, true);
        e = QEXEC_CMD(q, "SELECT MetaText FROM Spectra WHERE MSLevel=1"); eee;
        //Just check the first MS1 to see if it's centroided.  Sufficient for now.
        while (q.next()) {
            QString metaText = q.value(0).toString();
            if (metaText.contains("<PeakMode>")) {
                if (metaText.contains("centroid spectrum")) {
                    centroid_count++;
                } else if (metaText.contains("profile spectrum")) {
                    profile_count++;
                    break;
                }
            }
        }
        if (profile_count > 0) {
            *has_centroided_ms1 = 0;
            // Yong observed that MS1 can contain both centroid and profile.  This happened with an
            // Amgen intact data (PL-41948_Red_MT031215_23.d)  The first 30 scans or so had centroid
            // meta tag and without any x-y points; the rest were profile.  We will assume such data
            // is profile.
            if (centroid_count > 0) {
                debugMs() << "Note the MS1 contains both centroid and profile.  Centroid count of "
                             "centroid_count="
                          << centroid_count;
            }
        } else if (centroid_count > 0) {
            *has_centroided_ms1 = 1;
        }
    }
    // Both "db" and "query" are destroyed because they are out of scope
    QSqlDatabase::removeDatabase("check_centroid");

error:
    return e;
}

static Err
IsScanNumberFilledProperly(QSqlQuery & q, bool * has_proper_ScanNumber) {
    Err e = kNoErr;
    *has_proper_ScanNumber = true;

    e = QEXEC_CMD(q, "SELECT COUNT(Id) FROM cc.Spectra WHERE ScanNumber is NULL OR Length(ScanNumber) <= 0"); eee;
    if (!q.next()) {
        e = kBadParameterError; eee;
    }
    if (q.value(0).toInt() > 0) {
        *has_proper_ScanNumber = false;
        return e;
    }

    e = QEXEC_CMD(q, "SELECT COUNT(Id) FROM Spectra WHERE ScanNumber is NULL OR Length(ScanNumber) <= 0"); eee;
    if (!q.next()) {
        e = kBadParameterError; eee;
    }
    if (q.value(0).toInt() > 0) {
        *has_proper_ScanNumber = false;
        return e;
    }

error:
    return e;
}

static Err
MergeCentroidedToFirstByspecFile(QString byspecProxyFilename, QString centroid_byspecProxyFilename, int update_Peaks_table, QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    //TODO(Kevin): Copy the CompressionInfo and CompressionInfoId from this centroided byspec2 file and copy to Peaks_MS1Centroided.
    //Be sure to consider that CompressionInfoId maybe different; or deal with it later once we actually have more than one compression flavor.
    {
        if (progress) progress->setText("Merging scans...");
        bool has_proper_ScanNumber = false;
        QSqlDatabase db = QSqlDatabase::addDatabase(kQSQLITE, "extract_centroid");
        db.setDatabaseName(byspecProxyFilename);
        QSqlQuery q;
        if (!db.open()) {
            warningMs() << "Could not open file:" << byspecProxyFilename << endl;
            warningMs() << db.lastError().text();
            e = kBadParameterError; eee;
        }
        q = makeQuery(db, true);

        e = QPREPARE(q, "ATTACH ? AS cc"); eee;
        q.bindValue(0,centroid_byspecProxyFilename);
        e = QEXEC_NOARG(q); eee;

        e = IsScanNumberFilledProperly(q, &has_proper_ScanNumber); eee;

        if (update_Peaks_table) {
        } else {
            e = QEXEC_CMD(q, "DROP TABLE IF EXISTS Peaks_MS1Centroided"); eee;
            e = QEXEC_CMD(q, "CREATE TABLE IF NOT EXISTS Peaks_MS1Centroided(Id INTEGER PRIMARY KEY, PeaksMz BLOB, PeaksIntensity BLOB, SpectraIdList TEXT, PeaksCount INT, MetaText TEXT, Comment TEXT, IntensitySum REAL, CompressionInfoId INT)"); eee;
        }

        //Note: The second byspec only contains MS1, so PeaksId do not match between the two files.
        //Have to be careful as cc.PeaksId must match to the first byspec file.
        //Create index first.
        if (progress) progress->setText("Merging scans...");
        if (has_proper_ScanNumber) {
            e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS idx_SpectraScanNumber ON Spectra(ScanNumber)"); eee;
            e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS cc.idx_SpectraScanNumber ON Spectra(ScanNumber)"); eee;
        } else {
            e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS idx_SpectraNativeId ON Spectra(NativeId)"); eee;
            e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS cc.idx_SpectraNativeId ON Spectra(NativeId)"); eee;
        }
        if (progress) progress->setText("Merging scans...");
        e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS cc.idx_SpectraMSLevel ON Spectra(MSLevel)"); eee;
        if (update_Peaks_table) {
            transaction_begin(db);
            QSqlQuery q_update = makeQuery(db,true);
            e = QPREPARE(q_update, "UPDATE Peaks SET PeaksMz=?,PeaksIntensity=?,PeaksCount=?,MetaText=?,Comment=?,IntensitySum=?,CompressionInfoId=? WHERE Id = ?");
            if (has_proper_ScanNumber) {
                e = QEXEC_CMD(q, "\
                SELECT s.PeaksId, k.PeaksMz, k.PeaksIntensity,k.PeaksCount,k.MetaText,k.Comment,k.IntensitySum,k.CompressionInfoId \
                FROM cc.Peaks k \
                JOIN cc.Spectra ccs ON ccs.PeaksId = k.Id \
                JOIN Spectra s ON s.ScanNumber = ccs.ScanNumber \
                WHERE ccs.MSLevel = 1 \
                "); eee;
            } else {
                //Match with ScanNumber
                e = QEXEC_CMD(q, "\
                SELECT s.PeaksId, k.PeaksMz, k.PeaksIntensity,k.PeaksCount,k.MetaText,k.Comment,k.IntensitySum,k.CompressionInfoId \
                FROM cc.Peaks k \
                JOIN cc.Spectra ccs ON ccs.PeaksId = k.Id \
                JOIN Spectra s ON s.NativeId = ccs.NativeId \
                WHERE ccs.MSLevel = 1 \
                "); eee;
            }
            while(q.next()) {
                debugMs() << "Did we want to update PeaksMz? Peaks ID = " << q.value(0).toLongLong();
                qlonglong peaksId = q.value(0).toLongLong();
                QVariant peaksMz = q.value(1);
                QVariant peaksInten = q.value(2);
                int peaksCount = q.value(3).toInt();
                QVariant metaText = q.value(4);
                QVariant comment = q.value(5);
                QVariant intensitySum = q.value(6);
                q_update.bindValue(0, peaksMz);
                q_update.bindValue(1, peaksInten);
                q_update.bindValue(2, peaksCount);
                q_update.bindValue(3, metaText);
                q_update.bindValue(4, comment);
                q_update.bindValue(5, peaksId);
                q_update.bindValue(6, intensitySum);
                e = QEXEC_NOARG(q_update); eee;
                if (progress) progress->refreshUI();
            }
            transaction_end(db);
        } else {
            if (has_proper_ScanNumber) {
                //Match with ScanNumber
                e = QEXEC_CMD(q, "\
                INSERT INTO Peaks_MS1Centroided(Id,PeaksMz,PeaksIntensity,PeaksCount,MetaText,Comment,IntensitySum,CompressionInfoId) \
                SELECT s.PeaksId, k.PeaksMz, k.PeaksIntensity,k.PeaksCount,k.MetaText,k.Comment,k.IntensitySum,k.CompressionInfoId \
                FROM cc.Peaks k \
                JOIN cc.Spectra ccs ON ccs.PeaksId = k.Id \
                JOIN Spectra s ON s.ScanNumber = ccs.ScanNumber \
                WHERE ccs.MSLevel = 1 \
                "); eee;
            } else {
                //Match with ScanNumber
                e = QEXEC_CMD(q, "\
                INSERT INTO Peaks_MS1Centroided(Id,PeaksMz,PeaksIntensity,PeaksCount,MetaText,Comment,IntensitySum,CompressionInfoId) \
                SELECT s.PeaksId, k.PeaksMz, k.PeaksIntensity,k.PeaksCount,k.MetaText,k.Comment,k.IntensitySum,k.CompressionInfoId \
                FROM cc.Peaks k \
                JOIN cc.Spectra ccs ON ccs.PeaksId = k.Id \
                JOIN Spectra s ON s.NativeId = ccs.NativeId \
                WHERE ccs.MSLevel = 1 \
                "); eee;
            }
        }

        //TODO: Do proper CompressionInfo table copy and possibly remap CompressionInfoId; for now, CompressionInfo always have the same two entries.
        e = pmi::bugPatchSchema_CompressionInfo(db); eee;
    }
    // Both "db" and "query" are destroyed because they are out of scope
    QSqlDatabase::removeDatabase("extract_centroid");

error:
    return e;
}

static QString
make_centroid_byspecProxyFilename(QString byspecProxyFilename) {
    QString centroid_byspecProxyFilename = byspecProxyFilename;
    centroid_byspecProxyFilename.replace(".byspec2", ".centroided.byspec2");
    return centroid_byspecProxyFilename;
}

/// This is a patch for already existing files (as of 6-7-2014) where the MS1Centroided may not be populated correctly.
static Err
patch_Peaks_MS1Centroided(QString byspecProxyFilename) {
    Err e = kNoErr;

    //make sure both files exists
    if (!QFile::exists(byspecProxyFilename)) {
        return e;
    }
    QString centroid_byspecProxyFilename = make_centroid_byspecProxyFilename(byspecProxyFilename);
    if (!QFile::exists(centroid_byspecProxyFilename)) {
        return e;
    }

    int database_added = 0;
    bool recompute = false;
    {
        QStringList tableNames;
        QSqlDatabase db = QSqlDatabase::addDatabase(kQSQLITE, "check_centroid");
        database_added = 1;
        QFileInfo fi1(byspecProxyFilename), fi2(centroid_byspecProxyFilename);
        QDateTime last_date_to_consider_for_recompute = QDateTime::fromString("2014:06:08 00:00:00","yyyy:MM:dd HH:mm:ss");
//        commonMsDebug() << "last_date_to_consider_for_recompute=" << last_date_to_consider_for_recompute;
//        commonMsDebug() << "fi1.lastModified()=" << fi1.lastModified();
//        commonMsDebug() << "fi2.lastModified()=" << fi2.lastModified();

        if (fi1.lastModified() < last_date_to_consider_for_recompute && fi2.lastModified() < last_date_to_consider_for_recompute ) {
            db.setDatabaseName(byspecProxyFilename);
            if (!db.open()) {
                warningMs() << "Could not open file:" << byspecProxyFilename << endl;
                warningMs() << db.lastError().text();
                e = kBadParameterError; eee;
            }

            e = GetSQLiteTableNames(db, tableNames); eee;

            // The bug case we are trying to fix is if MS1Centroided is empty but the centroided byspec file exists
            if (tableNames.contains(kPeaks_MS1Centroided) && QFile::exists(centroid_byspecProxyFilename)) {
                QSqlQuery q = makeQuery(db, true);
                e = QEXEC_CMD(q, "SELECT COUNT(Id) FROM Peaks_MS1Centroided"); eee;
                if (!q.next()) {
                    e = kBadParameterError; eee;
                }
                if (q.value(0).toInt() <= 0) {
                    recompute = true;
                }
            }
        }
    }
    //commonMsDebug() << byspecProxyFilename << ": " << "recompute=" << recompute;

    //To be safe, remove old database first.
    if (recompute) {
        int has_centroided_ms1 = 0;
        e = ContainsCentroidedMS1(byspecProxyFilename, &has_centroided_ms1); eee;
        e = MergeCentroidedToFirstByspecFile(byspecProxyFilename, centroid_byspecProxyFilename, has_centroided_ms1, nullptr); eee;
    }

error:
    // Both "db" and "query" are destroyed because they are out of scope
    if (database_added) QSqlDatabase::removeDatabase("check_centroid");
    return e;
}

static Err
handleUserCancel(QProcess & run, QString outDirStr, QString outFileStr) {
    Err e = kNoErr;
    QString fullpath = joinDirAndFilename(outDirStr, outFileStr);

    run.kill();
    QFile::remove(fullpath);
    QFile::remove(fullpath+"-journal");
    e = kUserCanceled; eee;

error:
    return e;
}

static Err
setBrukerRegistry(QVariant newValue, QVariant &outPreviousValue)
{
    Err e = kNoErr;

    QString registryKey("UseRecalibratedSpectra");
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Bruker Daltonik\\CompassXport", QSettings::NativeFormat);

    QVariant previousValue = settings.value(registryKey);
//    commonMsDebug() << "Previous value for UseRecalibratedSpectra = " << previousValue << endl;
//    commonMsDebug() << "New value for UseRecalibratedSpectra = " << newValue << endl;

    // Set the value if appropriate, otherwise delete the key completely
    if(newValue.isValid() && !newValue.isNull()) {
        settings.setValue(registryKey, newValue);
    } else {
        settings.remove(registryKey);
    }

    // Save the old value so we can use it later
    outPreviousValue = previousValue;

//error:
    return e;
}

Err makeByspec(QString inputMSFileName, QString byspecProxyFilename,
               MSConvertOption convert_options, const CentroidOptions &centroidOption,
               const IonMobilityOptions &ionMobilityOptions, QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    QProcess run;
    QVariant previousBrukerRegistryValue;

    QString outFileStr = getFilename(byspecProxyFilename);
    QString outDirStr = getDirOnly(byspecProxyFilename);
    QString centroid_byspecProxyFilename = make_centroid_byspecProxyFilename(byspecProxyFilename);
    QString centroid_outFileStr = getFilename(centroid_byspecProxyFilename);

    if (VendorPathChecker::isWatersFile(inputMSFileName)) {
        debugMs() << "Waters file detected. Running pico console." << endl;
        QString pico_console = PathUtils::getPicoExecPath();
        if (pico_console.size() <= 0) {
            e = kBadParameterError; eee;
        }

        QDir outputDirectory(outDirStr);
        QString outputPath = outputDirectory.absoluteFilePath(outFileStr);

        /* modes
         -n 2 --- profile compression and ms1 centroided table
         -n 4 --- centroiding MS2
         -n 5 --- (temporary patch) no compression on byonic
         */

        QStringList args_profile;
        args_profile << "-r";
        args_profile << "-t" << "1";
        args_profile << "-n" << "4";
        args_profile << "-l" << "100"; // Verbose mode, output every 100 scans

        if (convert_options == ConvertChronosOnly) {
            // Only get chromatograms
            args_profile << "-d" << "1";
        } else {
            // Read everything
            args_profile << "-d" << "0";
        }

        args_profile << "-i" << inputMSFileName;
        args_profile << "-o" << outputPath;

        QStringList centroidArgs = centroidOption.buildPicoConsoleArguments();
        args_profile << centroidArgs;

        debugMs() << "pico console path:" << pico_console;
        debugMs() << "args:" << args_profile;

        run.setProcessChannelMode(QProcess::MergedChannels);

        //Start pico console extraction
        run.start(pico_console, args_profile);
        if (!run.waitForStarted()) {
            e = kBadParameterError; eee;
        }
        if (progress) progress->setText("Converting scans...");
        while (!run.waitForFinished(CONVERT_WAIT_MILLISECOND)) {
            QString str(run.readAll());
            if (str.size() > 0) {
                debugMs() << str;
            }
            if (progress) {
                progress->refreshUI();
                if (progress->userCanceled()) {
                    e = handleUserCancel(run, outDirStr, outFileStr); eee;
                }
            }
        }
        e = centroidOption.saveOptionsToDatabase(outputPath); eee;
    } else if (VendorPathChecker::formatBruker(inputMSFileName)
               == VendorPathChecker::ReaderBrukerFormatTims) {
        debugMs() << "BrukersTims file detected. Running PMI-MSConvert." << endl;
        QString console = PathUtils::getPMIMSConvertPath();
        if (console.size() <= 0) {
            e = kBadParameterError; eee;
        }

        QDir outputDirectory(outDirStr);
        QString outputPath = outputDirectory.absoluteFilePath(outFileStr);

        QStringList args_profile;
        args_profile << QStringLiteral("--centroid");
        args_profile << QStringLiteral("--imo.summingTolerance")
                     << QString::number(ionMobilityOptions.summingTolerance);
        args_profile << QStringLiteral("--imo.mergingTolerance")
                     << QString::number(ionMobilityOptions.mergingTolerance);
        args_profile << QStringLiteral("--imo.summingUnit")
                     << QString::number(ionMobilityOptions.summingUnit);
        args_profile << QStringLiteral("--imo.mergingUnit")
                     << QString::number(ionMobilityOptions.mergingUnit);
        args_profile << QStringLiteral("--imo.retainNumberOfPeaks")
                     << QString::number(ionMobilityOptions.retainNumberOfPeaks);
        args_profile << inputMSFileName;
        args_profile << QStringLiteral("--output=%1").arg(byspecProxyFilename);

        //TODO:
        if (convert_options == ConvertChronosOnly) {
        } else {
        }

        debugMs() << "PMi-MSConvert console path:" << console;
        debugMs() << "args:" << args_profile;

        run.setProcessChannelMode(QProcess::MergedChannels);

        //Start pico console extraction
        run.start(console, args_profile);
        if (!run.waitForStarted()) {
            e = kBadParameterError; eee;
        }
        if (progress) progress->setText("Converting scans...");
        while (!run.waitForFinished(CONVERT_WAIT_MILLISECOND)) {
            QString str(run.readAll());
            if (str.size() > 0) {
                debugMs() << str;
            }
            if (progress) {
                progress->refreshUI();
                if (progress->userCanceled()) {
                    e = handleUserCancel(run, outDirStr, outFileStr); eee;
                }
            }
        }
        e = centroidOption.saveOptionsToDatabase(outputPath); eee;
    } else {
        // We only want to do pico for profile data if this is a Bruker BAF file
        bool do_pico = false;
        if (VendorPathChecker::isBrukerAnalysisBaf(inputMSFileName)) {
            do_pico = true;
        }

        debugMs() << "do_pico = " << do_pico << endl;

        QStringList args_profile;
        //Either the profile and TIC byspec file
        args_profile << "--byspec" << inputMSFileName;
        args_profile << "--outfile" << outFileStr;
        args_profile << "-o" << outDirStr;
        if (convert_options == ConvertChronosOnly) {
            //args_profile << "--filter" << "scanNumber 1-1"; //keep something to make sure it filters something
            args_profile << "--filter" << "index -1"; //keep something to make sure it filters something
        } else if (convert_options == ConvertWithCentroidButNoPeaksBlobs){
            //args_profile << "--filter" << "msLevel 1-";  //Note: calling "msLevel" filter slows down bruker data convert time.
            args_profile << "--option" << "ignore_spectra_content";
        } else {
            //args_profile << "--filter" << "msLevel 1-";  //Note: calling "msLevel" filter slows down bruker data convert time.
        }

        if (do_pico) {
            // This expects pmi_pico.dll in the same path as msconvert-pmi.exe
            args_profile << "--option" << "do_pico";
        }

        // Tells pwiz we only want original data when using Bruker files.
        // We can always set this, even for non-Bruker files since it will only effect Bruker files.
        args_profile << "--option" << "bruker_original_only";

    //    commonMsDebug() << "program:" << program;
    //    commonMsDebug() << "args:" << args;
        const QString msconvert_pmi = PathUtils::getMSConvertExecPath();
        if (msconvert_pmi.size() <= 0) {
            e = kBadParameterError; eee;
        }
        run.setProcessChannelMode(QProcess::MergedChannels);

        debugMs() << "msconvert path:" << msconvert_pmi;
        debugMs() << "args:" << args_profile;
        // Set Bruker Registry value to use calibration
        setBrukerRegistry(1, previousBrukerRegistryValue);

        //Start msconvert-pmi extraction
        run.start(msconvert_pmi, args_profile);
        if (!run.waitForStarted()) {
            e = kBadParameterError; eee;
        }
        if (progress) progress->setText("Converting scans...");
        while (!run.waitForFinished(CONVERT_WAIT_MILLISECOND)) {
            QString str(run.readAll());
            if (str.size() > 0) {
                debugMs() << str;
            }
            if (progress) {
                progress->refreshUI();
                if (progress->userCanceled()) {
                    e = handleUserCancel(run, outDirStr, outFileStr); eee;
                }
            }
        }

        //We now create centroided data and merge into the first byspec file.
        if (convert_options != ConvertChronosOnly) {
            int has_centroided_ms1 = 0;
            //Check to see that we actually need to do this.  If the original file is centroided, we do not need to implement this.
            e = ContainsCentroidedMS1(byspecProxyFilename, &has_centroided_ms1); eee;
            debugMs() << "has_centroided_ms1 = " << has_centroided_ms1;

            if(!has_centroided_ms1) {
                //We want to avoid saving profile data because they are quite large and takes a long time to save.
                //But centroid data is quite small and we need them for XIC and other purposes.  So, we'll extract centroid data
                //If profile data, make centroided data.
                //If centroid data, populate Peaks with the centroid data.
                QStringList args_ms1_centroid;
                //The temporary byspec file for centroided data, needed for XIC and TIC extraction.
                //Only extract MS1 data, as that's all we need.
                args_ms1_centroid << "--byspec" << inputMSFileName;
                args_ms1_centroid << "--outfile" << centroid_outFileStr;
                args_ms1_centroid << "-o" << outDirStr;

                if (MSReaderBase::getCentroidMethod_Vendor_Or_Custom(centroidOption, inputMSFileName) == CentroidOptions::CentroidMethod_PreferVendor) {
                    args_ms1_centroid << "--filter" << "peakPicking true 1-";
                } else {
                    //peakPicking gauss:0.3:0:5000 1-
                    args_ms1_centroid << "--filter" << centroidOption.buildMSConvertGaussFilterString();
                }

                // Always do pico for centroid
                args_ms1_centroid << "--option" << "do_pico";

                // Tells pwiz we only want original data when using Bruker files.
                // We can always set this, even for non-Bruker files since it will only effect Bruker files.
                args_ms1_centroid << "--option" << "bruker_centroid_only";

                //Note: calling "msLevel" filter causes a slow down.  So, let's not call it.
                //TODO: But not calling means extracting non-ms1 peaks, which may slow down the extraction or
                //increase disk space.  Consider making --option "extract_ms1_only" to at least reduce disk space usage
                //args_ms1_centroid << "--filter" << "msLevel 1-1";

                debugMs() << "constructing centroided byspec2 of " << inputMSFileName << " as " << centroid_outFileStr;
                debugMs() << "msconvert path:" << msconvert_pmi;
                debugMs() << "args:" << args_ms1_centroid;

                //Start msconvert-pmi extraction
                run.start(msconvert_pmi, args_ms1_centroid);
                if (!run.waitForStarted()) {
                    e = kBadParameterError; eee;
                }
                debugMs() << "starting.." << endl;
                if (progress) progress->setText("Converting centroided scans...");
                while (!run.waitForFinished(CONVERT_WAIT_MILLISECOND)) {
                    QString str(run.readAll());
                    if (str.size() > 0) {
                        debugMs() << str;
                    }
                    if (progress) {
                        progress->refreshUI();
                        if (progress->userCanceled()) {
                            e = handleUserCancel(run, outDirStr, outFileStr); eee;
                        }
                    }
                }
                debugMs() << "done." << endl;

                //Now merge back to the database
                e = MergeCentroidedToFirstByspecFile(byspecProxyFilename, centroid_byspecProxyFilename, has_centroided_ms1, progress); eee;
                debugMs() << "merge to sample byspec done." << endl;
            }
        }
    }

error:
    if (!QFile::exists("C:\\pmi_keep_byspec.txt")) {
        //QFile(byspecProxyFilename).remove();
        QFile(centroid_byspecProxyFilename).remove();
    }

    // Set Bruker Registry value back to its original value
    QVariant v;
    setBrukerRegistry(previousBrukerRegistryValue, v);

    return e;
}

static Err
makeByspecScanNumberList(QString inputMSFileName, QString byspecProxyFilename, const QList<int> & scanNumberList, bool use_config_filename, QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    QProcess run;

    if (QFile::exists(byspecProxyFilename)) {
        bool removed = QFile::remove(byspecProxyFilename);
        if (!removed) {
            //can't remove it, let's hope the file is not corrupt/good enough.
            return e;
        }
    }

    bool do_pico = false;
    if (VendorPathChecker::isBrukerAnalysisBaf(inputMSFileName)) {
        do_pico = true;
    }

    QString outFileStr = getFilename(byspecProxyFilename);
    QString outDirStr = getDirOnly(byspecProxyFilename);
    QString configFileStr = joinDirAndFilename(outDirStr, outFileStr+"_config.txt");

    QStringList args_profile;
    args_profile << "--byspec" << inputMSFileName;
    args_profile << "--outfile" << outFileStr;
    args_profile << "-o" << outDirStr;
    args_profile << "--option" << "ignore_chromatagram_completely";

    if (do_pico) {
        args_profile << "--option" << "do_pico";
    }

    QString indexListStr;
    foreach(int scanNumber, scanNumberList){
        if (indexListStr.size() > 0) {
            indexListStr += " ";
        }
        indexListStr += QString::number(scanNumber-1);
    }
    if (indexListStr.size() > 0) {
        bool proper_write_to_config = false;
        if (use_config_filename) {
            QFile file(configFileStr);
            if(file.open(QIODevice::WriteOnly)) {
                //filter="index 1 2 3 4"
                QTextStream out(&file);
                out << "filter=\"index ";
                out << indexListStr;
                out << "\"";
                proper_write_to_config = true;
            }
            file.close();
        }
        if (proper_write_to_config) {
            args_profile << "--config" << configFileStr;
        } else {
            args_profile << "--filter" << QString("index "+indexListStr);
        }
    }

    const QString msconvert_pmi = PathUtils::getMSConvertExecPath();
    if (msconvert_pmi.size() <= 0) {
        e = kBadParameterError; eee;
    }
    run.setProcessChannelMode(QProcess::MergedChannels);

    debugMs() << "msconvert path:" << msconvert_pmi;
    debugMs() << "args:" << args_profile;

    //Start msconvert-pmi extraction
    run.start(msconvert_pmi, args_profile);
    if (!run.waitForStarted()) {
        e = kBadParameterError; eee;
    }

    debugMs() << "--- run output begin---\n";
    while (!run.waitForFinished(CONVERT_WAIT_MILLISECOND)) {
        QString str(run.readAll());
        if (str.size() > 0) {
            debugMs() << str;
        }
        if (progress) {
            progress->refreshUI();
            if (progress->userCanceled()) {
                e = handleUserCancel(run, outDirStr, byspecProxyFilename); eee;
            }
        }
    }
    debugMs() << "--- run output end---\n";

//    if (!run.waitForFinished(msecs_to_wait)) {
//        e = kBadParameterError; eee;
//    }

error:
    return e;
}

static Err
GetByspecVersion(const QString & fileName, double * versionNumber)
{
    Err e = kNoErr;
    *versionNumber=0;

    bool added_db = false;
    if (QFile::exists(fileName))
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(kQSQLITE, "test_byspecversion");
        added_db = true;
        db.setDatabaseName(fileName);
        QSqlQuery q;
        if (!db.open()) {
            debugMs() << "Could not open " << fileName;
            e = kBadParameterError; eee;
        }
        q = makeQuery(db, true);
        e = QEXEC_CMD(q, "SELECT Value FROM FilesInfo WHERE lower(Key)='version'");
        if (e != kNoErr) {
            db.close();
            eee;
        }
        if (q.next()) {
            *versionNumber = q.value(0).toDouble();
        }
        db.close();
    }
error:
    if (added_db)
        QSqlDatabase::removeDatabase("test_byspecversion");
    return e;
}

/*!
 * \brief IsFullyPopulated checks to see if the .byspec2 file is fully populateds
 * Note: the implementation checks the Peaks table only.  The chromatography is expected to be
 * populated due to how openFile() works.
 * \param byspecProxyFilename
 * \param full_ms true if fully populated
 * \return
 */
Err IsFullyPopulated(QString byspecProxyFilename, bool * full_ms)
{
    Err e = kNoErr;
    *full_ms = false;

    //TODO: add cached value instead of going through all of Peaks table.
    {  //scope db and q to prevent memory leak when removeDatabase gets called.
        QSqlDatabase db;
        QSqlQuery q;

        db = QSqlDatabase::addDatabase(kQSQLITE, "checkByspecContent");
        db.setDatabaseName(byspecProxyFilename);
        if (!db.open()) {
            e = kError; eee; //kErrSQLOpen;
        }
        q = makeQuery(db, true);
        e = QEXEC_CMD(q, "SELECT COUNT(Id) FROM Peaks WHERE PeaksMz is NULL"); eee;
        if (q.next()) {
            int count = q.value(0).toInt();
            if (count == 0) {
                *full_ms = true;
            }
        }
    }

error:
    QSqlDatabase::removeDatabase("checkByspecContent");
    if (e) {
        *full_ms = false;
        eee_absorb;
    }
    return e;
}

MSReaderByspec::MSReaderByspec(const CacheFileManagerInterface &cacheFileManager)
    : m_cacheFileManager(&cacheFileManager)
    , m_containsSpectraMobilityValue(false)
{
    static int count = 0;
    //Note: making this into a new instead of normal instance to avoid the warning message
    //during removeDatabase call: "QSqlDatabasePrivate::removeDatabase: connection 'byspec_msreader' is still in use, all queries will cease to work."
    //We destory the new instance and then call removeDatabase.
    m_byspecDB = new QSqlDatabase;
    //Note: addDatabase of same connection name appears to be causing problems.  It might be
    //because having two or more instances of MSReaderByspec is bad because the later instance will replace
    //the old instance.  And the destructor removes the database, which then causes other instances to have its connection
    //to disappear.  This is solved by making a different connection name per instance.

    m_databaseConnectionName = QString("%1_%2)").arg(kbyspec_msreader).arg(count++);
    *m_byspecDB = QSqlDatabase::addDatabase(kQSQLITE, m_databaseConnectionName);
    //commonMsDebug() << "Constructor MSReaderByspec(), QSqlDatabase::connectionNames()=" << QSqlDatabase::connectionNames();
}

MSReaderByspec::~MSReaderByspec() {
    if (!m_openIndirectWithDatabase) {
        m_byspecDB->close();
    }
    delete m_byspecDB;
    QSqlDatabase::removeDatabase(m_databaseConnectionName);
}

MSReaderBase::MSReaderClassType MSReaderByspec::classTypeName() const
{
    return MSReaderClassTypeByspec;
}

template<class T>
void safe_free(T* & ptr) {
    if (ptr) {
        free(ptr);
        ptr = nullptr;
    }
}

void MSReaderByspec::clear() {
    MSReaderBase::clear();
    m_tableNames.clear();
    m_tableColumnNames.clear();
    m_containsSpectraMobilityValue = false;
}

bool MSReaderByspec::canOpen(const QString & fileName) const {
    Q_UNUSED(fileName);
    return true;
}

Err MSReaderByspec::indirectOpen_becareful(const QSqlDatabase & db)
{
    Err e = kNoErr;

    m_openIndirectWithDatabase = true;
    *m_byspecDB = db;
    setFilename(db.databaseName());
    e = _postOpenMetaPopulate(); eee;

error:
    return e;
}

// delete all waters files if older than the given time stamp
static Err
checkWatersVersion_ByDate(const QString & byspecProxyFilename, bool * do_makeByspec)
{
    Err e = kNoErr;
    {
        QSqlDatabase db;
        QSqlQuery q;
        e = addDatabaseAndOpen("checkWatersData_xx_2016", byspecProxyFilename, db); eee;
        q = makeQuery(db, true);

        e = QEXEC_CMD(q, "SELECT Key,Value FROM FilesInfo WHERE Key='CompileTime'"); eee;
        if (q.next()) {
            QString value = q.value(1).toString();
            //Feb 10 2016 | 13:01:55
            QStringList datetime = value.split("|");
            if (datetime.size() == 2) {
                QString dateStr = datetime[0].trimmed();
                QDateTime compileDateFile = QDateTime::fromString(dateStr, "MMM dd yyyy");
                //NOTE: Change the following date as we make bug fixes.
                //QDate::shortMonthName: Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep,Oct,Nov,Dec

                //Compile time of pico is "Jan 18 2017 | 17:51:33".  So we want the date before this
                //but after bugs started appearing.  Having the dateCheck be after Jan 18 will cause the
                //condition below to be true, causing constant re-make of the byspec file.
                QDateTime dateCheck = QDateTime::fromString("Jan 15 2017", "MMM dd yyyy");
                uint compileDateFile_u = compileDateFile.toTime_t();
                uint dateCheck_u = dateCheck.toTime_t();
                debugMs() << "dateFile,dateCheck,dateFile_u,dateCheck_u=" << compileDateFile << "," << dateCheck<< "," <<compileDateFile_u<< "," <<dateCheck_u << " diff=" << dateCheck_u - compileDateFile_u;
                //If older than the given check time, we'll re-create
                if (compileDateFile_u < dateCheck_u) {
                    debugMs() << "Do make byspec for waters file: " << byspecProxyFilename;
                    *do_makeByspec = true;
                }
            }
        }
        db.close();
    }

error:
    QSqlDatabase::removeDatabase("checkWatersData_xx_2016");  //Must close. TODO: make into RAII style
    return e;
}

Err MSReaderByspec::openFile(const QString & fileName, MSConvertOption convert_options, QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    QString byspecProxyFilename;
    bool do_makeByspec = false;

    clear();

    if (fileName.endsWith(".byspec2", Qt::CaseInsensitive)){
        byspecProxyFilename = fileName;
    } else {
        //If mzML or other formats, try and convert to byspec
        //First check to see if the byspec file exists and the version number is proper.
        e = findProxyFile(fileName, *m_cacheFileManager, convert_options,
                          &byspecProxyFilename); ree;

        if (QFile::exists(byspecProxyFilename)) {
            //Already open? Do nothing and return that file.
            if (m_byspecDB->isOpen()) {
                QString databaseName = m_byspecDB->databaseName();
                if (databaseName == byspecProxyFilename) {
                    setFilename(fileName);
                    return e;
                }
            }
            //Check to see if the byspec2 version is old and should be removed.
            double versionNumber = 0;
            bool full_ms = false;
            GetByspecVersion(byspecProxyFilename, &versionNumber);
            if (versionNumber < 3.99999) {
                do_makeByspec = true;
            }
            //Check that the contents are all properly filled if they want ConvertFull
            //Assume the file is always fully populated
            if (do_makeByspec == false && ( (versionNumber < 4.9999 && convert_options == ConvertFull) ||
                 (convert_options == ConvertFull && BYOMAP_DEFAULT_CONVERT_OPTION != ConvertFull)) ) {
                e = IsFullyPopulated(byspecProxyFilename, &full_ms); eee;
                if (!full_ms) {
                    do_makeByspec = true;
                }
            }
            if (do_makeByspec == false && versionNumber < 5.99999) {  //MUST increase pwiz version number to 6
                //Note: normally, we want to delete all old versions.  But to make our current customer from re-building the byspec file,
                //we will not delete them if it's a bruker analysis file.
                bool is_bruker_analysis_baf = VendorPathChecker::isBrukerAnalysisBaf(fileName);
                if (!is_bruker_analysis_baf) {
                    do_makeByspec = true;
                }
            }
            if (VendorPathChecker::isWatersFile(fileName) && convert_options == ConvertFull){
                bool options_match = false;
                e = m_centroidOptionInByspec.matchesDatabase(byspecProxyFilename, &options_match); eee;
                if (!options_match) {
                    do_makeByspec = true;
                }
            }
            //We want to use the latest Waters files.
            if (VendorPathChecker::isWatersFile(fileName) && !do_makeByspec) {
                e = checkWatersVersion_ByDate(byspecProxyFilename, &do_makeByspec); nee;
            }

            // for BrukerTims with version less than 6.3
            if (VendorPathChecker::formatBruker(fileName)
                    == VendorPathChecker::ReaderBrukerFormatTims
                && versionNumber < 6.29999) {
                do_makeByspec = true;
            }

            //Note: we want to use if statements above instead of if-else, as we want to check against all cases.
            //E.g. version 5 may contain analysis.baf.

            if (do_makeByspec) {
                if (!QFile::remove(byspecProxyFilename)) {
                    warningMs() << "Could not remove stale cache byspec file: " << byspecProxyFilename << endl;
                    e = kError; eee;
                }
            }

        }
        else {
            do_makeByspec = true;
        }
    }

    //Now try and open the byspec file
    //First check to see if the database is already open.
    if (m_byspecDB->isOpen()) {
        QString databaseName = m_byspecDB->databaseName();
        if (databaseName == byspecProxyFilename) {
            setFilename(fileName);
            return e;
        }
    }
    //Prepare to open database. Close first.
    if (m_byspecDB->isOpen()) {
        m_byspecDB->close();
    }

    if (do_makeByspec) {
        e = makeByspec(fileName, byspecProxyFilename, convert_options, m_centroidOptionInByspec,
                       ionMobilityOptions(), progress); eee;
    }

    //Check if file exists
    if (!QFile::exists(byspecProxyFilename)) {
        e = kFileOpenError; eee;
    }

    if (!do_makeByspec) {
        e = patch_Peaks_MS1Centroided(byspecProxyFilename); eee;
    }

    //Open connection
    m_byspecDB->setDatabaseName(byspecProxyFilename);
    if (!m_byspecDB->open()) {
        warningMs() << "Could not open file:" << byspecProxyFilename;
        warningMs() << m_byspecDB->lastError().text();
        e = kSQLiteExecError; eee;
    }
    //Set filename
    setFilename(fileName);
    e = _postOpenMetaPopulate(); eee;

    if (true) {
        //PMI-1546 need this index to speed up the getXICData
        QSqlQuery q = makeQuery(m_byspecDB, true);
        e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS idx_SpectraScanNumber ON Spectra(ScanNumber)"); eee_absorb;
    }

error:
    return e;
}

Err MSReaderByspec::_postOpenMetaPopulate()
{
    Err e = kNoErr;
    QStringList spectraColNames;

    e = GetSQLiteTableNames(*m_byspecDB, m_tableNames); eee;

    // calculate m_containsNonEmptyPeaksMS1CentroidedTable
    m_containsNonEmptyPeaksMS1CentroidedTable = false;
    if (m_tableNames.contains(kPeaks_MS1Centroided)) {
        QSqlQuery q = makeQuery(*m_byspecDB, true);
        e = QEXEC_CMD(q, QString("SELECT count(*) FROM %1").arg(kPeaks_MS1Centroided)); eee;
        if (q.next()) {
            m_containsNonEmptyPeaksMS1CentroidedTable = (q.value(0).toInt() > 0);
        }
    }

    //NOTE: not needed, but might as well keep it.
    e = GetTableColumnNames(*m_byspecDB, kSpectra, spectraColNames); eee;
    m_tableColumnNames[kSpectra] = spectraColNames;

    m_containsSpectraMobilityValue = spectraColNames.contains(kMobilityValue);

    // Verify if compression table and proper linking column both exist
    if (m_tableNames.contains(kPeaks)) {
        bool containsCompressionInfoColumn = tableContainsColumn(*m_byspecDB, kPeaks, kCompressionInfoId);
        m_containsCompression = (containsCompressionInfoColumn && m_tableNames.contains(kCompressionInfo));
    } else {
        m_containsCompression = false;
    }

    if (m_tableNames.contains(kPeaks_MS1Centroided)) {
        bool containsCentroidedCompressionInfoColumn = tableContainsColumn(*m_byspecDB, kPeaks_MS1Centroided, kCompressionInfoId);
        m_containsCentroidedCompression = (containsCentroidedCompressionInfoColumn && m_tableNames.contains(kCompressionInfo));
    } else {
        m_containsCentroidedCompression = false;
    }

//    commonMsDebug() << "m_containsCompression = " << m_containsCompression << endl;
//    commonMsDebug() << "m_containsCentroidedCompression = " << m_containsCentroidedCompression << endl;

    if(m_containsCompression) {
        fetchCompressionInfo();
    }
    if (true && m_byspecDB) {
        QSqlQuery q = makeQuery(*m_byspecDB, true);
        //Used by getXICData and getScanPrecursorInfo; if it fails, process without it anyway
        e = QEXEC_CMD(q, "PRAGMA synchronous = 0"); eee;
        e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS idx_SpectraRetentionTime ON Spectra(RetentionTime)"); eee_absorb;
    }

error:
    return e;
}


Err MSReaderByspec::fetchCompressionInfo()
{
    Err e = kNoErr;
    //commonMsDebug() << "fetchCompressionInfo";

    QSqlQuery q;

    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; eee;
    }

    q = makeQuery(m_byspecDB, true);

    e = QEXEC_CMD(q, "SELECT Id, MzDict, IntensityDict, Property, Version FROM " + kCompressionInfo); eee;

    m_compressionInfoHolder.clearList();

    while(q.next()) {
        int id = q.value(0).toInt();
        QByteArray mzDictionary = q.value(1).toByteArray();
        QByteArray intensityDictionary = q.value(2).toByteArray();
        QString property = q.value(3).toString();
        QString version = q.value(4).toString();

        m_compressionInfoHolder.addCompressionInfo(id, mzDictionary, intensityDictionary, property, version);

        //commonMsDebug() << "CompressionInfo ID = " << id << endl;
    }

    //commonMsDebug() << "CompressionInfo Count = " << m_compressionInfoHolder.getSize();

error:
    return e;
}

Err MSReaderByspec::closeFile()
{
    Err e = kNoErr;
    MSReaderBase::clear();
//error:
    m_byspecDB->close();
    return e;
}

Err MSReaderByspec::_pruneTICChromatogram(point2dList &inpoints) const
{
    Err e = kNoErr;
    long totalNumberScans, startScan, endScan;
    e = getNumberOfSpectra(&totalNumberScans, &startScan, &endScan); eee;
    if (inpoints.size() == totalNumberScans) {
        point2dList outpoints;
        int i = 0;
        int level = 0;
        QString metaText;
        QSqlQuery q;
        if (!m_byspecDB->isOpen()) {
            e = kFileOpenError; eee;
        }
        q = makeQuery(m_byspecDB, true);
        e = QEXEC_CMD(q, QString("SELECT MSLevel,MetaText FROM Spectra ORDER BY ScanNumber")); eee;
        while(q.next()) {
            if (i >= int(inpoints.size())) {
                debugMs() << "Expected number of Spectra different from expected:" << inpoints.size();
                e = kBadParameterError; eee;
            }
            level = q.value(0).toInt();
            metaText = q.value(1).toString();
            if (level == 1 && MetaTextParser::getScanMethod(metaText) != ScanMethodZoomScan) {
                outpoints.push_back(inpoints[i]);
            }
            i++;
        }
        std::swap(outpoints,inpoints);
    }

error:
    return e;
}

static bool is_centroid_data(const QString &metaText, const point2dList &plist)
{
    bool is_centroided = true;
    PeakPickingMode pick_mode = MetaTextParser::getPeakPickingMode(metaText);
    if (pick_mode != PeakPickingModeUnknown) {
        is_centroided = pick_mode == PeakPickingCentroid;
    } else {
        is_centroided = isCentroidPlotData_HeuristicsBasedOnMedianDistance(plist);
    }
    return is_centroided;
}

/*!
 * \brief returns the sum of the critical points between the given mz range
 * \param plot input profile or centroid data
 * \param is_centroid true if centroid data
 * \param mz_start start mz range, if -1, assume no mz contraint
 * \param mz_end end mz range, if -1, assume no mz contraint
 * \param range_sum_value sum of y-intensity values
 * \return true if sum if greater than zero (something was found)
 */
static double extractMS1Sum(const PlotBase &plot, bool is_centroid, double mz_start, double mz_end)
{
    double range_sum_value = 0;
    if (plot.getPointListSize() <= 0)
        return range_sum_value;
    point2dList inList;
    bool sum_all = mz_start < 0 && mz_end < 0; //If -1 and -1, sum all.

    if (is_centroid) {
        if (sum_all) {
            return getYSumAll(plot.getPointList());
        } else {
            plot.getPoints(mz_start, mz_end, true, inList);
        }
    } else {
        point2dIdxList idxList;
        point2dList maxPoints;
        plot.getMaxIndexList(idxList);
        plot.makePointsFromIndex(idxList, maxPoints);
        if (sum_all) {
            return getYSumAll(maxPoints);
        }
        getPointsInBetween(maxPoints, mz_start, mz_end, inList, true);
    }

    //critial_point = getMaxPoint(inList);
    for (unsigned int i = 0; i < inList.size(); i++) {
        range_sum_value += inList[i].y();
    }
    return range_sum_value;
}

Err MSReaderByspec::constructTICByDatabaseQuery(point2dList &points) const
{
    Err e = kNoErr;
    QSqlQuery q;

    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; eee;
    }

    q = makeQuery(m_byspecDB, true);

    e = QEXEC_CMD(q, "SELECT DataX,DataY,DataCount,MetaText FROM Chromatogram WHERE ChromatogramType='total ion current chromatogram' OR Identifier LIKE 'TIC%'"); eee;
    if (q.next()) {
        QString metaText = q.value(3).toString();
        e = byteArrayToPlotBase(q.value(0).toByteArray(), q.value(1).toByteArray(), points, true); eee;

        e = _pruneTICChromatogram(points); eee;
        //convert to minutes if in seconds.
        if (metaText.toLower().contains("second")) {
            for (point2d & p : points) {
                p.rx() = p.x() / 60.0;
            }
        }
    }

error:
    return e;
}

Err MSReaderByspec::constructTICByQueryingCentroidedMS1(point2dList &inpoints) const
{
    Err e = kNoErr;

    QSqlQuery q;
    point2d critial_point;
    double tic_value = 0;

    // Check to see if we have the intensity sum column, if not, then just get out
    if (m_containsNonEmptyPeaksMS1CentroidedTable) {
        if (!tableContainsColumn(*m_byspecDB, kPeaks_MS1Centroided, kIntensitySum)) {
            return e;
        }
    } else {
        if (!tableContainsColumn(*m_byspecDB, kPeaks, kIntensitySum)) {
            return e;
        }
    }

    QString ms1_extract_template = "\
            SELECT s.MetaText, s.RetentionTime, k.IntensitySum\
            FROM %1 k \
            JOIN Spectra s ON s.PeaksId = k.Id WHERE MSLevel=1\
            ORDER BY s.RetentionTime\
            ";

    QString cmd = m_containsNonEmptyPeaksMS1CentroidedTable ?
                ms1_extract_template.arg(kPeaks_MS1Centroided)
              : ms1_extract_template.arg(kPeaks);

    inpoints.clear();

    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; eee;
    }
    q = makeQuery(m_byspecDB, true);

    e = QEXEC_CMD(q, cmd); eee;
    while(q.next()) {

        QString metaText = q.value(0).toString();
        double retTimeSeconds = q.value(1).toDouble();  //msconvert-pmi extracts it into seconds; the MetaText should contain "<RetentionTimeUnit>second</RetentionTimeUnit>"
        ScanMethod scanMethod = MetaTextParser::getScanMethod(metaText);
        if (scanMethod != ScanMethodZoomScan) {
            tic_value = q.value(2).toDouble();
            critial_point.rx() = retTimeSeconds/60.0;
            critial_point.ry() = tic_value;
            inpoints.push_back(critial_point);
        }
    }

error:
    return e;

}

Err MSReaderByspec::constructTICBySummingCentroidedMS1(point2dList &inpoints) const
{
    Err e = kNoErr;
    //We have either data from profile or centroided data.
    //Choose to use the centroided data, if the centroided table exists.
    //Otherwise, use the normal Peaks table and apply manual centroiding.
    if (!m_containsNonEmptyPeaksMS1CentroidedTable) {
        debugMs() << "Note: using custom centroiding algorithm to extract TIC";
    } else {
        debugMs() << "Note: using vendor centroiding algorithm to extract TIC";
    }

    QSqlQuery q;
    PlotBase plot;
    point2dList & plist = plot.getPointList();
    point2d critial_point;
    double tic_value = 0;
    pico::PicoLocalDecompressQtUse pico;
    const CompressionInfo* compressionInfo = nullptr;
    int compressionInfoId = -1;
    double* restoredMz = nullptr;
    float* restoredIntensity = nullptr;
    int uncompressedSize = 0;
    QByteArray mzValues;
    QByteArray intensityValues;

    bool containsPertinentCompression
        = (m_containsCompression && !m_containsNonEmptyPeaksMS1CentroidedTable)
        || (m_containsCentroidedCompression && m_containsNonEmptyPeaksMS1CentroidedTable);

    QString ms1_extract_template;
    if(containsPertinentCompression) {
        ms1_extract_template = "\
        SELECT k.PeaksMz, k.PeaksIntensity, s.MetaText, s.RetentionTime, k.CompressionInfoId, k.NativeId \
        FROM %1 k \
        JOIN Spectra s ON s.PeaksId = k.Id \
        WHERE MSLevel=1\
        ORDER BY s.RetentionTime\
        ";
    } else {
        ms1_extract_template = "\
        SELECT k.PeaksMz, k.PeaksIntensity, s.MetaText, s.RetentionTime \
        FROM %1 k \
        JOIN Spectra s ON s.PeaksId = k.Id \
        WHERE MSLevel=1 \
        ORDER BY s.RetentionTime \
        ";
    }

    QString cmd = m_containsNonEmptyPeaksMS1CentroidedTable ?
                ms1_extract_template.arg(kPeaks_MS1Centroided)
              : ms1_extract_template.arg(kPeaks);

    inpoints.clear();

    //commonMsDebug() << ms1_extract_template << endl;

    //TODO: if profile data, make sure the Peaks_MS1Centroided is populated
    //If centroided data Peaks may not be populated.
    //Make a patch call and populate either the Peaks or Peaks_MS1Centroided

    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; eee;
    }
    q = makeQuery(m_byspecDB, true);

    e = QEXEC_CMD(q, cmd); eee;
    while(q.next()) {

        bool foundValidCompression = false;
        bool sortByX = false;

        if(containsPertinentCompression) {
            //commonMsDebug() << "Contains compression, so give it a shot." << endl;
            //commonMsDebug() << q.value(4).toInt() << endl;

            // If the joined CompressionInfoId is NULL, then that means this particular row of Peaks is not compressed
            if(!q.value(4).isNull()) {
                //commonMsDebug() << "CompressionInfoId was not null" << endl;
                compressionInfoId = q.value(4).toInt();
                compressionInfo = m_compressionInfoHolder.findById(compressionInfoId);

                if(compressionInfo != nullptr) {
                    foundValidCompression = true;

                    QString nativeId = q.value(5).toString();
                    // NativeId
                    QString functionNumber = pico::getFunctionNumberFromNativeId(nativeId);
                    pico::WatersCalibration::Coefficents coef;
                    pico::getCoefficientInformation(functionNumber, *m_byspecDB, &coef);

                    pico.decompress(coef, compressionInfo, q.value(0).toByteArray(), q.value(1).toByteArray(), &restoredMz, &restoredIntensity, &uncompressedSize);

                    if (restoredMz == nullptr || restoredIntensity == nullptr) {
                        if (uncompressedSize > 0) {
                            e = kMemoryError; eee;
                        } else {
                            // No data, so put a 0 point and move on
                            QString metaText = q.value(2).toString();
                            ScanMethod scanMethod = MetaTextParser::getScanMethod(metaText);
                            if (scanMethod != ScanMethodZoomScan) {
                                double retTimeSeconds = q.value(3).toDouble();
                                critial_point.rx() = retTimeSeconds/60.0;
                                critial_point.ry() = 0;
                                inpoints.push_back(critial_point);
                            }

                            continue;
                        }
                    }

                    //mzValues = QByteArray(reinterpret_cast<const char*>(restoredMz), uncompressedSize * 8);
                    //intensityValues = QByteArray(reinterpret_cast<const char*>(restoredIntensity), uncompressedSize * 4);
                    toByteArrayReference_careful(reinterpret_cast<const char*>(restoredMz), uncompressedSize * 8, mzValues);
                    toByteArrayReference_careful(reinterpret_cast<const char*>(restoredIntensity), uncompressedSize * 4, intensityValues);
                }
            }
        }

        // If no valid compression is found (or compression does not exist), use the data as normal
        if(!foundValidCompression) {
            mzValues = q.value(0).toByteArray();
            intensityValues = q.value(1).toByteArray();
            sortByX = true;
        }

        e = byteArrayToPlotBase(mzValues, intensityValues, plist, sortByX); eee;
        QString metaText = q.value(2).toString();
        double retTimeSeconds = q.value(3).toDouble();  //msconvert-pmi extracts it into seconds; the MetaText should contain "<RetentionTimeUnit>second</RetentionTimeUnit>"
        ScanMethod scanMethod = MetaTextParser::getScanMethod(metaText);
        if (scanMethod != ScanMethodZoomScan) {
            bool is_centroided = true;
            if (!m_containsNonEmptyPeaksMS1CentroidedTable) {
                is_centroided = is_centroid_data(metaText, plist);
            }

            tic_value = extractMS1Sum(plot, is_centroided, -1, -1);
            critial_point.rx() = retTimeSeconds/60.0;
            critial_point.ry() = tic_value;
            inpoints.push_back(critial_point);
        }

        // Deallocate any data returned via decompression
        safe_free(restoredMz);
        safe_free(restoredIntensity);
    }

error:
    // Deallocate any data returned via decompression
    safe_free(restoredMz);
    safe_free(restoredIntensity);

    return e;
}

Err MSReaderByspec::getBasePeak(point2dList *points) const
{
    Q_UNUSED(points);

    return kFunctionNotImplemented;
}

Err MSReaderByspec::getTICData(point2dList *points) const
{
    Q_ASSERT(points);

    Err e = kNoErr;
    bool successful = false;

    e = constructTICByQueryingCentroidedMS1(*points); eee;
    successful = (points->size() > 0);

    if (!successful) {
        e = constructTICBySummingCentroidedMS1(*points); eee;
        successful = (points->size() > 0);
    }

    //If something got extracted, use that. Otherwise, we will try querying the Chromatogram table
    if(!successful) {
        e = constructTICByDatabaseQuery(*points); eee;
        successful = (points->size() > 0);
    }

    if(!successful) {
        debugMs() << "Unable retrieve TIC (MSReaderByspec::getTICData) for datafile: " << getFilename();
    }

error:
    return e;
}

static QString makeOutputByspecFilename_ForScanNumberList(const QString &_databaseName,
                                                          const QList<int> &scanNumberList)
{
    QString databaseName(_databaseName);
    QString firstLocFilename = databaseName.replace(".byspec2", "") + "_scan_"
        + QString::number(scanNumberList.front()) + "_" + QString::number(scanNumberList.back())
        + "_" + QString::number(scanNumberList.size()) + ".byspec2";

    QFile file(firstLocFilename);
    if (file.open(QIODevice::WriteOnly)) {
        file.close();
        return firstLocFilename;
    }

    QString outfile = getAppDataLocationFile(firstLocFilename, true);
    return outfile;
}

Err MSReaderByspec::extractScanNumbersToByspec(const QList<int> &uniqueAndOrderedScanNumberList,
                                             QList<point2dList> &pointsList,
                                             QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    pointsList.clear();
    if (uniqueAndOrderedScanNumberList.size() <= 0) {
        DEBUG_WARNING_LIMIT(debugMs() << "No scans to extract. scanNumberList.size()=" << uniqueAndOrderedScanNumberList.size(), 25);
        return e;
    }
    if (progress) progress->setText("Extracting and saving scans...");

    QSqlQuery queryMainBySpec = makeQuery(m_byspecDB, true);
    QSqlQuery querySmallBySpec;
    QString databaseName = m_byspecDB->databaseName();
    QString outputByspecName = makeOutputByspecFilename_ForScanNumberList(databaseName, uniqueAndOrderedScanNumberList);
    bool database_added = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(kQSQLITE, "extract_scans");
        database_added = true;

        e = makeByspecScanNumberList(getFilename(), outputByspecName, uniqueAndOrderedScanNumberList, true, progress); eee;

        db.setDatabaseName(outputByspecName);
        if (!db.open()) {
            warningMs() << "Could not open file:" << outputByspecName << endl;
            warningMs() << m_byspecDB->lastError().text();
            e = kBadParameterError; eee;
        }

        querySmallBySpec = makeQuery(db, true);

        //Gather data from the small byspec file
        e = QPREPARE(querySmallBySpec, "SELECT P.PeaksMz, P.PeaksIntensity, P.PeaksCount, P.CompressionInfoId, S.NativeId FROM Peaks P JOIN Spectra S ON (P.Id = S.PeaksId)"); eee;
        e = QEXEC_NOARG(querySmallBySpec); eee;

        transaction_begin(m_byspecDB);

        while(querySmallBySpec.next()) {
            //Update Main byspec file
            e = QPREPARE(queryMainBySpec, "\
                         UPDATE Peaks SET PeaksMz=?, PeaksIntensity=?, PeaksCount=?, CompressionInfoId=? \
                         WHERE Id = (SELECT PeaksId FROM Spectra WHERE NativeId = ?)"); eee;
            queryMainBySpec.bindValue(0, querySmallBySpec.value(0));
            queryMainBySpec.bindValue(1, querySmallBySpec.value(1));
            queryMainBySpec.bindValue(2, querySmallBySpec.value(2));
            queryMainBySpec.bindValue(3, querySmallBySpec.value(3));
            queryMainBySpec.bindValue(4, querySmallBySpec.value(4));
            e = QEXEC_NOARG(queryMainBySpec); eee;

            if (progress) progress->refreshUI();
        }
        transaction_end(m_byspecDB);
    } //db


error:
    if (database_added) {
        QSqlDatabase::removeDatabase("extract_scans");
    }

    if (!QFile::exists("C:\\pmi_keep_byspec.txt")) {
        QFile::remove(outputByspecName);
        QFile::remove(outputByspecName+"_config.txt");
    }
    return e;
}

//call msconvert-pmi, and merge into existing database
Err MSReaderByspec::extractScanNumberAndMergeIntoByspec(qlonglong scanNumber,
                                                      QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    if (scanNumber <= 0) {
        debugMs() << "Illegal value in extractScanNumberAndMergeIntoByspec with scanNumber = " << scanNumber;
        return e;
    }
    QList<int> uniqueScanNumberList;
    QList<point2dList> pointsList;
    e = makeUniqueAndNotCachedScanNumbers(QList<int>() << scanNumber, uniqueScanNumberList); eee;
    e = extractScanNumbersToByspec(uniqueScanNumberList, pointsList, progress); eee;

error:
    return e;
}

Err MSReaderByspec::makeUniqueAndNotCachedScanNumbers(const QList<int> &scanNumberList,
                                                    QList<int> &outScanNumberList)
{
    Err e = kNoErr;

    //note: copy first just in case the input two are the same list
    QList<int> uniqueScanNumberList = scanNumberList;
    outScanNumberList.clear();

    //Remove duplicated and invalid scan numbers
    RemoveDuplicatesSlow<int>(uniqueScanNumberList,true);

    //Make sure there aren't any negative scan numbers
    QList<int> tmp;
    foreach(int scanNumber, uniqueScanNumberList) {
        if (scanNumber <= 0) {
            debugMs() << "Illegal value in cacheScanNumbers with scanNumber = " << scanNumber;
            continue;
        }
        tmp << scanNumber;
    }
    uniqueScanNumberList.swap(tmp);

    QSqlQuery q;
    QString sql;
    const CompressionInfo* compressionInfo = nullptr;
    int compressionInfoId = -1;

    q = makeQuery(m_byspecDB, true);

    sql = m_containsCompression ?
                "SELECT PeaksMz,PeaksIntensity,CompressionInfoId FROM Peaks k JOIN Spectra s ON s.PeaksId = k.Id WHERE s.ScanNumber = ?" :
                "SELECT PeaksMz,PeaksIntensity FROM Peaks k JOIN Spectra s ON s.PeaksId = k.Id WHERE s.ScanNumber = ?";

    e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS idx_SpectraScanNumber ON Spectra(ScanNumber)"); eee;
    e = QPREPARE(q, sql); eee;
    //Now check if scan number has been cached or not.
    foreach(int scanNumber, uniqueScanNumberList) {
        q.bindValue(0, scanNumber);
        e = QEXEC_NOARG(q); eee;
        if (q.next()) {
            QVariant peakmz = q.value(0);
            QVariant peakint = q.value(1);

            bool foundValidCompression = false;

            if(m_containsCompression) {
                if(!q.value(2).isNull()) {
                    compressionInfoId = q.value(2).toInt();
                    compressionInfo = m_compressionInfoHolder.findById(compressionInfoId);

                    if(compressionInfo != nullptr) {
                        foundValidCompression = true;
                    }
                }
            }

            if (peakmz.isNull() || (!foundValidCompression && peakint.isNull())) {
                outScanNumberList << scanNumber;
            }
        } else {
            debugMs() << "Warning, could not find scanNumber" << scanNumber << " byspec of " << m_byspecDB->databaseName() << ", implied file: " << getFilename() ;
        }
    }

error:
    return e;
}

Err MSReaderByspec::cacheScanNumbers(const QList<int> &scanNumberList, QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    QList<point2dList> pointsList;
    QList<int> uniqueScanNumberList;

    debugMs() << "scanNumberList to extract:\n" << scanNumberList;
    e = makeUniqueAndNotCachedScanNumbers(scanNumberList, uniqueScanNumberList); eee;
    debugMs() << "uniqueScanNumberList to extract:\n" << uniqueScanNumberList;

    e = extractScanNumbersToByspec(uniqueScanNumberList, pointsList, progress); eee;


error:
    return e;
}

//Note(2013): scanNumber refers to either Thermo RAW's scan number, or in other situations,
//the Spectra.Id.  This is a temporary patch to handle various instrustment types without
//breaking the current API
//Later, we need a good way to extract these information, and to handle cases when the
//input spectra does not start from byspec2 originating from a single file.
//Assuming we are dealing with one file converted into byspec2 file.
//Note(2014-10-29):  We will now use ScanNumber column.  We assume this number has been extracted properly and is sequential starting with 1.
const QString cmd_peaks_spectra_scannumber_template =
        "SELECT k.PeaksMz, k.PeaksIntensity, s.MetaText \
        FROM Peaks k \
        JOIN Spectra s ON s.PeaksId = k.Id \
        WHERE s.ScanNumber = %1;";

const QString cmd_peaks_compressed_spectra_scannumber_template =
        "SELECT k.PeaksMz, k.PeaksIntensity, s.MetaText, k.CompressionInfoId, s.NativeId \
        FROM Peaks k \
        JOIN Spectra s ON s.PeaksId = k.Id \
        WHERE s.ScanNumber = %1;";

Err MSReaderByspec::patchPeaksBlob(long scanNumber, QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    QSqlQuery q;
    QString cmd;
    const CompressionInfo* compressionInfo = nullptr;
    int compressionInfoId = -1;
    int do_update_patch  = 0;
    QString cmd_peaks_spectra_scannumber_template =
        "SELECT k.PeaksMz, k.PeaksIntensity, s.MetaText \
        FROM Peaks k \
        JOIN Spectra s ON s.PeaksId = k.Id \
        WHERE s.ScanNumber = %1;";

    QString cmd_peaks_compressed_spectra_scannumber_template =
        "SELECT k.PeaksMz, k.PeaksIntensity, s.MetaText, k.CompressionInfoId \
        FROM Peaks k \
        JOIN Spectra s ON s.PeaksId = k.Id \
        WHERE s.ScanNumber = %1;";

    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; eee;
    }

    q = makeQuery(m_byspecDB, true);

    e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS idx_SpectraPeaksId ON Spectra(PeaksId)"); eee;
    cmd = m_containsCompression ?
            cmd_peaks_compressed_spectra_scannumber_template.arg(scanNumber) :
            cmd_peaks_spectra_scannumber_template.arg(scanNumber);
    e = QEXEC_CMD(q,cmd); eee;

    if (q.next()) {
        QVariant peakmz = q.value(0);
        QVariant peakint = q.value(1);

        bool foundValidCompression = false;

        if(m_containsCompression) {
            if(!q.value(2).isNull()) {
                compressionInfoId = q.value(2).toInt();
                compressionInfo = m_compressionInfoHolder.findById(compressionInfoId);

                if(compressionInfo != nullptr) {
                    foundValidCompression = true;
                }
            }
        }

        if (peakmz.isNull() || (!foundValidCompression && peakint.isNull())) {
            do_update_patch = 1;
        }
    } else {
        //We assume Peaks table exists, but the blob is just null.  So, we should not be here.
        debugMs() << "Could not execute command:" << cmd;
        debugMs() << "file=" << getFilename();
        e = kBadParameterError; eee;
    }

    if (do_update_patch ) {
        e = extractScanNumberAndMergeIntoByspec(scanNumber, progress); eee;
    }

error:
    return e;
}

Err MSReaderByspec::getScanData(long scanNumber, point2dList *points, bool do_centroiding,
                              PointListAsByteArrays *pointListAsByteArrays)
{
    Q_ASSERT(points);

    Err e = kNoErr;
    points->clear();
    if (scanNumber <= 0) {
        return e;
    }

    QSqlQuery q;
    QString cmd;
    pico::PicoLocalDecompressQtUse pico;
    const CompressionInfo* compressionInfo = nullptr;
    int compressionInfoId = -1;
    QByteArray compressedMzByteArray;
    QByteArray compressedIntensityByteArray;
    double* restoredMz = nullptr;
    float* restoredIntensity = nullptr;
    int uncompressedSize = 0;
    QByteArray mzValues;
    QByteArray intensityValues;
    ScanInfo scanInfo;

    bool extract_from_Peaks_MS1Centroided
        = do_centroiding && m_containsNonEmptyPeaksMS1CentroidedTable;
    bool containsPertinentCompression = (m_containsCompression && !extract_from_Peaks_MS1Centroided)
        || (m_containsCentroidedCompression && extract_from_Peaks_MS1Centroided);

    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; eee;
    }

    q = makeQuery(m_byspecDB, true);
    e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS idx_SpectraPeaksId ON Spectra(PeaksId)"); eee;

    //The Peaks_MS1Centroided only contains MS1 information.  So, if it's ms2, don't use this table.
    if (extract_from_Peaks_MS1Centroided) {
        e = getScanInfo(scanNumber, &scanInfo); eee;
        if (scanInfo.scanLevel != 1) {
            extract_from_Peaks_MS1Centroided = false;
        }
    }

    if (extract_from_Peaks_MS1Centroided) {
        //Try and extract from
        QString cmd_peaks_centroid_spectra_scannumber_template;
        if(m_containsCentroidedCompression) {
            cmd_peaks_centroid_spectra_scannumber_template =
                    "SELECT k.PeaksMz, k.PeaksIntensity, s.MetaText, k.CompressionInfoId \
                    FROM Peaks_MS1Centroided k \
                    JOIN Spectra s ON s.PeaksId = k.Id \
                    WHERE s.ScanNumber = %1";
        } else {
            cmd_peaks_centroid_spectra_scannumber_template =
                    "SELECT k.PeaksMz, k.PeaksIntensity, s.MetaText \
                    FROM Peaks_MS1Centroided k \
                    JOIN Spectra s ON s.PeaksId = k.Id \
                    WHERE s.ScanNumber = %1";
        }
        cmd = cmd_peaks_centroid_spectra_scannumber_template.arg(scanNumber);
    } else {
        //Calling this causes issue with files with just centroid data.
        //e = patchPeaksBlob(scanNumber); eee;


        //Note: scanNumber refers to either Thermo RAW's scan number, or in other situations,
        //the Spectra.Id.  This is a temporary patch to handle various instrustment types without
        //breaking the current API
        //Later, we need a good way to extract these information, and to handle cases when the
        //input spectra does not start from byspec2 originating from a single file.
        //Assuming we are dealing with one file converted into byspec2 file.
        cmd = m_containsCompression ?
                cmd_peaks_compressed_spectra_scannumber_template.arg(scanNumber) :
                cmd_peaks_spectra_scannumber_template.arg(scanNumber);
    }

    e = QEXEC_CMD(q,cmd); eee;
    if (q.next()) {
        bool foundValidCompression = false;
        bool sortByX = false;
        sortByX = true; //We must sort because Waters can contain odd values
        /*
        99.973758053705851    0.00000000000000000
        99.971036583582006    17.000000000000000  <<--- x out of order
        99.968315113458161    0.00000000000000000
        99.975801950469204    0.00000000000000000
        99.973080646201154    12.000000000000000
        99.970359341933104    0.00000000000000000
        99.977845847259488    0.00000000000000000
        */

        QString metaText;
        PeakPickingMode peakmode;

        if(containsPertinentCompression) {
            // If the joined CompressionInfoId is NULL, then that means this particular row of Peaks is not compressed
            if(!q.value(3).isNull() && q.value(3).isValid()) {
                compressionInfoId = q.value(3).toInt();
                compressionInfo = m_compressionInfoHolder.findById(compressionInfoId);

                if(compressionInfo != nullptr) {
                    foundValidCompression = true;

                    compressedMzByteArray = q.value(0).toByteArray();
                    compressedIntensityByteArray = q.value(1).toByteArray();

                    QString nativeId = q.value(4).toString();
                    // NativeId
                    QString functionNumber = pico::getFunctionNumberFromNativeId(nativeId);
                    pico::WatersCalibration::Coefficents coef;
                    pico::getCoefficientInformation(functionNumber, *m_byspecDB, &coef);

                    bool val = pico.decompress(coef, compressionInfo->GetProperty(), compressionInfo->GetVersion(), compressedMzByteArray, compressedIntensityByteArray, &restoredMz, &restoredIntensity, &uncompressedSize);

                    if (restoredMz == nullptr || restoredIntensity == nullptr) {
                        if (uncompressedSize == 0) {
                            //Nothing to output or free. return empty points.
                            return e;
                        }
                        warningMs() << "sscanNumber,val =" << scanNumber << "," << val;
                        warningMs() << "compressedMzByteArray,compressedIntensityByteArray sizes=" << compressedMzByteArray << "," << compressedIntensityByteArray.size();
                        // Should always be data, so throw error
                        e = kMemoryError; eee;
                    }

                    //mzValues = QByteArray(reinterpret_cast<const char*>(restoredMz), uncompressedSize * 8);
                    //intensityValues = QByteArray(reinterpret_cast<const char*>(restoredIntensity), uncompressedSize * 4);
                    toByteArrayReference_careful(reinterpret_cast<const char*>(restoredMz), uncompressedSize * 8, mzValues);
                    toByteArrayReference_careful(reinterpret_cast<const char*>(restoredIntensity), uncompressedSize * 4, intensityValues);

                    if (pointListAsByteArrays != nullptr) {
                        pointListAsByteArrays->dataX = compressedMzByteArray;
                        pointListAsByteArrays->dataY = compressedIntensityByteArray;
                        pointListAsByteArrays->compressionInfoId = compressionInfoId;
                        pointListAsByteArrays->compressionInfoHolder = &m_compressionInfoHolder;
                    }
                }
            }
        }

        // If no valid compression is found (or compression does not exist), use the data as normal
        if(!foundValidCompression) {
            mzValues = q.value(0).toByteArray();
            intensityValues = q.value(1).toByteArray();
            sortByX = true;

            if (pointListAsByteArrays != nullptr) {
                pointListAsByteArrays->dataX = mzValues;
                pointListAsByteArrays->dataY = intensityValues;
            }
        }

        metaText = q.value(2).toString();
        peakmode = MetaTextParser::getPeakPickingMode(metaText);
        if (do_centroiding && peakmode == PeakPickingProfile) {
            if (!extract_from_Peaks_MS1Centroided) {
                PlotBase plot;
                point2dList & tmppoints = plot.getPointList();
                e = byteArrayToPlotBase(mzValues, intensityValues, tmppoints, sortByX); eee;
                //plot.sortPointListByX();
                plot.makeCentroidedPoints(points);
            } else {
                e = byteArrayToPlotBase(mzValues, intensityValues, *points, sortByX); eee;
            }
        } else {
            e = byteArrayToPlotBase(mzValues, intensityValues, *points, sortByX); eee;
        }
    } else {
        debugMs() << "Could not execute command:" << cmd;
        debugMs() << "file=" << getFilename();
        // WARNING, KELSON, DO NOT SKIP e=kBadParameterError, UNCOMMENT
        // e = kBadParameterError; eee;
    }

error:
    // Deallocate any data returned via decompression
    safe_free(restoredMz);
    safe_free(restoredIntensity);
    return e;
}

Err MSReaderByspec::getXICData(const XICWindow &win, point2dList *xic_points, int ms_level) const
{
    Q_UNUSED(win);
    Q_UNUSED(xic_points);
    Q_UNUSED(ms_level);

    return kFunctionNotImplemented;
}

const QString getScanInfoStrTemplate = "SELECT RetentionTime, MSLevel, NativeId, MetaText, Id AS SpectraId FROM Spectra s [where]";

Err MSReaderByspec::getScanInfo(long scanNumber, ScanInfo *obj) const
{
    Q_ASSERT(obj);

    Err e = kNoErr;
    QSqlQuery q;
    QString metaStr;
    QString scanInfoStr;
    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; eee;
    }
    q = makeQuery(m_byspecDB, true);
    scanInfoStr = QStringLiteral("SELECT RetentionTime, MSLevel, NativeId, MetaText, Id AS SpectraId [MobilityValue] FROM Spectra s %1").arg("WHERE ScanNumber = %1");
    if (m_containsSpectraMobilityValue) {
        scanInfoStr = scanInfoStr.replace("[MobilityValue]", ", MobilityValue");
    } else {
        scanInfoStr = scanInfoStr.replace("[MobilityValue]", "");
    }

    e = QEXEC_CMD(q, scanInfoStr.arg(scanNumber)); eee;
    if (q.next()) {
        obj->retTimeMinutes = q.value(0).toDouble()/60.0;
        obj->scanLevel = q.value(1).toInt();
        obj->nativeId = q.value(2).toString();
        metaStr = q.value(3).toString();
        obj->peakMode = MetaTextParser::getPeakPickingMode(metaStr);
        obj->scanMethod = MetaTextParser::getScanMethod(metaStr);

        obj->mobility = MobilityData();
        if (m_containsSpectraMobilityValue) {
            const int MOBILITY_DATA_INDEX = 5;

            bool ok = false;
            const double val = q.value(MOBILITY_DATA_INDEX).toDouble(&ok);
            if (ok) {
                obj->mobility.setMobilityValue(val);
            }
        }
    } else {
        e = kError; eee;
    }

error:
    return e;
}

Err MSReaderByspec::getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const
{
    Q_ASSERT(pinfo);

    Err e = kNoErr;
    QSqlQuery q;
    QString get_scan_number_template = "\
    SELECT ScanNumber, NativeId \
    FROM Spectra s \
    WHERE s.RetentionTime < ? AND MSLevel = 1 \
    ORDER BY s.RetentionTime DESC;\
    ";

    QString get_scan_number_template_no_compare = "\
    SELECT ScanNumber, NativeId \
    FROM Spectra s \
    WHERE MSLevel = 1 \
    ORDER BY s.RetentionTime ASC;\
    ";

    double ret_time = 0;
    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; eee;
    }
    q = makeQuery(m_byspecDB, true);

    e = QEXEC_CMD(q, QString("SELECT Id as SpectraId, ObservedMz, ChargeList\
                             ,ParentScanNumber, ParentNativeId, RetentionTime \
                             ,IsolationWindowLowerOffset, IsolationWindowUpperOffset \
                             FROM Spectra WHERE ScanNumber = %1;").arg(scanNumber)); eee;
    ret_time = 0;
    if (q.next()) {
        pinfo->dIsolationMass = q.value(1).toDouble();
        pinfo->dMonoIsoMass = q.value(1).toDouble(); //TODO later
        pinfo->nChargeState = q.value(2).toInt();
        pinfo->nScanNumber = q.value(3).toInt();
        pinfo->nativeId = q.value(4).toString();
        ret_time = q.value(5).toDouble();
        pinfo->lowerWindowOffset = q.value(6).toDouble();
        pinfo->upperWindowOffset = q.value(7).toDouble();
    } else {
        e = kError; eee;
    }

    //Note: this will work for now, but needs to handle more general cases.
    //Assume the parent scan is always the MS1 scan previous to this current MS2 scan.
    if (pinfo->nScanNumber <= 0) {
        e = QPREPARE(q, get_scan_number_template); eee;
        q.bindValue(0, ret_time);
        e = QEXEC_NOARG(q); eee;
        if (q.next()) {
            pinfo->nScanNumber = q.value(0).toInt();
            pinfo->nativeId = q.value(1).toString();
        } else {
            //It's possible that the given scan is triggered without an MS1 scan,
            //such as waters-e with lock mass scan (function=3).  In this case,
            //let's assign it to the closest MS1 scan.
            warningMs() << "Could not find precursor scan number for scanNumber:" << scanNumber;
            warningMs() << "Attempting to find the first scan.";
            e = QEXEC_CMD(q, get_scan_number_template_no_compare); eee;
            if (q.next()) {
                pinfo->nScanNumber = q.value(0).toInt();
                pinfo->nativeId = q.value(1).toString();
            } else {
                warningMs() << "Could not find precursor scan number for scanNumber:" << scanNumber;
                e = kError; eee;
            }
        }
    }

error:
    return e;
}

Err MSReaderByspec::getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const
{
    Err e = kNoErr;
    QSqlQuery q;
    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; eee;
    }
    q = makeQuery(m_byspecDB, true);
    e = QEXEC_CMD(q, "SELECT MAX(ScanNumber), MIN(ScanNumber) FROM Spectra"); eee;
    if (q.next()) {
        *totalNumber = q.value(0).toInt();
        *startScan = q.value(1).toInt(); //assume ScanNumber starts with one.
        if (*startScan != 1) {
            debugMs() << "warning, scan number does not start with one, but starts with:" << *startScan;
        }
        *endScan = *totalNumber;
    } else {
        e = kError; eee;
    }

error:
    return e;
}

Err MSReaderByspec::getFragmentType(long scanNumber, long scanLevel, QString *fragmentType) const
{
    Q_ASSERT(fragmentType);

    Err e = kNoErr;
    Q_UNUSED(scanLevel);

    fragmentType->clear();
    QSqlQuery q;
    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; eee;
    }
    q = makeQuery(m_byspecDB, true);
    //TODO: consider using scanLevel in the future; make sure that it's robust
    e = QEXEC_CMD(q, QString("SELECT FragmentationType FROM Spectra WHERE ScanNumber = %1").arg(scanNumber)); eee;
    if (q.next()) {
        *fragmentType = q.value(0).toString();
    } else {
        e = kError; eee;
    }

error:
    return e;
}

Err MSReaderByspec::getChromatograms(QList<msreader::ChromatogramInfo> *chroList,
                                   const QString &channelInternalName)
{
    Q_ASSERT(chroList);

    Err e = kNoErr;
    Q_UNUSED(channelInternalName);
    ChromatogramInfo chroinfo;
    chroList->clear();

    QSqlQuery q;
    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; eee;
    }
    q = makeQuery(m_byspecDB, true);

    e = QEXEC_CMD(q, "SELECT DataX,DataY,DataCount,MetaText,Identifier FROM Chromatogram WHERE ChromatogramType='absorption chromatogram' OR Identifier='UV'"); eee;
    while (q.next()) {
        chroinfo.clear();

        QString metaText = q.value(3).toString();
        QString identStr = q.value(4).toString();
        chroinfo.channelName = identStr;
        chroinfo.internalChannelName = identStr;

        if (!channelInternalName.isEmpty()) {
            if (channelInternalName != identStr) {
                debugMs() << "Ignoring UV trace as the ident does not match. channelInternalName,identStr=" << channelInternalName << "," << identStr;
                continue;
            }
        }

        e = byteArrayToPlotBase(q.value(0).toByteArray(), q.value(1).toByteArray(), chroinfo.points, true); ree;
        //convert to minutes if in seconds.
        if (metaText.contains("second")) {
            for (point2d & p : chroinfo.points) {
                p.rx() = p.x() / 60.0;
            }
        }
        chroList->push_back(chroinfo);
    }

error:
    return e;
}

QSqlDatabase *MSReaderByspec::getDatabasePointer()
{
    return m_byspecDB;
}

Err MSReaderByspec::getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const
{
    Err e = kNoErr;
    double scanTimeSec = scanTimeMinutes * 60;

    const QString cmd_get_parent_template = " \
    SELECT s.ScanNumber, s.RetentionTime \
    FROM Spectra s \
    WHERE MSLevel=%1 \
    AND RetentionTime <= (%2+%3) \
    ORDER BY s.RetentionTime DESC; \
    ";
    // add '0.00000001' just in case there were some rounding errors.  This should be small enough
    // that no two scan triggers would  occur within this time window.
    const QString cmd_alt = " \
    SELECT s.ScanNumber, s.RetentionTime \
    FROM Spectra s \
    WHERE MSLevel=%1 \
    ORDER BY s.RetentionTime ASC; \
    ";
    QString cmd_get_parent = cmd_get_parent_template.arg(msLevel)
                                 .arg(scanTimeSec)
                                 .arg(MSReaderBase::scantimeFindParentFudge);
    // commonMsDebug() << "cmd_get_parent=" << cmd_get_parent;

    QSqlQuery q;
    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; eee;
    }
    q = makeQuery(m_byspecDB, true);

    e = QEXEC_CMD(q,
                  "CREATE INDEX if not exists idx_Spectra_RetentionTime ON Spectra(RetentionTime)"); eee;
    e = QEXEC_CMD(q,
                  "CREATE INDEX if not exists idx_Spectra_MSLevelRetentionTime ON "
                  "Spectra(MSLevel,RetentionTime)"); eee;

    e = QEXEC_CMD(q, cmd_get_parent); eee;
    if (q.next()) {
        *scanNumber = q.value(0).toInt();
    } else {
        e = QEXEC_CMD(q, cmd_alt.arg(msLevel)); eee;
        if (q.next()) {
            *scanNumber = q.value(0).toInt();
        } else {
            debugMs() << "Could not find scanNumber for scanTime=" << scanTimeMinutes << "("
                      << scanTimeSec << " sec)"
                      << " mslevel=" << msLevel;
            e = kBadParameterError; eee;
        }
    }

error:
    return e;
}

Err MSReaderByspec::getLockmassScans(QList<ScanInfoWrapper> *list) const
{
    Err e = kNoErr;
    const QString lockmass_scans_filter_default
        = "s.MetaText LIKE '%<Calibration>dynamic</Calibration>%'";
    QString filterString = lockmass_scans_filter_default;
    QString cmd;
    int count;

    if (!m_byspecDB->isOpen()) {
        e = kFileOpenError; ree;
    }

    QSqlDatabase &db = *m_byspecDB;
    QSqlQuery q = makeQuery(db, true);
    QString select_cmd
        = QString("SELECT COUNT(Id) FROM Spectra s WHERE %1").arg(lockmass_scans_filter_default);
    e = QEXEC_CMD(q, select_cmd); eee;
    if (!q.next()) {
        e = kBadParameterError; ree;
    }
    count = q.value(0).toInt();
    //If no calibration file, it must be not Waters (or none to begin with).
    //Let's try and search for calibration on MS1 scans (e.g. Bruker)
    if (count <= 0) {
        filterString = " s.MSLevel = 1";
    }

    cmd = QString(getScanInfoStrTemplate).replace("[where]", "WHERE " + filterString);

    e = QEXEC_CMD(q, cmd); eee;
    while (q.next()) {
        msreader::ScanInfoWrapper lockmassObj;
        ScanInfo &obj = lockmassObj.scanInfo;
        obj.retTimeMinutes = q.value(0).toDouble();
        obj.scanLevel = q.value(1).toInt();
        obj.nativeId = q.value(2).toString();
        QString metaStr = q.value(3).toString();
        if (metaStr.contains("minute", Qt::CaseInsensitive)) {
        } else {
            // assume it's in seconds if nothing is found (or in second)
            obj.retTimeMinutes /= 60.0;
        }

        obj.peakMode = MetaTextParser::getPeakPickingMode(metaStr);
        obj.scanMethod = MetaTextParser::getScanMethod(metaStr);
        lockmassObj.scanNumber = q.value(4).toInt();

        list->push_back(lockmassObj);
    }

error:
    return e;
}


MSReaderBase::FileInfoList MSReaderByspec::filesInfoList() const
{
    FileInfoList list;
    // If waters file, return the cal info
    if (this->classTypeName() == MSReaderClassTypeByspec) {
        QSqlQuery q = makeQuery(m_byspecDB, true);
        Err e = QEXEC_CMD(q, "SELECT Key, Value FROM FilesInfo WHERE Key LIKE 'Cal Modification%'"); nee;
        if (e == kNoErr) {
            // for question marks q.bindValue()
            QPair<QString, QString> fileInfo;
            while (q.next()) {
                fileInfo.first = q.value(0).toString();
                fileInfo.second = q.value(1).toString();
                list.push_back(fileInfo);
            }
        } else {
            warningMs() << "QPREPARE was not successful";
        }
    }
    return list;
}

Err computeAndApplyLockMassCalibration(QString byspecFilename,
                                       const CacheFileManagerInterface &cacheFileManager,
                                       QString lockmassListWithAliasName, QString ppmTolerance)
{
    Err e = kNoErr;
    MzCalibration mzCal(cacheFileManager);
    MzCalibrationOptions options;
    options.lock_mass_list = lockmassListWithAliasName.toStdString();
    options.lock_mass_ppm_tolerance = ppmTolerance.toStdString();


    e = mzCal.computeCoefficientsFromByspec(byspecFilename); eee;
    e = mzCal.writeToDB(byspecFilename); eee;
    e = applyCoefficientsToByspec(byspecFilename, cacheFileManager, 1); eee;

error:
    return e;
}

_MSREADER_END
_PMI_END
