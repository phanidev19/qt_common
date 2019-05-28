#include <QtTest>
#include "pmi_core_defs.h"

#include "MSReader.h"
#include "ScanIndexNumberConverter.h"
#include "PMiTestUtils.h"
#include "MSReaderInfo.h"
#include "NonUniformTilesSerialization.h"

#include <AdvancedSettings.h>

static const QString DM_Av_BYSPEC2 = QStringLiteral("020215_DM_Av.byspec2");
static const QString DM_AvastinEu_IA_LysN = QStringLiteral("011315_DM_AvastinEu_IA_LysN.raw");
static const QString DM_AvastinUS_IA_LysN = QStringLiteral("011315_DM_AvastinUS_IA_LysN.raw");
static const QString Agilent_18Sep01_03_d = QStringLiteral("18Sep01_03.d"); // no peak data here
static const QString ngHeLaPASEF_2min_compressed = QStringLiteral("200ngHeLaPASEF_2min_compressed.d");

static const QString PFITZER_LT4211_RAW = QStringLiteral("ES_15Jan19_NGHer2_T0_30min_pH8.raw");


static const QString TEST_DATA_DIR = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/MSReaderTest");

_PMI_BEGIN

class MSReaderTest : public QObject
{
    Q_OBJECT

public: 
    MSReaderTest(const QStringList &args) : m_testDataBasePath(args[0]) {
        m_rawFilePath = m_testDataBasePath.filePath(DM_Av_BYSPEC2);

//#define LONG_TESTS
#ifdef LONG_TESTS
        // 1200 seconds
        qputenv("QTEST_FUNCTION_TIMEOUT", QString::number(1200 * 1000).toLatin1());
#endif
    }

private:
    //! computes random xic windows 
    //! uses the dimensions of the minumum and maximum mz and minimum and maximum scan time 
    //! dimensions take from last opened MS Data file
    QVector<msreader::XICWindow> xicFromReader(MSReader * reader, int count);
    QVector<msreader::XICWindow> randomXic(double mzMin, double mzMax, double timeMin, double timeMax, int count);

private Q_SLOTS:
    void testGetXICDataFuzzy();
    void testSwitchFiles();
    void testThermoVsManualXICWindow();
    void testGetScanDataMS1Sum_SingleScan();

    void testOpenFile();
    void testOpenFromOtherThread();

    void testByspec2CacheReusing();

    void testGetTICData_data();
    void testGetTICData();

    void testGetBasePeak_data();
    void testGetBasePeak();

    void testGetScanData_data();
    void testGetScanData();

    void testCompareXICData_data();
    void testCompareXICData();

private:
    QDir m_testDataBasePath;
    QString m_rawFilePath;
};

// helper class for testOpenFromOtherThread test
class MSReaderOpenJob : public QThread
{
    Q_OBJECT
public:
    void setFilePath(const QString &filePath) { m_filePath = filePath; }

    Err status() { return m_status; }

protected:
    void run() override
    {
        if (QFileInfo(m_filePath).isReadable()) {
            MSReader *reader = MSReader::Instance();
            m_status = reader->openFile(m_filePath);
        } else {
            m_status = kError;
        }
    }

private:
    QString m_filePath;
    Err m_status = kNoErr;
};

QVector<msreader::XICWindow> MSReaderTest::xicFromReader(MSReader * reader, int count)
{
    MSReaderInfo info(reader);

    double mzMin;
    double mzMax;
    bool ok = false;
    auto minMax = info.mzMinMax(true, &ok);
    if (!ok) {
        qDebug() << "info.mzMinMax returned bad result";
    }
    mzMin = minMax.first;
    mzMax = minMax.second;
    
    QList<msreader::ScanInfoWrapper> scanInfo;
    reader->getScanInfoListAtLevel(1, &scanInfo);
    if (scanInfo.isEmpty()) {
        return QVector<msreader::XICWindow>();
    }

    double timeMin = scanInfo.first().scanInfo.retTimeMinutes;
    double timeMax = scanInfo.last().scanInfo.retTimeMinutes;


    qDebug() << "Mz min, max" << mzMin << mzMax;
    qDebug() << "Time min, max" << timeMin << timeMax;

    return randomXic(mzMin, mzMax, timeMin, timeMax, count);
}


