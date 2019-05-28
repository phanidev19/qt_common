/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "ScanIterator.h"

// common_ms
#include "CrossSampleFeatureCollatorTurbo.h"
#include "MSReader.h"
#include "pmi_common_ms_debug.h"
#include "ProgressBarInterface.h"
#include "ProgressContext.h"

// common_mini
#include "CommonFunctions.h"
#include "FindMzToProcess.h"
#include "SpectraDisambigutron.h"
#include "SpectraSubtractomatic.h"

// stable
#include <sqlite_utils.h>
#include <QtSqlUtils.h>

#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Sparse>

#define SPECTRA_DISAMBIGUTRON
//#define  TROUBLESHOOTING_ENABLED

//// TODO make sure you take care of all the returned Err from each function and implement the error
///// checking
_PMI_BEGIN

ScanIterator::ScanIterator(const QString &neuralNetworkDbFilePath, const SettableFeatureFinderParameters &ffUserParams)
    : m_neuralNetworkDbFilePath(neuralNetworkDbFilePath)
    , m_isWorkingDirectorySet(false)
    , m_ffUserParams(ffUserParams)
{
}

ScanIterator::~ScanIterator()
{
}

Err ScanIterator::init()
{
    Err e = kNoErr;

    if (!QFileInfo::exists(m_neuralNetworkDbFilePath)) {
        warningMs() << "Failed to load neural network at" << m_neuralNetworkDbFilePath;
        rrr(kError);
    }

    e = m_chargeDeterminator.init(m_neuralNetworkDbFilePath); ree;
    e = m_monoDeterminator.init(m_neuralNetworkDbFilePath); ree;

    return e;
}

void ScanIterator::setWorkingDirectory(const QDir &workingDirectory)
{
    m_isWorkingDirectorySet = true;
    m_workingDirectory = workingDirectory;
}

QDir ScanIterator::workingDirectory() const
{
    return m_workingDirectory;
}

Err ScanIterator::createFeatureFinderDB(QSqlDatabase &db, const QString &msFilePath, QString *outputFilePath)
{
    QFileInfo fi(msFilePath);
    QString fileName = QString("%1.ftrs").arg(fi.completeBaseName());

    const QString dbFilePath = m_isWorkingDirectorySet ? m_workingDirectory.filePath(fileName)
        : fi.absoluteDir().filePath(fileName);

    if (QFileInfo(dbFilePath).exists()) {
        // remove the file
        warningMs() << "Ftrs file already exists! Removing" << dbFilePath;
        bool ok = QFile::remove(dbFilePath);
        if (!ok) {
            warningMs() << "Failed to remove ftrs file!";
            rrr(kError);
        }
    }

    debugMs() << "Saving features to" << dbFilePath;

    QString connectionName = QStringLiteral("ScanIterator_ftrs_cache_%1")
                                 .arg(QFileInfo(dbFilePath).completeBaseName());

    Err e = addDatabaseAndOpen(connectionName, dbFilePath, db);
    if (e != kNoErr) {
        qDebug() << "Failed to load or create database"
                 << QString::fromStdString(pmi::convertErrToString(e));
    }
    
    e = createFeatureFinderSqliteDbSchema(db); ree;

    QString sql = QString(
        R"(INSERT INTO FeatureFinderSettings (parameter, value) 
						VALUES (:parameter, :value))");
    QSqlQuery q = makeQuery(db, true);
    e = QPREPARE(q, sql); ree;

    q.bindValue(":parameter", "averagineCorrelationCutOff");
    q.bindValue(":value", m_ffUserParams.averagineCorrelationCutOff);
    e = QEXEC_NOARG(q); ree;

    q.bindValue(":parameter", "minFeatureMass");
    q.bindValue(":value", m_ffUserParams.minFeatureMass);
    e = QEXEC_NOARG(q); ree;

    q.bindValue(":parameter", "maxFeatureMass");
    q.bindValue(":value", m_ffUserParams.maxFeatureMass);
    e = QEXEC_NOARG(q); ree;

    q.bindValue(":parameter", "minScanCount");
    q.bindValue(":value", m_ffUserParams.minScanCount);
    e = QEXEC_NOARG(q); ree;

    q.bindValue(":parameter", "minIsotopeCount");
    q.bindValue(":value", m_ffUserParams.minIsotopeCount);
    e = QEXEC_NOARG(q); ree;

    q.bindValue(":parameter", "noiseFactorMultiplier");
    q.bindValue(":value", m_ffUserParams.noiseFactorMultiplier);
    e = QEXEC_NOARG(q); ree;

    q.bindValue(":parameter", "ppm");
    q.bindValue(":value", m_ffUserParams.ppm);
    e = QEXEC_NOARG(q); ree;

    q.bindValue(":parameter", "minPeakWidthMinutes");
    q.bindValue(":value", m_ffUserParams.minPeakWidthMinutes);
    e = QEXEC_NOARG(q); ree;

    *outputFilePath = dbFilePath;

    return e;
}

