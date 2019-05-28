/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*/

#include "MSReaderAgilent.h"
#include "MSReaderByspec.h"
#include "MSReaderBruker.h"
#include "MSReader.h"

#include <CacheFileManager.h>
#include <PMiTestUtils.h>

#include <QtTest>
#include <iostream>
#include <fstream>
#include <iomanip>

#include <fstream>
#include <iostream>

#include <time.h>

inline void trimTrailingZeroIntensities(pmi::point2dList & mzAndIntensityPoints) {
    if (mzAndIntensityPoints.empty()) {
        return;
    }
    while (mzAndIntensityPoints.back().y() == 0) {
        mzAndIntensityPoints.pop_back();
    }
}

inline void trimZeroIntensities(pmi::point2dList & mzAndIntensityPoints) {
    if (mzAndIntensityPoints.empty()) {
        return;
    }
    while (mzAndIntensityPoints.front().y() == 0) {
        mzAndIntensityPoints.erase(mzAndIntensityPoints.begin());
    }
    trimTrailingZeroIntensities(mzAndIntensityPoints);
}

inline void writeToCSV(const QString& filename, const pmi::point2dList& list) {
    std::ofstream file;
    file.open(filename.toStdString());

    for (const auto& p : list) {
        file << std::setprecision(30) << p.x() << "," << p.y() << "\n";
    }
    file.close();
}

// These tests compare Agilent MS file reading with Agilent Byspec file reading, and assumes Agilent Byspec file is read correctly
class MSReaderAgilentCompareWithByspecTest : public QObject
{
    Q_OBJECT
public:
    MSReaderAgilentCompareWithByspecTest();

private Q_SLOTS:
    void initTestCase();

    void testClassTypeName();

    void testGetNumberOfSpectra();
    void testGetTICData();
    void testProfileGetScanData();
    void testCentroidGetScanData();
    void testGetXICData();
    void testGetScanInfo();
    void testGetScanPrecursorInfo();
    void testGetFragmentType();
    void testGetChromatograms();
    void testGetLockmassScans();

    void cleanupTestCase();

private:
    bool fuzzyCompare(double x, double y, double epsilon);
    bool percentCompare(double x, double y, double tolerance);
    double percentDifference(double x, double y);

private:
    QString m_outPath;
    QString m_msFilePath;
    QString m_byspecPath;
    const double m_comparisonTolerance = 1e-4;
    long m_totalNumberOfSpectra = -1;
    long m_startScan = -1;
    long m_endScan = -1;

    std::clock_t m_startTestRunTime = -1;

    pmi::msreader::MSReaderAgilent m_agilent;
    QScopedPointer<pmi::CacheFileManager> m_cacheFileManager;
    pmi::msreader::MSReaderByspec m_pwiz;
};

MSReaderAgilentCompareWithByspecTest::MSReaderAgilentCompareWithByspecTest()
    : QObject()
    , m_cacheFileManager(new pmi::CacheFileManager())
    , m_pwiz(*m_cacheFileManager.data())
{   
    m_msFilePath = QString("%1/MSReaderAgilentTest/BMS-ADC2-1000ng.d").arg(PMI_TEST_FILES_OUTPUT_DIR);
    m_byspecPath = QString("%1/MSReaderAgilentTest/BMS ADC2-1000ng.d.byspec2").arg(PMI_TEST_FILES_OUTPUT_DIR);

    //m_msFilePath = m_path + "\\" + "ecoli-0500-r001.d";
    //m_byspecPath = m_path + "\\" + "ecoli-0500-r001.d.byspec2";

    //m_msFilePath = m_path + "\\" + "Contains_UV_Neupogen.d";
    //m_byspecPath = m_path + "\\" + "Contains_UV_Neupogen.d.byspec2";

    m_outPath = PMI_TEST_FILES_OUTPUT_DIR;

    m_cacheFileManager->setSourcePath(m_byspecPath);
}