QVector<msreader::XICWindow> MSReaderTest::randomXic(double mzMin, double mzMax, double timeMin, double timeMax, int count) {
    qsrand(0);
    // setting for the distribution of xic windows taken from Byologic + raw file 
    const double typicalMzMinLen = 0.011;
    const double typicalMzMaxLen = 0.05;
    const double typicalTimeMinLen = 11;
    const double typicalTimeMaxLen = 96;

    QVector<msreader::XICWindow> result;

    for (int i = 0; i < count; ++i) {
        double randMzPos = qrand() / double(RAND_MAX);
        double randMzLength = qrand() / double(RAND_MAX);
        double randTimePos = qrand() / double(RAND_MAX);
        double randTimeLength = qrand() / double(RAND_MAX);

        double mzLen = typicalMzMinLen + (typicalMzMaxLen - typicalMzMinLen) * randMzLength;
        double timeLen = typicalTimeMinLen + (typicalTimeMaxLen - typicalTimeMinLen) * randTimeLength;

        msreader::XICWindow win;
        win.mz_start = (mzMax - mzMin) * randMzPos + mzMin;
        win.mz_end = qMin(mzMax, win.mz_start + mzLen);

        win.time_start = (timeMax - timeMin)  * randMzPos + timeMin;
        win.time_end = qMin(timeMax, win.time_start + timeLen);
        
        result.push_back(win);
    }

    return result;
}

void MSReaderTest::testGetXICDataFuzzy()
{
    MSReader * reader = MSReader::Instance();
    static const int NUMBER_OF_XICs_PER_FILE = 20;

    QStringList rawPaths = { m_rawFilePath, };
    QStringList failedFiles;
    for (int id = 0; id < rawPaths.size(); ++id) {
        QString rawFilePath = rawPaths.at(id);

        QVERIFY(QFileInfo(rawFilePath).exists());
        QCOMPARE(reader->openFile(rawFilePath), kNoErr);

        QVector<msreader::XICWindow> xicWindows = xicFromReader(reader, NUMBER_OF_XICs_PER_FILE);
        
        QVERIFY(!xicWindows.isEmpty());
        qDebug() << "Testing" << xicWindows.size() << "XIC windows for file" << rawFilePath;

        int successful = xicWindows.size();
        point2dList times;
        QString rawBaseName = QFileInfo(rawFilePath).baseName();
        int xicWindowIndex = 0;
        for (const msreader::XICWindow &window : xicWindows) {
            xicWindowIndex++;

            // NonUniform cache enabled
            reader->setXICDataMode(MSReader::XICModeTiledCache);
            QElapsedTimer et;
            et.start();
            point2dList actual;
            QCOMPARE(reader->getXICData(window, &actual), kNoErr);
            qreal cacheTime = et.elapsed();

            // Compare with byspec file
            reader->setXICDataMode(MSReader::XICModeManual);
            et.start();
            point2dList expected;
            QCOMPARE(reader->getXICData(window, &expected), kNoErr);
            qreal rawTime = et.elapsed();
            times.push_back(QPointF(cacheTime, rawTime));

//#define DEBUG_XIC_WINDOWS
#ifdef DEBUG_XIC_WINDOWS  
            qDebug() << "Testing window" << window.mz_start << "," << window.mz_end << "returned" << actual.size() << "points";
#endif
            if (actual != expected) {
                successful--;

                QString filePath = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/MSReaderTest/%1_%2_nonUniform.csv").arg(rawBaseName).arg(xicWindowIndex);
                PlotBase(actual).saveDataToFile(filePath);

                filePath = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/MSReaderTest/%1_%2_actual.csv").arg(rawBaseName).arg(xicWindowIndex);
                PlotBase(expected).saveDataToFile(filePath);

                qWarning() << "Failed for window" << xicWindowIndex << "val:" << window.mz_start << window.mz_end << window.time_start << window.time_end;
            }
        }

        QString filePathTimes = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/MSReaderTest/%1_Times.csv").arg(rawBaseName);
        PlotBase(times).saveDataToFile(filePathTimes);
        if (successful < xicWindows.size()) {
            failedFiles << rawFilePath;
        }
    }

    if (!failedFiles.isEmpty()) {
        qDebug() << "Failed raw files:";
        qDebug() << failedFiles;
    }

    QVERIFY(failedFiles.isEmpty());
    reader->releaseInstance();
}

