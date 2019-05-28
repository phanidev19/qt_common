/*
 * Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Lukas Tvrdy ltvrdy@milosolutions.com
 */

#include <QtTest>
#include "pmi_core_defs.h"
#include "MSReader.h"

#include "CsvReader.h"
#include "MSReaderTypes.h"
#include "PMiTestUtils.h"

// Note: some vendor files here are pretty big, but they serve for the test quite well
// I made them optional, this test can be enabled on demand instead (but requires P drive mapped in your OS)
// WARNING: for testGetXICDataFromList test you need to unzip
//          the file P:/PMI-Dev/JIRA/ML-2251-Fix-cache-creation/DS109224_R-C2_01_2017.d.7z
//#define PMI_ENABLE_P_DRIVE_TESTS

_PMI_BEGIN

// vendor files 
static const QString CONA_RAW = QStringLiteral("cona_tmt0saxpdetd.raw");
static const QString PFITZER_LT4211_RAW = QStringLiteral("ES_15Jan19_NGHer2_T0_30min_pH8.raw");

// CSV files with XICs 
static const QString CONA_XIC_CSV = QStringLiteral("cona_tmt0saxpdetd.csv");
static const QString PFITZER_LT4211_XIC_CSV = QStringLiteral("ES_15Jan19_NGHer2_T0_30min_pH8.csv");

class MSReaderBenchmark : public QObject
{
    Q_OBJECT
public:
    explicit MSReaderBenchmark(const QStringList &args);

private Q_SLOTS:
    
    void testGetScanData_data();
    void testGetScanData();
    
    void testGetXICData();

    void testGetXICDataFromList_data();
    void testGetXICDataFromList();

private:
    QVector<msreader::XICWindow> xicsFromCSV(const QString &filePath);

private:
    MSReader * initReader();
    void cleanUpReader(MSReader * ms);

    QDir m_testDataBasePath;
    QString m_testFile;
};


MSReaderBenchmark::MSReaderBenchmark(const QStringList &args) :m_testDataBasePath(args[0])
{
// #define LONG_TESTS
#ifdef LONG_TESTS
// 1600 seconds
    qputenv("QTEST_FUNCTION_TIMEOUT", QString::number(1600 * 1000).toLatin1());
#endif
}

void MSReaderBenchmark::testGetScanData_data()
{
    QTest::addColumn<QString>("msDatafilePath");
    QTest::newRow("CONA") << m_testDataBasePath.filePath(CONA_RAW);
    

    // you can add paths to your raw files here, few files I tested:
    /*
    QTest::newRow("DM_BioS3") << R"(c:\PMI-Test-Data\Data\MS-Avas\MS\020215_DM_BioS3_trp_Glyco.raw)";
    QTest::newRow("DM_BioS3-byspec") << R"(c:\PMI-Test-Data\Data\MS-Avas\MS\020215_DM_BioS3_trp_Glyco.byspec2)";
    
    QTest::newRow("CONA-byspec") << R"(c:\PMI-Test-Data\Data\MS-Avas\MS\cona_tmt0saxpdetd.byspec2)";
    QTest::newRow("Waters") << R"(c:\work\pmi\jira\ML-404\Q4_\intact_comparison\mAb_Reduced.raw)";
    */
}

void MSReaderBenchmark::testGetScanData()
{
    MSReader * reader = initReader();
    QVERIFY(reader != nullptr);

    QFETCH(QString, msDatafilePath);
    QFileInfo fi(msDatafilePath);
    QVERIFY(fi.exists());
    QCOMPARE(reader->openFile(msDatafilePath), kNoErr);

    qDebug() << "Input test file name: " << fi.fileName();

    // get all scan numbers first
    QList<msreader::ScanInfoWrapper> lockmassList;
    reader->getScanInfoListAtLevel(1, &lockmassList);

    QVector<int> scanNumbers;
    for (const msreader::ScanInfoWrapper &item : lockmassList) {
        scanNumbers << item.scanNumber;
    }
    QVERIFY(!scanNumbers.isEmpty());
    qDebug() << "Scan number count" << scanNumbers.size();
    
    // benchmark getScanData
    QBENCHMARK {
        qint64 pointCount = 0;
        for (int scanNumber : scanNumbers) {
            point2dList data;
            Err e = reader->getScanData(scanNumber, &data);
            if (data.empty()) {
                qWarning() << "Empty scan number" << scanNumber;
            }
            pointCount += data.size();
        }
        qDebug() << "Point Count" << pointCount;
    }
        
    cleanUpReader(reader);
}

void MSReaderBenchmark::testGetXICData()
{
    MSReader * reader = initReader();
    QVERIFY(reader != nullptr);

    const QString msDatafilePath = m_testDataBasePath.filePath(CONA_RAW);
    QVERIFY(QFileInfo(msDatafilePath).exists());
    QCOMPARE(reader->openFile(msDatafilePath), kNoErr);

    msreader::XICWindow xic_win;
    // TODO: how to iterate the XIC windows to measure performance?

    xic_win.mz_start = 380.001;
    xic_win.mz_end = 720.001;
    xic_win.time_start = 0.02;
    xic_win.time_end = 9.6386;

    point2dList data;
    Err e = reader->getXICData(xic_win, &data);
    QCOMPARE(e, kNoErr);

    cleanUpReader(reader);
}

