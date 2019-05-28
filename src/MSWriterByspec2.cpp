/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 *  Author  : Victor Suponev vsuponev@milosolutions.com
 */

#include "MSWriterByspec2.h"

#include "MSReader.h"

#include "sqlite_utils.h"

#include <QDir>

_PMI_BEGIN

using namespace msreader;

static const QString VERSION_KEY = QStringLiteral("Version");
static const QString VERSION_VALUE = QStringLiteral("6.3");
static const QString TABLE_PEAKS = QStringLiteral("Peaks");
static const QString TABLE_PEAKS_MS1CENTROIDED = QStringLiteral("Peaks_MS1Centroided");

QAtomicInt MSWriterByspec2::connCounter = 0;

void calculatePeakIntensitySum(byspec2::PeakDaoEntry &peak) {
    peak.intensitySum = std::accumulate(peak.peaksIntensity.begin(), peak.peaksIntensity.end(), 0.0);
}

byspec2::PeakDaoEntry makeCentroided(const byspec2::PeakDaoEntry &peak, const pico::CentroidOptions &centroidOptions) {
    byspec2::PeakDaoEntry centroidedPeak;
    pico::Centroid centroid;
    std::vector<std::pair<double, double>> dataIn;
    std::vector<std::pair<double, double>> dataOut;

    // Prepare data for centroiding
    dataIn.reserve(static_cast<unsigned int>(peak.peaksCount));
    for (int i = 0; i < peak.peaksCount; ++i) {
        dataIn.push_back(
            std::make_pair(peak.peaksMz[i], static_cast<double>(peak.peaksIntensity[i])));
    }
    centroid.smooth_and_centroid(dataIn, centroidOptions, dataOut);
    // Convert data from the centroided format
    const size_t dataCount = dataOut.size();
    centroidedPeak.peaksMz.reserve(static_cast<int>(dataCount));
    centroidedPeak.peaksIntensity.reserve(static_cast<int>(dataCount));
    for (size_t i = 0; i < dataCount; ++i) {
        centroidedPeak.peaksMz.push_back(dataOut[i].first);
        centroidedPeak.peaksIntensity.push_back(static_cast<float>(dataOut[i].second));
    }
    // Populate all fields
    centroidedPeak.id = peak.id;
    centroidedPeak.spectraIdList = peak.spectraIdList;
    centroidedPeak.comment = peak.comment;
    centroidedPeak.metaText = peak.metaText;
    centroidedPeak.compressionInfoId = peak.compressionInfoId;
    centroidedPeak.peaksCount = static_cast<int>(dataCount);
    calculatePeakIntensitySum(centroidedPeak);

    return centroidedPeak;
}

MSWriterByspec2::MSWriterByspec2(const QString &outputFile)
    : m_database(QSqlDatabase::addDatabase(kQSQLITE, QStringLiteral("MakeByspec2-%1").arg(++connCounter)))
    , m_outputFile(outputFile)
{
    if(!outputFile.isEmpty()) {
        Err e = openDatabase(outputFile); nee;
    }
}

MSWriterByspec2::~MSWriterByspec2()
{
    if (m_database.open()) {
        m_database.close();
    }
}

// To be used on stack only
class MsReaderRaiiHelper final
{
public:
    MsReaderRaiiHelper(MSReader* reader)
        : m_reader(reader)
    {
        if (reader != nullptr) {
            m_routedVendors = reader->vendorsRoutedThroughByspec();
            reader->clearVendorsRoutedThroughByspec();
        }
    }

    ~MsReaderRaiiHelper()
    {
        if (m_reader != nullptr) {
            m_reader->setVendorsRoutedThroughByspec(m_routedVendors);
            m_reader->closeFile();

            m_reader = nullptr;
        }
    }
private:
    QSet<MSReaderBase::MSReaderClassType> m_routedVendors;
    MSReader* m_reader = nullptr;
};