void MSReaderTest::testCompareXICData_data()
{
    // Dumps are binary files in LittleEndian format with doubles:
    // first 4 doubles are parameters of window
    // <xic_win.time_start> <xic_win.time_end> <xic_win.mz_start> <xic_win.mz_end>
    // pairs of doubles with XY data are followed after parameters

    QTest::addColumn<QString>("msFilePath");
    QTest::addColumn<QString>("dumpPath");

    QTest::addRow("Thermo-ES_15Jan19_NGHer2_T0_30min_pH8")
        << m_testDataBasePath.filePath(PFITZER_LT4211_RAW)
        << m_testDataBasePath.filePath(PFITZER_LT4211_RAW + ".dump1");
    QTest::addRow("Thermo-ES_15Jan19_NGHer2_T0_30min_pH8")
        << m_testDataBasePath.filePath(PFITZER_LT4211_RAW)
        << m_testDataBasePath.filePath(PFITZER_LT4211_RAW + ".dump2");
    QTest::addRow("Thermo-ES_15Jan19_NGHer2_T0_30min_pH8")
        << m_testDataBasePath.filePath(PFITZER_LT4211_RAW)
        << m_testDataBasePath.filePath(PFITZER_LT4211_RAW + ".dump3");
    QTest::addRow("Thermo-ES_15Jan19_NGHer2_T0_30min_pH8")
        << m_testDataBasePath.filePath(PFITZER_LT4211_RAW)
        << m_testDataBasePath.filePath(PFITZER_LT4211_RAW + ".dump4");
}

void MSReaderTest::testCompareXICData()
{
    QFETCH(QString, msFilePath);
    QVERIFY(QFileInfo::exists(msFilePath));
    QFETCH(QString, dumpPath);
    QVERIFY(QFileInfo::exists(dumpPath));

    MSReader *reader = MSReader::Instance();
    Err e = reader->openFile(msFilePath);
    QCOMPARE(e, kNoErr);

    msreader::XICWindow xic_win;

    QFile dumpFile(dumpPath);
    QVERIFY(dumpFile.open(QIODevice::ReadOnly));
    QDataStream fileStream(&dumpFile);
    fileStream.setByteOrder(QDataStream::LittleEndian);

    QVector<double> dumpContents;
    for (;;) {
        double tempValue;
        fileStream.startTransaction();
        fileStream >> tempValue;
        if (!fileStream.commitTransaction()) {
            break;
        }
        dumpContents << tempValue;
    }

    // see comment in testCompareXICData_data for understanding constants
    QVERIFY(dumpContents.size() >= 4);
    QVERIFY(dumpContents.size() % 2 == 0);

    xic_win.time_start = dumpContents[0];
    xic_win.time_end = dumpContents[1];
    xic_win.mz_start = dumpContents[2];
    xic_win.mz_end = dumpContents[3];

    dumpContents.remove(0, 4);

    point2dList expected;
    for (int i = 0; i < dumpContents.size(); i += 2) {
        expected.push_back(QPointF(dumpContents[i], dumpContents[i + 1]));
    }

    point2dList data;
    e = reader->getXICData(xic_win, &data);
    QCOMPARE(e, kNoErr);

    QCOMPARE(data, expected);

    reader->releaseInstance();
}