void MSReaderBenchmark::testGetXICDataFromList_data()
{

    // This test benchmarks how fast MSReader::getXICData performs within Byologic project creation
    // Byologic caches the XICs in the project file, so this test consists of vendor file and CSV file 
    // with XICs dumped from Byologic project

    QTest::addColumn<QString>("msFilePath");
    QTest::addColumn<QString>("csvFilePath");

    QTest::addRow("Thermo-ES_15Jan19_NGHer2_T0_30min_pH8")
        << m_testDataBasePath.filePath(PFITZER_LT4211_RAW)
        << m_testDataBasePath.filePath(PFITZER_LT4211_XIC_CSV);

    QTest::addRow("Thermo-cona_tmt0saxpdetd.raw")
        << m_testDataBasePath.filePath(CONA_RAW) << m_testDataBasePath.filePath(CONA_XIC_CSV);

#ifdef PMI_ENABLE_P_DRIVE_TESTS
    QString shimadzu_XIC_CSV = R"(P:/PMI-Dev/J/LT/4200/LT-4211/tests/shimadzu/Sample D_TryDig100 DTT_IAA_2x_001.csv)";
    QString shimadzu_VENDOR = R"(P:/PMI-Dev/J/LT/4200/LT-4211/Shimadzu/Stephen kurzyniec - Sample D_TryDig100 DTT_IAA_2x_001.lcd)";
    QTest::addRow("Shimadzu-Kurzyniec") << shimadzu_VENDOR << shimadzu_XIC_CSV;

    QString agilent_XIC_CSV = R"(P:/PMI-Dev/J/LT/4200/LT-4211/tests/agilent/ecoli-0500-r001.csv)";
    QString agilent_VENDOR = R"(P:/PMI-Dev/JIRA/LT-691/Byologic/Agilent/ecoli-0500-r001.d)";
    QTest::addRow("Agilent-ecoli") << agilent_VENDOR<< agilent_XIC_CSV;
    
    // zipped at P:/PMI-Dev/JIRA/ML-2251-Fix-cache-creation
    QString bruker_XIC_CSV = R"(P:/PMI-Dev/J/LT/4200/LT-4211/tests/bruker/DS109224_R-C2_01_2017.csv)";
    QString bruker_VENDOR = R"(P:/PMI-Dev/JIRA/ML-2251-Fix-cache-creation/DS109224_R-C2_01_2017.d)";
    QTest::addRow("Bruker-DS") << bruker_VENDOR << bruker_XIC_CSV;

    //ABI 
    QString abi_XIC_CSV = R"(P:/PMI-Dev/J/LT/4200/LT-4211/tests/abi/iPRG_2012.csv)";
    QString abi_VENDOR = R"(P:/PMI-Dev/JIRA/lt-691/Byologic/Ab Sciex/iPRG_2012.wiff)";
    QTest::addRow("ABI-iPRG_2012") << abi_VENDOR<< abi_XIC_CSV;

    // Waters
    QString waters_XIC_CSV = R"(P:/PMI-Dev/J/LT/4200/LT-4211/tests/waters/20141211_ADM_Xevo_PepMap_27.csv)";
    QString waters_VENDOR = R"(P:/PMI-Dev/Share/For_Lukas/20141211_ADM_Xevo_PepMap_27.raw)";
    QTest::addRow("Waters-Xevo_PepMap") << waters_VENDOR << waters_XIC_CSV;
#endif

}

void MSReaderBenchmark::testGetXICDataFromList()
{
    QFETCH(QString, msFilePath);
    QFETCH(QString, csvFilePath);

    QVERIFY(QFileInfo::exists(msFilePath));
    QVERIFY(QFileInfo::exists(csvFilePath));

    MSReader *ms = MSReader::Instance();
    Err e = ms->openFile(msFilePath);
    QCOMPARE(e, kNoErr);

    QVector<msreader::XICWindow> xics = xicsFromCSV(csvFilePath);
    qDebug() << "Testing" << xics.size() << "windows.";

    QVERIFY(!xics.isEmpty());

    QElapsedTimer et;
    et.start();

    for (const msreader::XICWindow &win : xics) {
        point2dList data;
        e = ms->getXICData(win, &data);
        QCOMPARE(e, kNoErr);
    }
    qDebug() << "Done" << et.elapsed() << "ms";

    ms->closeFile();
}

QVector<msreader::XICWindow> MSReaderBenchmark::xicsFromCSV(const QString &filePath)
{
    QVector<msreader::XICWindow> result;

    CsvReader reader(filePath);
    if (!reader.open()) {
        qWarning() << "Cannot open file" << filePath;
        return result;
    }

    QList<QStringList> data;
    reader.readAllRows(&data);

    if (data.isEmpty()) {
        qWarning() << "Empty CSV file!";
        return result;
    }

    const QStringList header = data.first();
    const int mzStartIndex = header.indexOf(QStringLiteral("mzStart"));
    const int mzEndIndex = header.indexOf(QStringLiteral("mzEnd"));
    const int timeStartIndex = header.indexOf(QStringLiteral("timeStart"));
    const int timeEndIndex = header.indexOf(QStringLiteral("timeEnd"));

    if (mzStartIndex == -1 || mzEndIndex == -1 || timeStartIndex == -1 || timeEndIndex == -1) {
        qWarning() << "Invalid CSV format!";
        return result;
    }

    for (int i = 1; i < data.size(); ++i) {
        QStringList row = data.at(i);

        msreader::XICWindow win;

        win.mz_start = row[mzStartIndex].toDouble();
        win.mz_end = row[mzEndIndex].toDouble();

        win.time_start = row[timeStartIndex].toDouble();
        win.time_end = row[timeEndIndex].toDouble();

        result.push_back(win);
    }

    return result;
}

void MSReaderBenchmark::cleanUpReader(MSReader * ms)
{
    ms->releaseInstance();
    ms = nullptr;
}

MSReader * pmi::MSReaderBenchmark::initReader()
{
    return MSReader::Instance();
}


_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::MSReaderBenchmark, QStringList() << "Remote Data Folder")

#include "MSReaderBenchmark.moc"