Err MSWriterByspec2::makeByspec2(const QString &sourceFile, const QString &outputFile,
                                 const MSConvertOption &options, bool onlyCentroided,
                                 const msreader::IonMobilityOptions &ionMobilityOptions,
                                 const QStringList &argumentList)
{
    Q_UNUSED(options)
    MSReader *reader = MSReader::Instance();

    // Make sure this calls MSReaderBrukerTims instead of MSReaderBypsec
    // Otherwise, it will lead MSReaderBypsec2 to call PMI-MSConvert.exe,
    // which then calls this function and leading to a recursion.
    //
    // This also properly closes reader
    const MsReaderRaiiHelper raiiHelper(reader);

    reader->setIonMobilityOptions(ionMobilityOptions);

    Err e = reader->openFile(sourceFile); ree;

    // it is a temporary solution until byspec3 is implemented
    // see https://proteinmetrics.atlassian.net/browse/LT-4173?focusedCommentId=83352&page=com.atlassian.jira.plugin.system.issuetabpanels:comment-tabpanel#comment-83352
    if (reader->isBrukerTims()) {
        onlyCentroided = true;
    }

    const QString outputDirectory = QFileInfo(outputFile).absolutePath();

    if (!QDir(outputDirectory).exists()) {
        QDir().mkpath(outputDirectory);
    }

    m_database.setDatabaseName(outputFile);

    if (!m_database.open()) {
        const QString errMsg = QStringLiteral("Failed to open database:\n%1").arg(outputFile);
        qWarning() << errMsg;
        return kFileOpenError;
    }

    if (!QFileInfo(outputFile).isWritable()) {
        const QString errMsg = QStringLiteral("Cannot write to file:\n%1").arg(outputFile);
        qWarning() << errMsg;
        return kFileOpenError;
    }

    e = createByspec2Tables(); ree;

    QMap<QString, QString> filesInfoData;
    appendArgumentsToMap(argumentList, filesInfoData);
    appendIonMobilityOptionsToMap(ionMobilityOptions, filesInfoData);
    filesInfoData[VERSION_KEY] = VERSION_VALUE;

    int fileId = 0;
    e = insertDataIntoFilesTable(sourceFile, &fileId); ree;
    e = insertDataIntoFileInfoTable(filesInfoData, fileId); ree;
    e = insertDataIntoPeaksAndSpectraTable(onlyCentroided, fileId); ree;

    return e;
}

void MSWriterByspec2::appendArgumentsToMap(const QStringList &arguments,
                                           QMap<QString, QString> &destination)
{
    static const QString argvTemplate = QStringLiteral("argv_%1");
    for (int index = 0; index < arguments.size(); ++index) {
        destination[argvTemplate.arg(index)] = arguments[index];
    }
}

void MSWriterByspec2::appendIonMobilityOptionsToMap(const msreader::IonMobilityOptions &ionMobilityOptions,
    QMap<QString, QString> &destination)
{
    static const QString SUMMING_TOLERANCE_KEY = "IonMobilityOptions:summingTolerance";
    static const QString MERGING_TOLERANCE_KEY = "IonMobilityOptions:mergingTolerance";
    static const QString SUMMING_UNIT_KEY = "IonMobilityOptions:summingUnit";
    static const QString MERGING_UNIT_KEY = "IonMobilityOptions:mergingUnit";
    static const QString RETAIN_NUMBER_OF_PEAKS_KEY = "IonMobilityOptions:retainNumberOfPeaks";

    destination[SUMMING_TOLERANCE_KEY] = QString::number(ionMobilityOptions.summingTolerance);
    destination[MERGING_TOLERANCE_KEY] = QString::number(ionMobilityOptions.mergingTolerance);
    destination[SUMMING_UNIT_KEY] = (ionMobilityOptions.summingUnit == 0 ? "PPM" : "Dalton");
    destination[MERGING_UNIT_KEY] = (ionMobilityOptions.mergingUnit == 0 ? "PPM" : "Dalton");
    destination[RETAIN_NUMBER_OF_PEAKS_KEY] = QString::number(ionMobilityOptions.retainNumberOfPeaks);
}