Err ScanIterator::iterateMSFileLinearDBSCANSelect(const QString &msFilePath, QString *outputPath,
                                                  QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    if (!outputPath) {
        rrr(kBadParameterError);
    }

    QSqlDatabase db;
    MSReader *ms = MSReader::Instance();

#ifdef TROUBLESHOOTING_ENABLED
    QString testFile = "P:/PMI_Share_Data/Data2018/NIST/MAM RR/9119/SPK_MS.RAW";
    e = createFeatureFinderDB(db, testFile, outputPath); ree;
    e = ms->openFile(testFile); ree;
#else
    e = createFeatureFinderDB(db, msFilePath, outputPath); ree;
    e = ms->openFile(msFilePath); ree;
#endif
    ////Instantiate required classes and variables for scan iteration
    const bool doCentroid = true;

    QList<msreader::ScanInfoWrapper> scanInfoList;
    e = ms->getScanInfoListAtLevel(1, &scanInfoList); ree;

    FindMzToProcess mzFinderNew(m_chargeDeterminator, m_ffUserParams);
    SpectraSubtractomatic chargeClusterDecimator(m_ffUserParams.minIsotopeCount);
    e = chargeClusterDecimator.init(); ree;
    SpectraDisambigutron spectraDisambigutron;
    e = spectraDisambigutron.init(); ree;

    ////Being Scanning DB Transaction
    bool ok = db.transaction();
    if (!ok) {
        return kError;
    }

    if (progress) {
        progress->setText(QObject::tr("Feature finding at %1").arg(msFilePath));
    }

    {
        ProgressContext progressContext(scanInfoList.size(), progress);
        //// Begin Processing Scan
        for (int i = 0; i < scanInfoList.size(); ++i, ++progressContext) {
            const msreader::ScanInfoWrapper &scanInfo = scanInfoList[i];

            ScanDetails scanDetails;
            scanDetails.scanIndex = i;
            scanDetails.vendorScanNumber = scanInfo.scanNumber;
            scanDetails.rt = scanInfo.scanInfo.retTimeMinutes;

// For Troubleshooting and Testing uncomment the define above
#ifdef TROUBLESHOOTING_ENABLED
        int vendorScanNumber = 17746; // "P:\PMI_Share_Data\Data2018\NIST\MAM RR\9119\SPK_MS.RAW"
        if (scanInfo.scanNumber < vendorScanNumber) {
            continue;
        }
        else if (scanInfo.scanNumber > vendorScanNumber + 0) {
            break;
        }
#endif

            ///////////////////////////////////////////////////////////////////////////////////////////

            //////Retrieve Scan from file
            point2dList points;
            e = ms->getScanData(scanDetails.vendorScanNumber, &points, doCentroid); ree;

            if (points.empty()) {
                std::cout << "Scan " << scanInfo.scanNumber;
                std::cout << " Point Count " << points.size() << std::endl;
                continue;
            } else if (static_cast<int>(points.size()) > m_ffParams.maxIonCount + 1) {
                auto point2d_greater_y = [](const point2d &a, const point2d &b) {
                    return b.y() < a.y();
                };
                std::sort(points.begin(), points.end(), point2d_greater_y);
                points.resize(m_ffParams.maxIonCount);
                std::sort(points.begin(), points.end(), point2d_less_x);
            }


            // Convert scanData into Eigen::RowVectorXd  TODO Abstract this to Common Functions
            // later////////
            Eigen::SparseVector<double, Eigen::RowMajor> fullScan(
                static_cast<int>(m_ffParams.mzMax * m_ffParams.vectorGranularity));
            fullScan.reserve(m_ffParams.maxIonCount);
            for (size_t j = 0; j < points.size(); ++j) {
                double insertionPoint
                    = FeatureFinderUtils::hashMz(points[j].x(), m_ffParams.vectorGranularity);
                if (insertionPoint
                    < static_cast<int>(m_ffParams.mzMax * m_ffParams.vectorGranularity)) {
                    fullScan.insert(insertionPoint) = points[j].y();
                }
            }

            // Build Mz Iterators here w/ linearDBSCAN clustering.
            point2dList pointsOfInterest = mzFinderNew.searchFullScanForMzIterators(points, fullScan);
    

            // Iterate Points of Interest & determine charge, monoiso, subtract scans, and enter charge
            // cluster into DB
            std::vector<ChargeCluster> vectorChargeCluster;
            for (size_t k = 0; k < pointsOfInterest.size(); ++k) {

                double t_mz = pointsOfInterest[k].x();

                // Slice the array for use in the following functions and check if viable
                int t_index = FeatureFinderUtils::hashMz(t_mz, m_ffParams.vectorGranularity);
                Eigen::SparseVector<double> scanSegment
                    = fullScan.middleCols(t_index - m_ffParams.mzMatchIndex, (2 * m_ffParams.mzMatchIndex) + 1);

                Eigen::RowVectorXd scanSegmentMzTooth
                    = fullScan.middleCols(t_index - m_ffParams.errorRangeHashed, (2 * m_ffParams.errorRangeHashed) + 1);
                int t_maxIntensity = static_cast<int>(scanSegmentMzTooth.maxCoeff());
                if (scanSegment.cols() == 0 || t_maxIntensity < mzFinderNew.noiseFloor()) {
                    continue;
                }

                // Determine Charge
                int charge = m_chargeDeterminator.determineCharge(scanSegment);

                // Determine Mono Offset
                if (charge == 0) {
                    continue;
                }


#ifdef SPECTRA_DISAMBIGUTRON
                Eigen::SparseVector<double> cleanedScan;
                e = spectraDisambigutron.removeOverlappingIonsFromScan(scanSegment, charge, &cleanedScan); ree;
                int monoOffset = m_monoDeterminator.determineMonoisotopeOffset(cleanedScan, t_mz, charge);
                Decimator decimator = chargeClusterDecimator.buildChargeClusterDecimator(
                    cleanedScan, t_mz, charge, monoOffset, true);
#else
                int monoOffset = m_monoDeterminator.determineMonoisotopeOffset(scanSegment, t_mz, charge);
                Decimator decimator = chargeClusterDecimator.buildChargeClusterDecimator(
                    scanSegment, t_mz, charge, monoOffset, false);
#endif

                fullScan -= decimator.clusterDecimator; 

                // Sets all negative values in fullScan to 0 after decimator subtraction
                for (Eigen::SparseVector<double, Eigen::RowMajor>::InnerIterator it(fullScan); it;
                     ++it) {
                    if (it.value() < 0) {
                        fullScan.coeffRef(it.index()) = 0;
                    }
                }
                fullScan.prune(0.0);

    // For Troubleshooting and Testing uncomment the define above
    #ifdef TROUBLESHOOTING_ENABLED

                if (decimator.correlation > m_ffUserParams.averagineCorrelationCutOff) {
                        std::cout << "mz: " << t_mz << " charge: " << charge << " mono: " << monoOffset
                                  << " corr: " << decimator.correlation
                                  << " mw: " << (charge * t_mz) - charge - monoOffset << std::endl;
                    }
    #endif

                // Build Charge Cluster struct for entry to DB
                ChargeCluster chargeCluster;
                chargeCluster.charge = charge;
                chargeCluster.corr = decimator.correlation;
                chargeCluster.maxIntensity = t_maxIntensity;
                chargeCluster.monoOffset = monoOffset;
                chargeCluster.mzFound = t_mz;
                chargeCluster.istopeCount = decimator.isotopeCount;
                chargeCluster.scanNoiseFloor = mzFinderNew.noiseFloor();
                vectorChargeCluster.push_back(chargeCluster);
            }

            e = saveChargeClusterIntoDb(db, scanDetails, vectorChargeCluster); ree;

            if ((i % 1000) == 0) {
                debugMs() << i << "scans processed";
            }

        } ////End Processing Scan
    }

    //// End Scanning DB Transaction
    ok = db.commit();
    if (!ok) {
        return kError;
    }

    ////Collate Charge Clusters found to Features w/ DBSCAN.
    CollateChargeClustersToFeatures featureMaker(db, m_ffUserParams);
    e = featureMaker.init(); ree;
    
    ms->closeFile();
    ms->closeAllFileConnections();

    db.close();
    return e;
}