void MSReaderAgilentCompareWithByspecTest::initTestCase()
{
    QSKIP("These tests are not ready for other developers yet");

    QVERIFY(m_agilent.canOpen(m_msFilePath));
    QVERIFY(m_pwiz.canOpen(m_byspecPath));
    
    QCOMPARE(m_agilent.openFile(m_msFilePath), pmi::kNoErr);
    QCOMPARE(m_pwiz.openFile(m_byspecPath), pmi::kNoErr);

    long totalNumberOfSpectraPwiz = -1;
    long startScanPwiz = -1;
    long endScanPwiz = -1;
    m_pwiz.getNumberOfSpectra(&totalNumberOfSpectraPwiz, &startScanPwiz, &endScanPwiz);

    m_totalNumberOfSpectra = totalNumberOfSpectraPwiz;
    m_startScan = startScanPwiz;
    m_endScan = endScanPwiz;

    m_startTestRunTime = clock();
}

void MSReaderAgilentCompareWithByspecTest::testGetNumberOfSpectra()
{
    long totalNumberOfSpectraMs = -1;
    long startScanMs = -1;
    long endScanMs = -1;
    m_agilent.getNumberOfSpectra(&totalNumberOfSpectraMs, &startScanMs, &endScanMs);

    long totalNumberOfSpectraPwiz = -1;
    long startScanPwiz = -1;    
    long endScanPwiz = -1;
    m_pwiz.getNumberOfSpectra(&totalNumberOfSpectraPwiz, &startScanPwiz, &endScanPwiz);
    
    QCOMPARE(totalNumberOfSpectraPwiz, totalNumberOfSpectraMs);
    QCOMPARE(startScanPwiz, startScanMs);
    QCOMPARE(endScanPwiz, endScanMs);
}

bool MSReaderAgilentCompareWithByspecTest::fuzzyCompare(double x, double y, double epsilon) {
    bool testStatement = abs(x - y) <= abs(epsilon);
    if (!testStatement) {
        std::cout << "x: " << x << ",y: " << y << std::endl;
    }
    return testStatement;
}

bool MSReaderAgilentCompareWithByspecTest::percentCompare(double x, double y, double tolerance) {
    double difference = x - y;
    double percentageDifference = difference / x * 100;
    bool testStatement = abs(percentageDifference) < abs(tolerance);
    if (!testStatement) {
        std::cout << "x: " << x << ",y: " << y << std::endl;
    }
    return testStatement;
}

double MSReaderAgilentCompareWithByspecTest::percentDifference(double x, double y) {
    double difference = x - y;
    return difference / x * 100;
}

void MSReaderAgilentCompareWithByspecTest::testClassTypeName() 
{
    pmi::MSReaderBase::MSReaderClassType agilentClassNameType = m_agilent.classTypeName();
    pmi::MSReaderBase::MSReaderClassType pwizClassNameType = m_pwiz.classTypeName();

    QCOMPARE(pmi::MSReaderBase::MSReaderClassType::MSReaderClassTypeAgilent, agilentClassNameType);
    QCOMPARE(pmi::MSReaderBase::MSReaderClassType::MSReaderClassTypeByspec, pwizClassNameType);
}