Err MSWriterByspec2::createByspec2Tables()
{
    if (!m_database.isValid()) {
        return kNoErr;
    }

    Err e = kNoErr;
    QSqlQuery query= makeQuery(m_database, true);

    e = QEXEC_CMD(query, "DROP TABLE IF EXISTS Files;"); ree;
    QString command("CREATE TABLE IF NOT EXISTS Files(Id INTEGER PRIMARY KEY, \
                        Filename TEXT, Location TEXT,Type TEXT,Signature TEXT);");
    e = QEXEC_CMD(query, command); ree;

    e = QEXEC_CMD(query, "DROP TABLE IF EXISTS FilesInfo;"); ree;
    command = "CREATE TABLE IF NOT EXISTS FilesInfo(Id INTEGER PRIMARY KEY, \
                FilesId INT, Key TEXT,Value TEXT, \
                FOREIGN KEY(FilesId) REFERENCES Files(Id));";
    e = QEXEC_CMD(query, command); ree;

    e = QEXEC_CMD(query, "DROP TABLE IF EXISTS Info;"); ree;
    command = "CREATE TABLE IF NOT EXISTS Info(Id INTEGER PRIMARY KEY, Key TEXT, \
                Value TEXT);";
    e = QEXEC_CMD(query, command); ree;

    e = QEXEC_CMD(query, "DROP TABLE IF EXISTS Peaks;"); ree;
    command = "CREATE TABLE IF NOT EXISTS Peaks(Id INTEGER PRIMARY KEY, PeaksMz BLOB, \
            PeaksIntensity BLOB, SpectraIdList TEXT, PeaksCount INT, MetaText TEXT, \
            Comment TEXT, IntensitySum REAL, CompressionInfoId INT);";
    e = QEXEC_CMD(query, command); ree;

    e = QEXEC_CMD(query, "DROP TABLE IF EXISTS Peaks_MS1Centroided;"); ree;
    command = "CREATE TABLE IF NOT EXISTS Peaks_MS1Centroided(Id INTEGER PRIMARY KEY, PeaksMz BLOB, \
            PeaksIntensity BLOB, SpectraIdList TEXT, PeaksCount INT, MetaText TEXT, \
            Comment TEXT, IntensitySum REAL, CompressionInfoId INT);";
    e = QEXEC_CMD(query, command); ree;

    e = QEXEC_CMD(query, "DROP TABLE IF EXISTS Spectra;"); ree;
    command = "CREATE TABLE IF NOT EXISTS Spectra(Id INTEGER PRIMARY KEY, FilesId INT, \
                MSLevel INT, ObservedMz REAL, IsolationWindowLowerOffset REAL, \
                IsolationWindowUpperOffset REAL,RetentionTime REAL, ScanNumber INT, \
                NativeId TEXT, ChargeList TEXT,PeaksId INT, PrecursorIntensity REAL, \
                FragmentationType TEXT,ParentScanNumber INT, ParentNativeId TEXT, \
                Comment TEXT, MetaText TEXT, DebugText TEXT, Valid INTEGER DEFAULT 1, \
                MobilityValue REAL, \
                FOREIGN KEY(FilesId) REFERENCES Files(Id),FOREIGN KEY(PeaksId) \
                REFERENCES Peaks(Id));";
    e = QEXEC_CMD(query, command); ree;

    e = QEXEC_CMD(query, "DROP TABLE IF EXISTS CompressionInfo;"); ree;
    command = "CREATE TABLE IF NOT EXISTS CompressionInfo(Id INTEGER PRIMARY KEY, \
                MzDict BLOB, IntensityDict BLOB, Property TEXT, Version TEXT);";
    e = QEXEC_CMD(query, command); ree;

    e = QEXEC_CMD(query, "DROP TABLE IF EXISTS Chromatogram;"); ree;
    command = "CREATE TABLE IF NOT EXISTS Chromatogram(Id INTEGER PRIMARY KEY, \
                FilesId INT, Identifier TEXT, ChromatogramType TEXT, DataX BLOB, \
                DataY BLOB, DataCount INT, MetaText TEXT, DebugText TEXT, \
                FOREIGN KEY(FilesId) REFERENCES Files(Id));";
    e = QEXEC_CMD(query, command); ree;

    return e;
}

Err MSWriterByspec2::insertDataIntoFilesTable(const QString &sourceFile, int *fileId)
{
    if (!m_database.isValid()) {
        return kError;
    }

    Err e = kNoErr;
    QSqlQuery query = makeQuery(m_database, true);

    const QString command("INSERT into Files(Filename) values(:FileName)");

    e = QPREPARE(query, command); ree;
    query.bindValue(":FileName", sourceFile);
    e = QEXEC_NOARG(query); ree;

    *fileId = query.lastInsertId().toInt();

    return e;
}

Err MSWriterByspec2::insertDataIntoFileInfoTable(const QMap<QString, QString> &data, int fileId)
{
    if (!m_database.isValid()) {
        return kError;
    }

    Err e = kNoErr;

    QSqlQuery query = makeQuery(m_database, true);

    const QString command
        = QString("INSERT INTO FilesInfo(FilesId, Key, Value) values(:FilesId, :Key, :Value)");

    e = QPREPARE(query, command); ree;
    for (QMap<QString, QString>::const_iterator it = data.cbegin(); it != data.cend(); ++it) {
        // This is the same value for every entry, but we'd better exploit preparation safety here.
        query.bindValue(":FilesId", fileId);

        query.bindValue(":Key", it.key());
        query.bindValue(":Value", it.value());
        e = QEXEC_NOARG(query); ree;
    }

    return e;
}