Err ScanIterator::iterateMSFileLinearDBSCANSelectTestPurposes(
    const QVector<FauxScanCreator::Scan> &scansVec, QVector<CrossSampleFeatureTurbo> *crossSampleFeautreTurbosReturn)
{
    Err e = kNoErr;
    int chargeClusterID = 0;

    FindMzToProcess mzFinderNew(m_chargeDeterminator, m_ffUserParams);
    SpectraSubtractomatic chargeClusterDecimator(m_ffUserParams.minIsotopeCount);
    e = chargeClusterDecimator.init(); ree;
    SpectraDisambigutron spectraDisambigutron;
    e = spectraDisambigutron.init(); ree;

    {

        for (int i = 0; i < scansVec.size(); ++i) {

            const FauxScanCreator::Scan & scan = scansVec[i];

            ScanDetails scanDetails;
            scanDetails.scanIndex = i;
            scanDetails.vendorScanNumber = i;
            scanDetails.rt = scan.retTimeMinutes;

            //////Retrieve Scan from file
            point2dList points = scan.scanIons;

            if (points.empty()) {
                debugMs() << "Scan " << i;
                debugMs() << " Point Count " << points.size();
                continue;
            }
            else if (static_cast<int>(points.size()) > m_ffParams.maxIonCount + 1) {
                auto point2d_greater_y = [](const point2d &a, const point2d &b) {
                    return b.y() < a.y();
                };
                std::sort(points.begin(), points.end(), point2d_greater_y);
                points.resize(m_ffParams.maxIonCount);
                std::sort(points.begin(), points.end(), point2d_less_x);
            }

            // Convert scanData into Eigen::RowVectorXd  TODO Abstract this to Common Functions
            // later////////
            Eigen::SparseVector<double, Eigen::RowMajor> fullScan(
                static_cast<int>(m_ffParams.mzMax * m_ffParams.vectorGranularity));
            fullScan.reserve(m_ffParams.maxIonCount);
            for (size_t j = 0; j < points.size(); ++j) {
                double insertionPoint = FeatureFinderUtils::hashMz(points[j].x(), m_ffParams.vectorGranularity);
                if (insertionPoint < static_cast<int>(m_ffParams.mzMax * m_ffParams.vectorGranularity)) {
                    fullScan.insert(insertionPoint) = points[j].y();
                }
            }

            // Build Mz Iterators here w/ linearDBSCAN clustering.
            point2dList pointsOfInterest = mzFinderNew.searchFullScanForMzIterators(points, fullScan);

            // Iterate Points of Interest & determine charge, monoiso, subtract scans, and enter charge
            // cluster into DB
            std::vector<ChargeCluster> vectorChargeCluster;
            for (size_t k = 0; k < pointsOfInterest.size(); ++k) {

                double t_mz = pointsOfInterest[k].x();

                // Slice the array for use in the following functions and check if viable
                int t_index = FeatureFinderUtils::hashMz(t_mz, m_ffParams.vectorGranularity);
                Eigen::SparseVector<double> scanSegment
                    = fullScan.middleCols(t_index - m_ffParams.mzMatchIndex, (2 * m_ffParams.mzMatchIndex) + 1);

                Eigen::RowVectorXd scanSegmentMzTooth
                    = fullScan.middleCols(t_index - m_ffParams.errorRangeHashed, (2 * m_ffParams.errorRangeHashed) + 1);
                int t_maxIntensity = static_cast<int>(scanSegmentMzTooth.maxCoeff());
                if (scanSegment.cols() == 0 || t_maxIntensity < mzFinderNew.noiseFloor()) {
                    continue;
                }

                // Determine Charge
                int charge = m_chargeDeterminator.determineCharge(scanSegment);

                // Determine Mono Offset
                if (charge == 0) {
                    continue;
                }

#ifdef SPECTRA_DISAMBIGUTRON
                Eigen::SparseVector<double> cleanedScan;
                e = spectraDisambigutron.removeOverlappingIonsFromScan(scanSegment, charge, &cleanedScan); ree;
                int monoOffset = m_monoDeterminator.determineMonoisotopeOffset(cleanedScan, t_mz, charge);
                Decimator decimator = chargeClusterDecimator.buildChargeClusterDecimator(
                    cleanedScan, t_mz, charge, monoOffset, true);
#else
                int monoOffset = m_monoDeterminator.determineMonoisotopeOffset(scanSegment, t_mz, charge);
                Decimator decimator = chargeClusterDecimator.buildChargeClusterDecimator(
                    scanSegment, t_mz, charge, monoOffset, false);
#endif

                fullScan -= decimator.clusterDecimator;

                // Sets all negative values in fullScan to 0 after decimator subtraction
                for (Eigen::SparseVector<double, Eigen::RowMajor>::InnerIterator it(fullScan); it;
                    ++it) {
                    if (it.value() < 0) {
                        fullScan.coeffRef(it.index()) = 0;
                    }
                }
                fullScan.prune(0.0);

                // Build Charge Cluster struct for entry to DB
                ChargeCluster chargeCluster;
                chargeCluster.charge = charge;
                chargeCluster.corr = decimator.correlation;
                chargeCluster.maxIntensity = t_maxIntensity;
                chargeCluster.monoOffset = monoOffset;
                chargeCluster.mzFound = t_mz;
                chargeCluster.istopeCount = decimator.isotopeCount;
                chargeCluster.scanNoiseFloor = mzFinderNew.noiseFloor();

                vectorChargeCluster.push_back(chargeCluster);
            }

           
            for (const ChargeCluster & chargeCluster : vectorChargeCluster) {
                MWChargeClusterPoint mwChargeClusterPoint;
                mwChargeClusterPoint.charge = chargeCluster.charge;
                mwChargeClusterPoint.corr = chargeCluster.corr;
                mwChargeClusterPoint.ID = chargeClusterID;
                mwChargeClusterPoint.isotopeCount = chargeCluster.istopeCount;
                mwChargeClusterPoint.maxIntensity = chargeCluster.maxIntensity;
                mwChargeClusterPoint.monoOffset = chargeCluster.monoOffset;
                mwChargeClusterPoint.mwMonoisotopic = (chargeCluster.mzFound * chargeCluster.charge) - (PROTON * (chargeCluster.charge + chargeCluster.monoOffset));
                mwChargeClusterPoint.mzFound = chargeCluster.mzFound;
                mwChargeClusterPoint.rt = scanDetails.rt;
                mwChargeClusterPoint.scanIndex = scanDetails.scanIndex;
                mwChargeClusterPoint.vendorScanNumber = scanDetails.vendorScanNumber;

                if (chargeCluster.corr > m_ffUserParams.averagineCorrelationCutOff) {
                    m_MWChargeClusterPoints.push_back(mwChargeClusterPoint);
                    chargeClusterID++;
                }
            }

            if ((i % 1000) == 0) {
                debugMs() << i << "scans processed";
            }

        } ////End Processing Scan
    }

    ////Collate Charge Clusters found to Features w/ DBSCAN.
    CollateChargeClustersToFeatures featureMaker;
    e = featureMaker.initTestingPurposes(m_MWChargeClusterPoints, m_ffUserParams,
                                         crossSampleFeautreTurbosReturn);

    return e;
}