void MSReaderAgilentCompareWithByspecTest::testGetTICData() 
{
    pmi::point2dList TICDataMS;
    pmi::point2dList TICDataPWIZ;
    pmi::Err e = pmi::kNoErr;

    /* MSAgilent.getTICData returned data matching seeMS, which returns all TIC Data.
    pwiz byspec only contains TIC data for MS1. Thus, we make MSAgilent.getTICData return kFunctionNotImplemented, and 
    for this unit test we open the Agilent file in MSReader class so that we can have it eventually call _getTICManual,
    which should return the same data as the byspec */
    pmi::MSReader * reader = pmi::MSReader::Instance();
    e = reader->openFile(m_msFilePath);
    QCOMPARE(e, pmi::kNoErr);
    e = reader->getTICData(&TICDataMS);
    QCOMPARE(e, pmi::kNoErr);
    e = reader->closeFile();
    QCOMPARE(e, pmi::kNoErr);

    e = m_pwiz.getTICData(&TICDataPWIZ);
    QCOMPARE(e, pmi::kNoErr);

    const QString filename2 = QString("getScanData-scan_MS_notimplemented%1.txt").arg(2);
    std::ofstream f2(filename2.toStdString());
    for (unsigned int i = 0; i < TICDataMS.size(); i++) {
        f2 << QString("%1").arg(TICDataMS.at(i).x(), 0, 'f', 6).toStdString()
            << ", "
            << QString("%1").arg(TICDataMS.at(i).y(), 0, 'f', 6).toStdString() << std::endl;
    }
    f2.close();

    const QString filename = QString("getScanData-scan%1.txt").arg(2);
    std::ofstream f(filename.toStdString());
    for (unsigned int i = 0; i < TICDataPWIZ.size(); i++) {
        f << QString("%1").arg(TICDataPWIZ.at(i).x(), 0, 'f', 6).toStdString() 
            << ", " 
            << QString("%1").arg(TICDataPWIZ.at(i).y(), 0, 'f', 6).toStdString() << std::endl;
    }
    f.close();

    QCOMPARE(TICDataPWIZ.size(), TICDataMS.size());

    // TO-DO: Make this y-error tolerance relative to the size of the value (percentage tolerance)
    int TICDataYerrorTolerance = 100;
    for (unsigned int i = 0; i < TICDataPWIZ.size(); i++) {
        // fuzzier compare than normal fuzzy QCOMPARE
        bool verify_x = abs(TICDataPWIZ.at(i).rx() - TICDataMS.at(i).rx()) < m_comparisonTolerance;
        bool verify_y = abs(TICDataPWIZ.at(i).ry() - TICDataMS.at(i).ry()) < TICDataYerrorTolerance;
        QVERIFY(verify_x);
        QVERIFY(verify_y);
    }
}

void MSReaderAgilentCompareWithByspecTest::testProfileGetScanData() 
{
    using pmi::msreader::almostEqual;

    const int precision = 9;
    const QString filename = QString("getscanData-comp.txt");
    std::ofstream f(filename.toStdString());
    long totalNumber;
    long startScan;
    long endScan;

    m_agilent.getNumberOfSpectra(&totalNumber, &startScan, &endScan);

    std::vector<QPoint> infoDebugVector;
    bool enough = false;
    // assumes testGetNumberOfSpectra already run and the two files compare the same
    // Note: takes too long to test every scan, so increment by more than 1 to try to have test spotcheck and run faster.
    for (int scanNumber = startScan; scanNumber <= endScan; scanNumber+=10) {
        pmi::msreader::ScanInfo obj;
        m_agilent.getScanInfo(scanNumber, &obj);

        // Don't run if not profile mode
        if (obj.peakMode != pmi::msreader::PeakPickingMode::PeakPickingProfile) {
            continue;
        }

        std::cout << "end scan: " << endScan << ",scan number: " << scanNumber << std::endl;    
    
        if (enough) {
            break;
        }

        pmi::point2dList pointsMS;
        pmi::point2dList pointsPWIZ;

        pmi::Err e = m_agilent.getScanData(scanNumber, &pointsMS, true);
        QCOMPARE(e, pmi::kNoErr);

        e = m_pwiz.getScanData(scanNumber, &pointsPWIZ, true);
        //QCOMPARE(e, pmi::kNoErr);

        /*f << scanNumber << ","
            << pointsPWIZ.size() << ","
            << pointsMS.size() << ","            
            << std::endl;*/

        infoDebugVector.push_back(QPoint(static_cast<int>(pointsMS.size()), static_cast<int>(pointsPWIZ.size())));

        //pmi::point2dList beforeMS = pointsMS, beforePWIZ = pointsPWIZ;

        // significantly increases run time
        trimZeroIntensities(pointsMS);
        trimZeroIntensities(pointsPWIZ);

        //if (pointsMS.size() != pointsPWIZ.size()) {
        //    writeToCSV("E:\\jira\\_debug\\_debug\\outMS.csv", pointsMS);
        //    writeToCSV("E:\\jira\\_debug\\_debug\\outPWIZ.csv", pointsPWIZ);

        //    writeToCSV("E:\\jira\\_debug\\_debug\\beforeOutMS.csv", beforeMS);
        //    writeToCSV("E:\\jira\\_debug\\_debug\\beforeOutPWIZ.csv", beforePWIZ);
        //}
        // QCOMPARE(pointsMS.size() - firstNonZeroOffsetMS, pointsPWIZ.size() - firstNonZeroOffsetPWIZ);
        QCOMPARE(pointsMS.size(), pointsPWIZ.size());

        for (unsigned int i = 0; i < pointsMS.size(); i++) {

            const qreal x1 = pointsPWIZ.size() > i + 1 ? pointsPWIZ.at(i + 1).rx() : 0;
            const qreal x2 = pointsMS.at(i).rx();
            const qreal y1 = pointsPWIZ.size() > i + 1 ? pointsPWIZ.at(i + 1).ry() : 0;
            const qreal y2 = pointsMS.at(i).ry();

            const QString s = QString("%1,%2,%3,%4,%5,%6")
                .arg(x1, 0, 'f', precision)
                .arg(x2, 0, 'f', precision)
                .arg(std::abs(x1 - x2), 0, 'f', precision)
                .arg(y1, 0, 'f', precision)
                .arg(y2, 0, 'f', precision)
                .arg(std::abs(y1 - y2), 0, 'f', precision);

            f << s.toStdString() << std::endl;

            QCOMPARE(pointsMS.at(i).rx(), pointsPWIZ.at(i).rx());
            QCOMPARE(pointsMS.at(i).ry(), pointsPWIZ.at(i).ry());
        }
        enough = pointsMS.size() > 0;
    }
    f.close();
}