Err writeBrokenSpectra(QSqlQuery& query, int fileId, int scan, const QString& errorMessage)
{
    query.bindValue(":FilesId", fileId);
    query.bindValue(":ScanNumber", scan);
    query.bindValue(":Comment", QVariant(errorMessage));
    return QEXEC_NOARG(query);
}

Err MSWriterByspec2::insertDataIntoPeaksAndSpectraTable(bool onlyCentroided, int fileId)
{
    if (!m_database.isValid()) {
        return kError;
    }

    static const QString conversionErrorMessage = QStringLiteral("ConversionErrorMessage");

    Err e = kNoErr;

    QSqlQuery queryProfilePeaks = makeQuery(m_database, true);
    QSqlQuery queryCentroidPeaks = makeQuery(m_database, true);
    QSqlQuery querySpectra = makeQuery(m_database, true);
    QSqlQuery queryInvalidSpectra = makeQuery(m_database, true);

    TransactionInstance instance(&m_database);
    instance.setRollbackOnDestruction(true);

    MSReader *reader = MSReader::Instance();

    long start = 0;
    long end = 0;
    long total = 0;
    e = reader->getNumberOfSpectra(&total, &start, &end);
    if (e != kNoErr) {
        QMap<QString, QString> messages;
        messages[conversionErrorMessage] = QStringLiteral("getNumberOfSpectra returns error: ")
            + QString::fromStdString(convertErrToString(e));
        insertDataIntoFileInfoTable(messages, fileId);
        ree;
    }

    const QString commandPeaksTemplate
        = QStringLiteral("INSERT INTO %1 (Id, PeaksMz, PeaksIntensity, PeaksCount, \
                        IntensitySum, MetaText, Comment, CompressionInfoId) \
                        values(:Id, :PeaksMz, :PeaksIntensity, :PeaksCount, :IntensitySum, :MetaText, \
                        :Comment, :CompressionInfoId)");
    const QString commandProfilePeaks
        = onlyCentroided ? QString() : commandPeaksTemplate.arg(TABLE_PEAKS);
    const QString commandCentroidPeaks
        = commandPeaksTemplate.arg(onlyCentroided ? TABLE_PEAKS : TABLE_PEAKS_MS1CENTROIDED);

    const QString commandSpectra = QStringLiteral(
        "INSERT INTO Spectra(FilesId, MSLevel, ObservedMz, IsolationWindowLowerOffset, \
        IsolationWindowUpperOffset, RetentionTime, ScanNumber, NativeId, ChargeList, PeaksId, \
        PrecursorIntensity, FragmentationType, ParentScanNumber, ParentNativeId, Comment, \
        MetaText, DebugText, MobilityValue) \
        values(:FilesId, :MSLevel, :ObservedMz, :IsolationWindowLowerOffset, \
        :IsolationWindowUpperOffset, :RetentionTime, :ScanNumber, :NativeId, :ChargeList, :PeaksId, \
        :PrecursorIntensity, :FragmentationType, :ParentScanNumber, :ParentNativeId, :Comment, \
        :MetaText, :DebugText, :MobilityValue)");
    const QString commandInvalidSpectra(
        "INSERT INTO Spectra(FilesId, ScanNumber, PeaksId, Comment, Valid) \
        values(:FilesId, :ScanNumber, NULL, :Comment, 0)");

    if (!onlyCentroided) {
        e = QPREPARE(queryProfilePeaks, commandProfilePeaks); ree;
    }
    e = QPREPARE(queryCentroidPeaks, commandCentroidPeaks); ree;
    e = QPREPARE(querySpectra, commandSpectra); ree;
    e = QPREPARE(queryInvalidSpectra, commandInvalidSpectra); ree;

    e = instance.beginTransaction(); ree;

    int brokenScans = 0;
    auto queryValuesBinder = [](MSReader* reader, QSqlQuery& queryPeaks, int index, int peakId, bool centroided)->Err {
        point2dList points;
        PointListAsByteArrays pointsInBytes;
        const Err e = reader->getScanData(index, &points, centroided, &pointsInBytes); ree;

        double product = 0;
        for (const QPointF &point : points) {
            product += point.y();
        }

        int compressionInfo = 0;
        if (pointsInBytes.compressionInfoId.isValid()) {
            compressionInfo = pointsInBytes.compressionInfoId.toInt();
        }

        queryPeaks.bindValue(":Id", peakId);
        queryPeaks.bindValue(":PeaksMz", pointsInBytes.dataX);
        queryPeaks.bindValue(":PeaksIntensity", pointsInBytes.dataY);
        queryPeaks.bindValue(":PeaksCount", points.size());
        queryPeaks.bindValue(":IntensitySum", product);
        queryPeaks.bindValue(":MetaText", QVariant());
        queryPeaks.bindValue(":Comment", QVariant());
        queryPeaks.bindValue(":CompressionInfoId", compressionInfo);

        return kNoErr;
    };

    int peakId = 1;
    for (long index = start; index <= end; ++index) {
        QStringList errorMessages;

        /// Here data will go into Spectra table
        ScanInfo info;
        e = reader->getScanInfo(index, &info);
        if (e != kNoErr) {
            errorMessages << QStringLiteral("getScanInfo with scanNo=%1 returns error: %2")
                                 .arg(index)
                                 .arg(QString::fromStdString(convertErrToString(e)));
        } else {
            static const QString errorMessageTemplate
                = QStringLiteral("getScanData with scanNo=%1, centroided=%2 returns error: %3");

            if (!onlyCentroided) {
                const bool centroided = false;
                e = queryValuesBinder(reader, queryProfilePeaks, index, peakId, centroided);
                if (e != kNoErr) {
                    errorMessages << errorMessageTemplate.arg(index)
                                         .arg(centroided)
                                         .arg(QString::fromStdString(convertErrToString(e)));
                }
            }

            const bool centroided = true;
            e = queryValuesBinder(reader, queryCentroidPeaks, index, peakId, centroided);
            if (e != kNoErr) {
                errorMessages << errorMessageTemplate.arg(index)
                                     .arg(centroided)
                                     .arg(QString::fromStdString(convertErrToString(e)));
            }
        }

        if (!errorMessages.isEmpty()) {
            brokenScans++;
            const Err e = writeBrokenSpectra(queryInvalidSpectra, fileId, index,
                                             errorMessages.join(QStringLiteral(", "))); ree;
            continue;
        }

        if (info.scanLevel > 1) {
            PrecursorInfo precursorInfo;
            /// For some vendor data format this gives error which is not sufficient to
            /// return from this point, so didn't return on any such failures
            e = reader->getScanPrecursorInfo(index, &precursorInfo);
            if (e != kNoErr) {
                const QString errorMessage
                    = QStringLiteral("getScanPrecursorInfo with scanNo=%1 returns error: %2")
                          .arg(index)
                          .arg(QString::fromStdString(convertErrToString(e)));
                e = writeBrokenSpectra(queryInvalidSpectra, fileId, index, errorMessage); ree;
                brokenScans++;
                continue;
            }

            querySpectra.bindValue(":ObservedMz", precursorInfo.dMonoIsoMass);
            querySpectra.bindValue(":IsolationWindowLowerOffset", precursorInfo.lowerWindowOffset);
            querySpectra.bindValue(":IsolationWindowUpperOffset", precursorInfo.upperWindowOffset);
            querySpectra.bindValue(":ChargeList", QString::number(precursorInfo.nChargeState));
            QString fragmentationType;
            reader->getFragmentType(index, info.scanLevel, &fragmentationType);
            querySpectra.bindValue(":FragmentationType", QVariant(fragmentationType));
            querySpectra.bindValue(":ParentScanNumber", int(precursorInfo.nScanNumber));
            querySpectra.bindValue(":ParentNativeId", precursorInfo.nativeId);
        } else {
            querySpectra.bindValue(":ObservedMz", QVariant());
            querySpectra.bindValue(":IsolationWindowLowerOffset", QVariant());
            querySpectra.bindValue(":IsolationWindowUpperOffset", QVariant());
            querySpectra.bindValue(":ChargeList", QVariant());
            querySpectra.bindValue(":FragmentationType", QVariant());
            querySpectra.bindValue(":ParentScanNumber", QVariant());
            querySpectra.bindValue(":ParentNativeId", QVariant());
        }
        //Byonic Viewer doesn't have a way to view the Spectra.MobilityData without
        //some effort. Until Byonic Viewer can properly setup SQL statement, we'll
        //keep it in the comment section.
        if (info.mobility.isValid()) {
            const QString mobilityComment = QStringLiteral("mobility=%1").arg(info.mobility.mobilityValue());
            querySpectra.bindValue(":Comment", mobilityComment);
        } else {
            querySpectra.bindValue(":Comment", QVariant());
        }

        querySpectra.bindValue(":FilesId", fileId);
        querySpectra.bindValue(":MSLevel", info.scanLevel);

        querySpectra.bindValue(":RetentionTime", info.retTimeMinutes * 60);
        querySpectra.bindValue(":ScanNumber", int(index));
        querySpectra.bindValue(":NativeId", info.nativeId);

        querySpectra.bindValue(":PeaksId", peakId);
        querySpectra.bindValue(":PrecursorIntensity", QVariant());

        if (info.mobility.isValid()) {
            querySpectra.bindValue(":MobilityValue", info.mobility.mobilityValue());
        } else {
            querySpectra.bindValue(":MobilityValue", QVariant());
        }

        QString metaText(
            "<RetentionTimeUnit>second</RetentionTimeUnit><PeakMode>%1 spectrum</PeakMode>");

        switch (info.peakMode) {
        case PeakPickingProfile:
            metaText = metaText.arg("profile");
            break;

        case PeakPickingCentroid:
            metaText = metaText.arg("centroid");
            break;

        case PeakPickingModeUnknown:
            metaText = metaText.arg("unknown");
            break;
        }

        querySpectra.bindValue(":MetaText", metaText);
        querySpectra.bindValue(":DebugText", QVariant());

        if (!onlyCentroided) {
            e = QEXEC_NOARG(queryProfilePeaks); ree;
        }
        e = QEXEC_NOARG(queryCentroidPeaks); ree;
        e = QEXEC_NOARG(querySpectra); ree;
        ++peakId;
    }
    e = instance.endTransaction(); ree;

    if (brokenScans > 0) {
        QMap<QString, QString> messages;
        messages[conversionErrorMessage]
            = QStringLiteral("There were %1 scan(s) with errors.").arg(brokenScans);
        insertDataIntoFileInfoTable(messages, fileId);
    }

    return e;
}