void MSReaderTest::testSwitchFiles()
{
    QString path1 = m_testDataBasePath.filePath(DM_AvastinEu_IA_LysN);
    QString path2 = m_testDataBasePath.filePath(DM_AvastinUS_IA_LysN);

    MSReader * reader = MSReader::Instance();
    
    QCOMPARE(reader->openFile(path1), kNoErr);

    QList<msreader::ScanInfoWrapper> lockmassList1;
    reader->getScanInfoListAtLevel(1, &lockmassList1);
    
    QCOMPARE(reader->openFile(path2), kNoErr);

    QList<msreader::ScanInfoWrapper> lockmassList2;
    reader->getScanInfoListAtLevel(1, &lockmassList2);

    QVERIFY(lockmassList1.size() != lockmassList2.size());
}


void MSReaderTest::testThermoVsManualXICWindow()
{
    // This test demonstrates difference between XICs from Thermo and Manual/Tiled
    msreader::XICWindow xicSame;
    xicSame.mz_start = 968.23982829479098;
    xicSame.mz_end = 968.27468555604105;
    xicSame.time_start = 79.864803451733323;
    xicSame.time_end = 87.981628249132598;

    msreader::XICWindow xicDifferent;
    xicDifferent.mz_start = 968.83953128665098;
    xicDifferent.mz_end = 968.87441013759701;
    xicDifferent.time_start = 79.914151865866700;
    xicDifferent.time_end = 87.981628249132598;

    // if you use xicSame here, XIC output is the same for both vendor and manual
    msreader::XICWindow window = xicDifferent;

    MSReader * reader = MSReader::Instance();
    QCOMPARE(reader->openFile(m_testDataBasePath.filePath(DM_AvastinEu_IA_LysN)), kNoErr);

    // fetch xic with vendor
    reader->setXICDataMode(MSReader::XICModeVendor);
    point2dList vendorXic;
    QCOMPARE(reader->getXICData(window, &vendorXic), kNoErr);

    // fetch xic manually
    reader->setXICDataMode(MSReader::XICModeManual);
    point2dList manualXic;
    QCOMPARE(reader->getXICData(window, &manualXic), kNoErr);

    if (vendorXic != manualXic) {
        QString debugFilesPathPattern = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/MSReaderTest/%1_XIC.csv");

        PlotBase(vendorXic).saveDataToFile(debugFilesPathPattern.arg("vendor"));
        PlotBase(manualXic).saveDataToFile(debugFilesPathPattern.arg("manual"));
        
    }
    
    QEXPECT_FAIL("", "Thermo provides different XIC compared to manual/Tiled method for some windows", Continue);
    QVERIFY(vendorXic == manualXic);

    // IMPORTANT: reset reader for except influence to another tests.
    reader->releaseInstance();
}