Err ScanIterator::saveChargeClusterIntoDb(QSqlDatabase &db, const ScanDetails &scanDetails,
                                           const std::vector<ChargeCluster> vectorChargeCluster)
{
    Err e = kNoErr;

    QString sql = QString(
        R"(INSERT INTO ChargeClusters (scanIndex, vendorScanNumber, rt, mzFound, maxIntensity, mwMonoisotopic, monoOffset, corr, charge, isotopeCount, scanNoiseFloor) 
VALUES (:scanIndex, :vendorScanNumber, :rt, :mzFound, :maxIntensity, :mwMonoisotopic, :monoOffset, :corr, :charge, :isotopeCount, :scanNoiseFloor))");
    QSqlQuery q = makeQuery(db, true);
    e = QPREPARE(q, sql); ree;

    for (size_t y = 0; y < vectorChargeCluster.size(); ++y) {
        ChargeCluster t_chargeCluster = vectorChargeCluster[y];
        double monoisotopicMW = (t_chargeCluster.mzFound * t_chargeCluster.charge)
            - (t_chargeCluster.charge * PROTON) - (t_chargeCluster.monoOffset * PROTON);
        if (t_chargeCluster.corr > m_ffUserParams.averagineCorrelationCutOff 
            && monoisotopicMW >= m_ffUserParams.minFeatureMass) {
            q.bindValue(":scanIndex", scanDetails.scanIndex);
            q.bindValue(":vendorScanNumber", scanDetails.vendorScanNumber);
            q.bindValue(":rt", scanDetails.rt);
            q.bindValue(":mzFound", t_chargeCluster.mzFound);
            q.bindValue(":maxIntensity", t_chargeCluster.maxIntensity);
            q.bindValue(":mwMonoisotopic", monoisotopicMW);
            q.bindValue(":monoOffset", t_chargeCluster.monoOffset);
            q.bindValue(":corr", t_chargeCluster.corr);
            q.bindValue(":charge", t_chargeCluster.charge);
            q.bindValue(":isotopeCount", t_chargeCluster.istopeCount);
            q.bindValue(":scanNoiseFloor", t_chargeCluster.scanNoiseFloor);
            e = QEXEC_NOARG(q); ree;
        }
    }

    return e;
}