bool MSWriterByspec2::isValid()
{
    return m_database.isValid();
}

Err MSWriterByspec2::writeChromatogram(const byspec2::ChromatogramDaoEntry &chromatogram)
{
    if (!isValid()) {
        return kError;
    }

    // TODO: Overwrite an existing chromatogram
    Err e = kNoErr;
    QSqlQuery query = makeQuery(m_database, true);

    const QString command = QStringLiteral("INSERT INTO Chromatogram (Id, FilesId, Identifier, "
                                      "ChromatogramType, DataX, DataY, DataCount, MetaText, DebugText) "
                                      "VALUES (:Id, :FilesId, :Identifier, :ChromatogramType, :DataX, :DataY,"
                                      ":DataCount, :MetaText, :DebugText)");
    e = QPREPARE(query, command); ree;

    query.bindValue(":Id", chromatogram.id);
    query.bindValue(":FilesId", chromatogram.filesId);
    query.bindValue(":Identifier", chromatogram.identifier);
    query.bindValue(":ChromatogramType", chromatogram.chromatogramType);
    query.bindValue(":DataX", QByteArray::fromRawData(reinterpret_cast<const char*>(chromatogram.dataX.data()),
                                  chromatogram.dataX.size() * static_cast<int>(sizeof(double))));
    query.bindValue(":DataY", QByteArray::fromRawData(reinterpret_cast<const char*>(chromatogram.dataY.data()),
                                  chromatogram.dataY.size() * static_cast<int>(sizeof(double))));
    query.bindValue(":DataCount", chromatogram.dataCount);
    query.bindValue(":MetaText", chromatogram.metaText);
    query.bindValue(":DebugText", chromatogram.debugText);

    e = QEXEC_NOARG(query); ree;

    return e;
}