void MSReaderAgilentCompareWithByspecTest::testCentroidGetScanData() 
{
    long totalNumber;
    long startScan;
    long endScan;
    m_agilent.getNumberOfSpectra(&totalNumber, &startScan, &endScan);

    std::vector<QPoint> infoDebugVector;

    double maxXdiff = 0;
    double maxYdiff = 0;
    double maxPercentDiff = 0;

    ::pmi::point2dList maxIntensityAndNumPoints;
    ::pmi::point2dList retTimeAndScanNumber;

    ::pmi::point2dList trippedThresholdAndZero;
    for (int scanNumber = startScan; scanNumber <= endScan; scanNumber++) {
        pmi::msreader::ScanInfo obj;
        m_agilent.getScanInfo(scanNumber, &obj);

        // Don't run if not centroid mode
        if (obj.peakMode != pmi::msreader::PeakPickingMode::PeakPickingCentroid) {
            continue;
        }
        //std::cout << "scan number: " << scanNumber << std::endl;
        //std::cout << "scan time: " << obj.retTimeMinutes << std::endl;

        pmi::point2dList pointsMS;
        pmi::point2dList pointsPWIZ;

        pmi::Err e = m_agilent.getScanData(scanNumber, &pointsMS, true);
        QCOMPARE(e, pmi::kNoErr);

        e = m_pwiz.getScanData(scanNumber, &pointsPWIZ, true);

        QCOMPARE(e, pmi::kNoErr);

        infoDebugVector.push_back(QPoint(static_cast<int>(pointsMS.size()), static_cast<int>(pointsPWIZ.size())));

        pmi::point2dList beforeMS = pointsMS, beforePWIZ = pointsPWIZ;

        if (pointsMS.size() != pointsPWIZ.size()) {
            writeToCSV(m_outPath + "\\outMScent.csv", pointsMS);
            writeToCSV(m_outPath + "\\outPWIZcent.csv", pointsPWIZ);

            writeToCSV(m_outPath + "\\beforeOutMScent.csv", beforeMS);
            writeToCSV(m_outPath + "\\beforeOutPWIZcent.csv", beforePWIZ);
        }

        QCOMPARE(pointsMS.size(), pointsPWIZ.size());
        
        double maxIntensityPwiz = -1;
        bool failed = false;
        for (unsigned int i = 0; i < pointsMS.size(); i++) {
            if (pointsPWIZ.at(i).ry() > maxIntensityPwiz) {
                maxIntensityPwiz = pointsPWIZ.at(i).ry();
            }
            
            // for BMS-ADC max diff mz = 2.7292020604363643e-005, max intens = -0.5 (due to some scans rounding), max intens percent diff = 0.2483593019516076
            // the scans that round are correlated with maximum intensity for that scan. There is a certain intensity threshhold (scan by scan basis)
            // that prompts the code to process / compress the centroid pwiz data for that scan, thus leading to rounded numbers
            double episilonMz = 5.0e-005;
            double epsilonIntensity = 6e-1;
            double percentTolerance = 5e-1;
            
            QVERIFY(fuzzyCompare(pointsMS.at(i).rx(), pointsPWIZ.at(i).rx(), episilonMz));
            QVERIFY(fuzzyCompare(pointsMS.at(i).ry(), pointsPWIZ.at(i).ry(), epsilonIntensity));
            percentCompare(pointsMS.at(i).ry(), pointsPWIZ.at(i).ry(), percentTolerance);
        }
    }
 }