Err ScanIterator::createFeatureFinderSqliteDbSchema(QSqlDatabase &db)
{
    Err e = kNoErr;

    QString sql
        = QString("CREATE TABLE IF NOT EXISTS ChargeClusters ( ID INTEGER PRIMARY KEY NOT NULL, "
                  "scanIndex INT,"
                  "vendorScanNumber INT, "
                  "rt FLOAT, "
                  "mzFound FLOAT, "
                  "maxIntensity INT, "
                  "mwMonoisotopic FLOAT, "
                  "monoOffset INT, "
                  "corr FLOAT, "
                  "charge INT, "
                  "isotopeCount INT, "
                  "scanNoiseFloor FLOAT)");

    QSqlQuery q = makeQuery(db, true);
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    sql = "DELETE FROM ChargeClusters";
    q = makeQuery(db, true);
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    sql = "CREATE TABLE IF NOT EXISTS Features ( ID INTEGER PRIMARY KEY NOT NULL, "
          "xicStart FLOAT, "
          "xicEnd FLOAT, "
          "rt FLOAT, "
          "feature INT, "
          "mwMonoisotopic FLOAT, "
          "corrMax FLOAT, "
          "maxIntensity INT, "
          "ionCount INT, "
          "chargeOrder VARCHAR(100), "
          "maxIsotopeCount INT)";

    q = makeQuery(db, true);
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    sql = "DELETE FROM Features";
    q = makeQuery(db, true);
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    sql = "CREATE TABLE IF NOT EXISTS msmsToFeatureMatches ( ID INTEGER PRIMARY KEY NOT NULL, "
          "featureID INT, "
          "pqmID INT, "
          "peptide VARCHAR(100))";

    q = makeQuery(db, true);
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    sql = "DELETE FROM msmsToFeatureMatches";
    q = makeQuery(db, true);
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

	sql = "CREATE TABLE IF NOT EXISTS FeatureFinderSettings ( ID INTEGER PRIMARY KEY NOT NULL, "
		"parameter VARCHAR(100), "
		"value VARCHAR(100))";

	q = makeQuery(db, true);
	e = QEXEC_CMD(q, sql); ree;
	e = QEXEC_NOARG(q); ree;

	sql = "DELETE FROM FeatureFinderSettings";
	q = makeQuery(db, true);
	e = QEXEC_CMD(q, sql); ree;
	e = QEXEC_NOARG(q); ree;

    return e;
}