Err MSWriterByspec2::writePeak(const byspec2::PeakDaoEntry &peak, PeakOptions options,
                               const pico::CentroidOptions &centroidOptions)
{
    if (!isValid()) {
        return kError;
    }

    // TODO: Overwrite an existing peak
    Err e = kNoErr;

    if (options.testFlag(PeakOption::Normal)) {
        // Write normal peak
        // We're creating a copy of a passed peak to overwrite intensitySum and leave the initial
        // peak intact No overhead is introduced due to COW in QVectors within Peak.
        byspec2::PeakDaoEntry peakCopy = peak;
        if (options.testFlag(PeakOption::CalculateIntensitySum)) {
            calculatePeakIntensitySum(peakCopy);
        }
        e = writePeakToDb(peakCopy, false); ree;
    }
    const bool centroided = options.testFlag(PeakOption::Centroided);
    const bool centroidedAsNormal = options.testFlag(PeakOption::CentroidedAsNormal);
    if (centroided || centroidedAsNormal) {
        // Calculate centroided peak
        const byspec2::PeakDaoEntry centroidedPeak = makeCentroided(peak, centroidOptions);
        if(centroided) {
            e = writePeakToDb(centroidedPeak, true); ree;
        }
        /// TODO: Rework this logic in LT-2190
        if(centroidedAsNormal) {
            e = writePeakToDb(centroidedPeak, false); ree;
        }
    }

    return e;
}