void MSReaderAgilentCompareWithByspecTest::testGetXICData() 
{
    
}

void MSReaderAgilentCompareWithByspecTest::testGetScanInfo() 
{
    for (int scanNumber = m_startScan; scanNumber <= m_endScan; scanNumber++) {
        pmi::msreader::ScanInfo infoMS;
        pmi::msreader::ScanInfo infoPWIZ;
        m_agilent.getScanInfo(scanNumber, &infoMS);
        m_pwiz.getScanInfo(scanNumber, &infoPWIZ);

        //QCOMPARE(infoMS.retTimeMinutes, infoPWIZ.retTimeMinutes);
        QVERIFY(abs(infoPWIZ.retTimeMinutes - infoMS.retTimeMinutes) < m_comparisonTolerance);
        QCOMPARE(infoMS.scanLevel, infoPWIZ.scanLevel); 
        QCOMPARE(infoMS.nativeId, infoPWIZ.nativeId);
        QCOMPARE(infoMS.peakMode, infoPWIZ.peakMode);
        //QCOMPARE(infoMS.scanMethod, infoPWIZ.scanMethod);
    }
}

void MSReaderAgilentCompareWithByspecTest::testGetScanPrecursorInfo() 
{
    for (int scanNumber = m_startScan; scanNumber <= m_endScan; scanNumber++) {
        pmi::msreader::PrecursorInfo precursorInfoMS;
        pmi::msreader::PrecursorInfo precursorInfoPWIZ;
        QCOMPARE(precursorInfoMS.dIsolationMass, precursorInfoPWIZ.dIsolationMass);
        QCOMPARE(precursorInfoMS.dMonoIsoMass, precursorInfoPWIZ.dMonoIsoMass);
        QCOMPARE(precursorInfoMS.nativeId, precursorInfoPWIZ.nativeId);
        QCOMPARE(precursorInfoMS.nChargeState, precursorInfoPWIZ.nChargeState);
        QCOMPARE(precursorInfoMS.nScanNumber, precursorInfoPWIZ.nScanNumber);
    }
}

void MSReaderAgilentCompareWithByspecTest::testGetFragmentType()
{
    for (int scanNumber = m_startScan; scanNumber <= m_endScan; scanNumber++) {
        pmi::msreader::ScanInfo obj;
        m_agilent.getScanInfo(scanNumber, &obj);
        // Only run test if ms level is 2, because there will only be fragment type for ms level 2
        // Should this be here? Shouldn't we also be consistent if ms level is other than 2, even if 
        // fragment type wouldn't be there? 
        if (obj.scanLevel == 2) {
            QString fragTypeMS;
            QString fragTypePWIZ;

            pmi::Err e = m_agilent.getFragmentType(scanNumber, 0, &fragTypeMS);
            QCOMPARE(e, pmi::kNoErr);
            e = m_pwiz.getFragmentType(scanNumber, 0, &fragTypePWIZ);
            QCOMPARE(e, pmi::kNoErr);

            if (fragTypePWIZ.compare(fragTypeMS) != 0) {
                std::cout << "fragtype: " << fragTypePWIZ.toStdString() << " vs " << fragTypeMS.toStdString() << std::endl;
            }
            QCOMPARE(fragTypePWIZ, fragTypeMS);
        }
    }
}