void MSReaderTest::testGetScanDataMS1Sum_SingleScan()
{
    static const QString DM_PL2_REDUCED_QB_ONE_SCAN = QStringLiteral("112014_DM_PL2_reduced-qb-one-scan.raw");

    const QString rawFilePath = m_testDataBasePath.filePath(DM_PL2_REDUCED_QB_ONE_SCAN);
    QString ms1CachePath = rawFilePath + QString(".ms1.cache");
    if (QFile::exists(ms1CachePath)) {
        qDebug() << "MS1 cache exists! Removing it so that it can be re-created";
        QFile::remove(ms1CachePath);
    }

    MSReader * reader = MSReader::Instance();
    QCOMPARE(reader->openFile(rawFilePath), kNoErr);

    double SCAN_TIME = 26.608173237333; // taken from test file using SeeMS app
    long scanNumber;
    QCOMPARE(reader->getBestScanNumber(1, SCAN_TIME, &scanNumber), kNoErr);

    GridUniform grid;
    QCOMPARE(reader->getScanDataMS1Sum(SCAN_TIME, SCAN_TIME, &grid, nullptr), kNoErr);

    // sum of scans {1} should return that one scan instead of sum of {}
    // construct expected sum here
    point2dList scanData;
    QCOMPARE(reader->getScanData(scanNumber, &scanData), kNoErr);
    
    GridUniform expectedGrid;
    double mz_bin_space = 0.005;
    double mzStart = scanData.front().x();
    double mzEnd = scanData.back().x();
    expectedGrid.initGridByMzBinSpace(mzStart, mzEnd, mz_bin_space);
    expectedGrid.accumulate(scanData);

    // convert to floats, we do that in order to save space? looses some precision
    QByteArray hereAndThere;
    expectedGrid.toByteArray_float(hereAndThere);
    expectedGrid.fromByteArray_float(hereAndThere);

    QCOMPARE(grid.getSum(), expectedGrid.getSum());
}

void MSReaderTest::testByspec2CacheReusing()
{
    const QString bTimsFilePath = m_testDataBasePath.filePath(ngHeLaPASEF_2min_compressed);
    QString byspec2CachePath = bTimsFilePath + QString(".byspec2");
    if (QFile::exists(byspec2CachePath)) {
        qDebug() << "byspec2 cache exists! Removing it so that it can be re-created";
        QFile::remove(byspec2CachePath);
    }

    MSReader *reader = MSReader::Instance();

    // open file
    QCOMPARE(reader->openFile(bTimsFilePath), kNoErr);

    // check cache existence
    QVERIFY(QFile::exists(byspec2CachePath));

    // close file
    QCOMPARE(reader->closeFile(), kNoErr);

    // check cache existence
    QVERIFY(QFile::exists(byspec2CachePath));

    const QDateTime createdExpected = QFileInfo(byspec2CachePath).created();
    const QDateTime lastModifiedExpected = QFileInfo(byspec2CachePath).lastModified();

    // open file again
    QCOMPARE(reader->openFile(bTimsFilePath), kNoErr);

    // check cache existence
    QVERIFY(QFile::exists(byspec2CachePath));

    const QDateTime createdActual = QFileInfo(byspec2CachePath).created();
    const QDateTime lastModifiedActual = QFileInfo(byspec2CachePath).lastModified();

    // check cache date-times
    QCOMPARE(createdActual, createdExpected);
    QCOMPARE(lastModifiedActual, lastModifiedExpected);

    // close file
    QCOMPARE(reader->closeFile(), kNoErr);

    // check cache existence
    QVERIFY(QFile::exists(byspec2CachePath));
}

void MSReaderTest::testOpenFile()
{
    const QString msFilePath = m_testDataBasePath.filePath(DM_AvastinEu_IA_LysN);
    QVERIFY(QFileInfo(msFilePath).exists());

    MSReader *reader = MSReader::Instance();
    QCOMPARE(reader->openFile(msFilePath), kNoErr);
}

void MSReaderTest::testOpenFromOtherThread()
{
    // test settings
    const int MS1_SCAN_COUNT = 5683;
    const QString msFilePath = m_testDataBasePath.filePath(DM_AvastinEu_IA_LysN);
    
    QVERIFY(QFileInfo(msFilePath).exists());

    // open MS file in different thread
    MSReaderOpenJob job;
    job.setFilePath(msFilePath);
    job.start();
    QVERIFY(job.wait());

    if (job.status() == kNoErr) {
        // reason: I guess COM components are initialized per process just once
        QSKIP("This test works only when run without previous MSReader tests");
    }

    // it fails to read the file in different thread, so there is error
    // error message says "Could not use ACTCTX. Attempting to load instance without it"
    QCOMPARE(job.status(), kNoErr);

    // it works if we open the file in the main thread
    MSReader *reader = MSReader::Instance();
    QCOMPARE(reader->openFile(msFilePath), kNoErr);

    QList<msreader::ScanInfoWrapper> lockmassList;
    reader->getScanInfoListAtLevel(1, &lockmassList);
    QCOMPARE(lockmassList.size(), MS1_SCAN_COUNT);
}