Err MSWriterByspec2::writeSpectrum(const byspec2::SpectrumDaoEntry &spectrum)
{
    if (!isValid()) {
        return kError;
    }

    // TODO: Overwrite an existing peak
    Err e = kNoErr;
    QSqlQuery query = makeQuery(m_database, true);

    const QString command = QStringLiteral(
        "INSERT INTO Spectra (Id, FilesId, MSLevel, "
        "ObservedMz, IsolationWindowLowerOffset, IsolationWindowUpperOffset, RetentionTime, "
        "ScanNumber, NativeId, ChargeList, PeaksId, PrecursorIntensity, FragmentationType, "
        "ParentScanNumber, ParentNativeId, Comment, MetaText, DebugText) "
        "VALUES (:Id, :FilesId, :MSLevel, :ObservedMz, :IsolationWindowLowerOffset, :IsolationWindowUpperOffset, "
        ":RetentionTime, :ScanNumber, :NativeId, :ChargeList, :PeaksId, :PrecursorIntensity, :FragmentationType, "
        ":ParentScanNumber, :ParentNativeId, :Comment, :MetaText, :DebugText)");
    e = QPREPARE(query, command); ree;

    query.bindValue(":Id", spectrum.id);
    query.bindValue(":FilesId", spectrum.filesId);
    query.bindValue(":MSLevel", spectrum.msLevel);
    query.bindValue(":ObservedMz", spectrum.observedMz);
    query.bindValue(":IsolationWindowLowerOffset", spectrum.isolationWindowLowerOffset);
    query.bindValue(":IsolationWindowUpperOffset", spectrum.isolationWindowUpperOffset);
    query.bindValue(":RetentionTime", spectrum.retentionTime);
    query.bindValue(":ScanNumber", spectrum.scanNumber);
    query.bindValue(":NativeId", spectrum.nativeId);
    query.bindValue(":ChargeList", spectrum.chargeList);
    query.bindValue(":PeaksId", spectrum.peaksId);
    query.bindValue(":PrecursorIntensity", spectrum.precursorIntensity);
    query.bindValue(":FragmentationType", spectrum.fragmentationType);
    query.bindValue(":ParentScanNumber", spectrum.parentScanNumber);
    query.bindValue(":ParentNativeId", spectrum.parentNativeId);
    query.bindValue(":Comment", spectrum.comment);
    query.bindValue(":MetaText", spectrum.metaText);
    query.bindValue(":DebugText", spectrum.debugText);

    e = QEXEC_NOARG(query); ree;

    return e;
}

Err MSWriterByspec2::writeCompressionInfo(const byspec2::CompressionInfoDaoEntry &compressionInfo)
{
    if (!isValid()) {
        return kError;
    }

    // TODO: Overwrite an existing peak
    Err e = kNoErr;
    QSqlQuery query = makeQuery(m_database, true);

    const QString command = QStringLiteral(
        "INSERT INTO CompressionInfo (Id, MzDict, IntensityDict, Property, Version)"
        "VALUES (:Id, :MzDict, :IntensityDict, :Property, :Version)");
    e = QPREPARE(query, command); ree;

    query.bindValue(":Id", compressionInfo.id);
    query.bindValue(":MzDict", compressionInfo.mzDict);
    query.bindValue(":IntensityDict", compressionInfo.intensityDict);
    query.bindValue(":Property", compressionInfo.property);
    query.bindValue(":Version", compressionInfo.version);

    e = QEXEC_NOARG(query); ree;

    return e;
}