void MSReaderAgilentCompareWithByspecTest::testGetChromatograms()
{
    QList<pmi::msreader::ChromatogramInfo> chromInfoListMS;
    QList<pmi::msreader::ChromatogramInfo> chromInfoListPWIZ;

    m_pwiz.getChromatograms(&chromInfoListPWIZ);

    // For now, we are writing this test such that if pwiz returned list size is 0
    // we do not run the test. We are assuming Agilent API results are correct if
    // Agilent returns something and pwiz does not, but we want to make sure if pwiz
    // ever returns something, it is consistent with what Agilent returns.
    if (chromInfoListPWIZ.empty()) {
        return;
    }

    m_agilent.getChromatograms(&chromInfoListMS);

    QCOMPARE(chromInfoListPWIZ.size(), chromInfoListMS.size());
    for (int i = 0; i < chromInfoListPWIZ.size(); i++) {
        const pmi::msreader::ChromatogramInfo & chromInfoPWIZ = chromInfoListPWIZ.at(i);
        const pmi::msreader::ChromatogramInfo & chromInfoMS = chromInfoListMS.at(i);

        QCOMPARE(chromInfoPWIZ.channelName, chromInfoMS.channelName);
        QCOMPARE(chromInfoPWIZ.internalChannelName, chromInfoMS.internalChannelName);

        const size_t chromInfoPointsSizePWIZ = chromInfoPWIZ.points.size();
        const size_t chromInfoPointsSizeMS = chromInfoMS.points.size();
        QCOMPARE(chromInfoPointsSizePWIZ, chromInfoPointsSizeMS);

        for (unsigned int j = 0; j < chromInfoPointsSizePWIZ; j++) {
            const QPointF & chromInfoPointPWIZ = chromInfoPWIZ.points.at(i);
            const QPointF & chromInfoPointMS = chromInfoMS.points.at(i);

            QCOMPARE(chromInfoPointPWIZ, chromInfoPointMS);
        }
    }
}

void MSReaderAgilentCompareWithByspecTest::testGetLockmassScans()
{
    QList<pmi::msreader::ScanInfoWrapper> ScanInfoWrapperMS;
    QList<pmi::msreader::ScanInfoWrapper> ScanInfoWrapperPWIZ;

    m_agilent.getLockmassScans(&ScanInfoWrapperMS);
    m_pwiz.getLockmassScans(&ScanInfoWrapperPWIZ);

    QCOMPARE(ScanInfoWrapperPWIZ.size(), ScanInfoWrapperMS.size());
    for (int i = 0; i < ScanInfoWrapperPWIZ.size(); i++) {
        const pmi::msreader::ScanInfo & scanInfoMS = ScanInfoWrapperMS.at(i).scanInfo;
        const pmi::msreader::ScanInfo & scanInfoPWIZ = ScanInfoWrapperPWIZ.at(i).scanInfo;
        QCOMPARE(scanInfoPWIZ.nativeId, scanInfoMS.nativeId);
        QCOMPARE(scanInfoPWIZ.peakMode, scanInfoMS.peakMode);
        QVERIFY(abs(scanInfoPWIZ.retTimeMinutes - scanInfoMS.retTimeMinutes) < m_comparisonTolerance);
        QCOMPARE(scanInfoPWIZ.scanLevel, scanInfoMS.scanLevel);

        // Always just assume the scanMethod is ScanMethodFullScan from Agilent
        // pwiz returns ScanMethodUnknown 
        QCOMPARE(pmi::msreader::ScanMethod::ScanMethodFullScan, scanInfoMS.scanMethod);
    
        QCOMPARE(ScanInfoWrapperPWIZ.at(i).scanNumber, ScanInfoWrapperMS.at(i).scanNumber);
    }
}

void MSReaderAgilentCompareWithByspecTest::cleanupTestCase()
{
    QCOMPARE(m_agilent.closeFile(), pmi::kNoErr);
    QCOMPARE(m_pwiz.closeFile(), pmi::kNoErr);

    std::cout << "Test run time (seconds): " << double(clock() - m_startTestRunTime) / CLOCKS_PER_SEC << std::endl;
}

QTEST_MAIN(MSReaderAgilentCompareWithByspecTest);

#include "MSReaderAgilentCompareWithByspecTest.moc"