void MSReaderTest::testGetTICData_data()
{
    QTest::addColumn<QString>("msDatafilePath");
    QTest::addColumn<int>("expectedSize");
    QTest::addColumn<QString>("expectedContentMD5Checksum");

    QTest::newRow("Thermo") << m_testDataBasePath.filePath(DM_AvastinEu_IA_LysN) << 5683 << "72181fe01f2ca0646c933fce30ce3954";
    
    as::setValue("MsReaderAgilent/TICDataManualFallback", false);

    QTest::newRow("Agilent-SS-837") << m_testDataBasePath.filePath(Agilent_18Sep01_03_d) << 4668
        << "892970ea696267000bf41146f8c15fdc";

    QTest::newRow("Agilent") << QString(
                                    R"(P:/PMI-Dev/Managed/Data/vendordata/Agilent/Cal_01_Test.d)")
                             << 33029 << "17b82636619840ec970b078500e633f9";

    QTest::newRow("BrukerTims") << m_testDataBasePath.filePath(ngHeLaPASEF_2min_compressed) << 130
                                << "486df49ea4fccb1aff2ef8b25991e60a";

#ifdef ENABLE_LONG_TESTS
    // Bruker vendor is still very slow
    QTest::newRow("Bruker")
        << QString(
               R"(P:\PMI-Dev\Managed\Data\VendorData\Agilent\L argeSlowWithUV\Impact_U5_BSA-ADH_210218_10_RB7_01_773.d)")
        << 4648;
#endif
}

QString computeMD5checkSum(const point2dList &data)
{
    QByteArray blob
        = NonUniformTilesSerialization::serializeVector(QVector<point2d>::fromStdVector(data));

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(blob);
    QString md5Checksum = QString::fromLatin1(hash.result().toHex());

    return md5Checksum;
}

void MSReaderTest::testGetTICData()
{
    // test settings
    QFETCH(QString, msDatafilePath);
    const QFileInfo fi(msDatafilePath);
    if (!fi.exists()) {
        QString msg = QString("File %1 is missing. Skipping test!").arg(msDatafilePath);
        QSKIP(qPrintable(msg));
    }

    MSReader *reader = MSReader::Instance();
    QElapsedTimer et;
    et.start();
    QCOMPARE(reader->openFile(msDatafilePath), kNoErr);
    qDebug() << "openFile done in " << et.restart();
    
    point2dList ticData;

    et.start();
    QCOMPARE(reader->getTICData(&ticData), kNoErr);
    qDebug() << "getTICData done in " << et.elapsed() << "ms";

    QVERIFY(!ticData.empty());

    QFETCH(QString, expectedContentMD5Checksum);
    QString md5Checksum = computeMD5checkSum(ticData);

    if (md5Checksum != expectedContentMD5Checksum) {
        QDir outDir(TEST_DATA_DIR);
        if (!outDir.exists()) {
            outDir.mkdir(TEST_DATA_DIR);
        }
        QString dstFilePath = outDir.filePath(QString("%1.csv").arg(md5Checksum));
        qDebug() << "Saving actual data to:" << dstFilePath;
        Err e = PlotBase(ticData).saveDataToFile(dstFilePath);
        if (e != kNoErr) {
            qWarning() << "Failed to save the output!" << e;
        }
    }

    QCOMPARE(md5Checksum, expectedContentMD5Checksum);

    QFETCH(int, expectedSize);
    QCOMPARE(static_cast<int>(ticData.size()), expectedSize);
}