Err MSWriterByspec2::writeFile(const byspec2::FileDaoEntry &file)
{
    if (!isValid()) {
        return kError;
    }

    // TODO: Overwrite an existing peak
    Err e = kNoErr;
    QSqlQuery query = makeQuery(m_database, true);

    const QString command = QStringLiteral(
        "INSERT INTO Files (Id, Filename, Location, Type, Signature)"
        "VALUES (:Id, :Filename, :Location, :Type, :Signature)");
    e = QPREPARE(query, command); ree;

    query.bindValue(":Id", file.id);
    query.bindValue(":Filename", file.filename);
    query.bindValue(":Location", file.location);
    query.bindValue(":Type", file.type);
    query.bindValue(":Signature", file.signature);

    e = QEXEC_NOARG(query); ree;

    return e;
}

Err MSWriterByspec2::writeFileInfo(const byspec2::FileInfoDaoEntry &fileInfo)
{
    if (!isValid()) {
        return kError;
    }

    // TODO: Overwrite an existing peak
    Err e = kNoErr;
    QSqlQuery query = makeQuery(m_database, true);

    const QString command = QStringLiteral(
        "INSERT INTO FilesInfo (Id, FilesId, Key, Value)"
        "VALUES (:Id, :FilesId, :Key, :Value)");
    e = QPREPARE(query, command); ree;

    query.bindValue(":Id", fileInfo.id);
    query.bindValue(":FilesId", fileInfo.filesId);
    query.bindValue(":Key", fileInfo.key);
    query.bindValue(":Value", fileInfo.value);

    e = QEXEC_NOARG(query); ree;

    return e;
}

Err MSWriterByspec2::writeInfo(const byspec2::InfoDaoEntry &info)
{
    if (!isValid()) {
        return kError;
    }

    // TODO: Overwrite an existing peak
    Err e = kNoErr;
    QSqlQuery query = makeQuery(m_database, true);

    const QString command = QStringLiteral(
        "INSERT INTO Info (Id, Key, Value)"
        "VALUES (:Id, :Key, :Value)");
    e = QPREPARE(query, command); ree;

    query.bindValue(":Id", info.id);
    query.bindValue(":Key", info.key);
    query.bindValue(":Value", info.value);

    e = QEXEC_NOARG(query); ree;

    return e;
}

Err MSWriterByspec2::writePeakToDb(const byspec2::PeakDaoEntry &peak, bool isCentroided)
{
    if (!isValid()) {
        return kError;
    }

    Err e = kNoErr;
    QSqlQuery query = makeQuery(m_database, true);

    const QString table = isCentroided ? TABLE_PEAKS_MS1CENTROIDED : TABLE_PEAKS;
    const QString command = QStringLiteral("INSERT INTO %1 (Id, PeaksMz, PeaksIntensity, "
                                      "SpectraIdList, PeaksCount, MetaText, Comment, IntensitySum, CompressionInfoId) "
                                      "VALUES (:Id, :PeaksMz, :PeaksIntensity, :SpectraIdList, :PeaksCount, :MetaText, "
                                      ":Comment, :IntensitySum, :CompressionInfoId)").arg(table);
    e = QPREPARE(query, command); ree;

    query.bindValue(":Id", peak.id);
    query.bindValue(":PeaksMz", QByteArray::fromRawData(reinterpret_cast<const char*>(peak.peaksMz.data()),
                                  peak.peaksMz.size() * static_cast<int>(sizeof(double))));
    query.bindValue(":PeaksIntensity", QByteArray::fromRawData(reinterpret_cast<const char*>(peak.peaksIntensity.data()),
                                  peak.peaksIntensity.size() * static_cast<int>(sizeof(float))));
    query.bindValue(":SpectraIdList", peak.spectraIdList);
    query.bindValue(":PeaksCount", peak.peaksCount);
    query.bindValue(":MetaText", peak.metaText);
    query.bindValue(":Comment", peak.comment);
    query.bindValue(":IntensitySum", peak.intensitySum);
    query.bindValue(":CompressionInfoId", peak.compressionInfoId);

    e = QEXEC_NOARG(query); ree;
    return e;
}

Err MSWriterByspec2::openDatabase(const QString &outputFile)
{
    const QString outputDirectory = QFileInfo(outputFile).absolutePath();

    if (!QDir(outputDirectory).exists()) {
        QDir().mkpath(outputDirectory);
    }
    m_database.setDatabaseName(outputFile);

    if (!m_database.open() || !QFileInfo(outputFile).isWritable()) {
        return kFileOpenError;
    }
    return kNoErr;
}

_PMI_END