int ScanIterator::determineNoiseFloor(const point2dList &scanData)
{

    std::vector<int> intensities;
    intensities.reserve(scanData.size());
    for (size_t y = 1; y < scanData.size(); ++y) {
        intensities.push_back(scanData[y].y());
    }

    std::sort(intensities.begin(), intensities.end());
    size_t s = intensities.size();

    intensities.resize(static_cast<int>(s * 0.8));

    double median = calculateMedian(intensities);
    double stDev = calculateSD(intensities);

    int noiseFloor = median + (m_ffUserParams.noiseFactorMultiplier * stDev);

    return noiseFloor;
}

double ScanIterator::calculateSD(const std::vector<int> &data)
{
    if (data.empty()) {
        return 0.0;
    }

    size_t s = data.size();
    double sum = 0.0;
    double mean = 0.0;
    double standardDeviation = 0.0;
    for (size_t i = 0; i < s; ++i) {
        sum += data[i];
    }

    mean = sum / s;
    for (size_t i = 0; i < s; ++i) {
        standardDeviation += pow(data[i] - mean, 2);
    }

    return sqrt(standardDeviation / s);
}

double ScanIterator::calculateMedian(std::vector<int> data)
{
    if (data.empty()) {
        return 0.0;
    }

    std::sort(data.begin(), data.end());

    int median = 0;
    int arraySize = static_cast<int>(data.size());
    int midPoint = 0;

    if (arraySize % 2 != 0) {
        midPoint = static_cast<int>(std::floor(arraySize / 2) + 1);
        median = data[midPoint];

    } else {
        midPoint = static_cast<int>(arraySize / 2);
        median = ((data[midPoint] + data[midPoint + 1]) / 2);
    }

    return median;
}

_PMI_END