void MSReaderTest::testGetBasePeak_data()
{
    QTest::addColumn<QString>("msDatafilePath");
    QTest::addColumn<int>("expectedSize");
    QTest::addColumn<QString>("expectedContentMD5Checksum");

    // if you want to test the fallback to compare content, set to true
    as::setValue(QStringLiteral("MsReaderAgilent/BasePeakManualFallback"), false);
    QTest::newRow("Agilent-SS-837") << m_testDataBasePath.filePath(Agilent_18Sep01_03_d) << 4668
                                << "e886fa8484b8f62d26480a80d8cbafcd";
}

void MSReaderTest::testGetBasePeak()
{
    QFETCH(QString, msDatafilePath);
    const QFileInfo fi(msDatafilePath);
    if (!fi.exists()) {
        QString msg = QString("File %1 is missing. Skipping test!").arg(msDatafilePath);
        QSKIP(qPrintable(msg));
    }

    MSReader *reader = MSReader::Instance();
    QElapsedTimer et;
    et.start();
    QCOMPARE(reader->openFile(msDatafilePath), kNoErr);
    qDebug() << "openFile done in " << et.restart();

    point2dList basePeak;

    et.start();
    QCOMPARE(reader->getBasePeak(&basePeak), kNoErr);
    qDebug() << "getBasePeak done in " << et.elapsed() << "ms";

    QVERIFY(!basePeak.empty());

    QFETCH(QString, expectedContentMD5Checksum);
    QString md5Checksum = computeMD5checkSum(basePeak);

    if (md5Checksum != expectedContentMD5Checksum) {
        QDir outDir(TEST_DATA_DIR);
        if (!outDir.exists()) {
            outDir.mkdir(TEST_DATA_DIR);
        }
        QString dstFilePath = outDir.filePath(QString("%1.csv").arg(md5Checksum));
        qDebug() << "Saving actual data to:" << dstFilePath;
        Err e= PlotBase(basePeak).saveDataToFile(dstFilePath);
        if (e != kNoErr) {
            qWarning() << "Failed to save the output!" << e;
        }
    }

    QCOMPARE(md5Checksum, expectedContentMD5Checksum);

    QFETCH(int, expectedSize);
    QCOMPARE(static_cast<int>(basePeak.size()), expectedSize);
}

void MSReaderTest::testGetScanData_data()
{
    QTest::addColumn<QString>("msDatafilePath");
    QTest::addColumn<QVector<long>>("scanNumbers");
    QTest::addColumn<QVector<point2dList::size_type>>("expectedSizes");

    QTest::newRow("Agilent-SS-837") << m_testDataBasePath.filePath(Agilent_18Sep01_03_d)
                                << QVector<long>{ 154, 3123, 3291 }
                                << QVector<point2dList::size_type>{ 5, 4210, 4259 };
}

void MSReaderTest::testGetScanData()
{
    QFETCH(QString, msDatafilePath);
    const QFileInfo fi(msDatafilePath);
    if (!fi.exists()) {
        QString msg = QString("File %1 is missing. Skipping test!").arg(msDatafilePath);
        QSKIP(qPrintable(msg));
    }

    MSReader *reader = MSReader::Instance();
    QElapsedTimer et;
    et.start();
    QCOMPARE(reader->openFile(msDatafilePath), kNoErr);
    qDebug() << "openFile done in " << et.restart();
    
    et.start();

    QFETCH(QVector<long>, scanNumbers);
    QFETCH(QVector<point2dList::size_type>, expectedSizes);
    
    // verify input test data 
    QVERIFY(scanNumbers.size() == expectedSizes.size());

    for (int i = 0; i < scanNumbers.size(); ++i) {
        point2dList scanData;
        Err e = reader->getScanData(scanNumbers.at(i), &scanData, true);
        QVERIFY(e == kNoErr);
        QCOMPARE(scanData.size(), expectedSizes.at(i));
    }
    
    qDebug() << "getScanData done in " << et.elapsed() << "ms";
}


_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::MSReaderTest, QStringList() << "Remote Data Folder")



#include "MSReaderTest.moc"
