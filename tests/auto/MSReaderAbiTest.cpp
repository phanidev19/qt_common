/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include <AdvancedSettings.h>
#include <MSReader.h>
#include <MSReaderAbi.h>
#include <PMiTestUtils.h>

using namespace pmi;
using namespace pmi::msreader;

static QString s_currentDataFile;

struct RunData {
    XICWindow xicWwin;
    int scanInfoNumber;
    double bestScanNumberTime;
    int getScanDataScanNumber;
    int getScanDataDoCentroiding;
    int getScanPrecursorInfoScanNumber;
};

struct ExpectedDataItem {
    struct {
        long totalNumber;
        long startScan;
        long endScan;
    } numberOfSpectra;
    long scanPointListSize;
    long uvPointListSize;
    long ticPointListSize;
    long xicPointListSize;
    ScanInfo scanInfo;
    long bestScanNumberLevel1;
    long bestScanNumberLevel2;
    PrecursorInfo precursorInfo;
    QSet<QString> chromatogramsIds;
};

static QMap<QString, QList<RunData>> s_runData
    = { { "OriginalMAb/OriginalMAb.wiff",
          { { XICWindow(), 200, 1.458, 200, false, 69000 },
            { XICWindow(1, 100, 20, 100), 50000, 1.300, 198, true, 200 } } } };

static QMap<QString, QList<ExpectedDataItem>> s_expectedData
    = { { "OriginalMAb/OriginalMAb.wiff",
          { { { 268336, 1, 268336 },
              74,
              -1,
              16771,
              16771,
              { 0.9718833333, 1, "sample=1 period=1 cycle=200 experiment=1", PeakPickingProfile,
                ScanMethodFullScan },
              299,
              17071,
              { PrecursorInfo(1916, 0, 0, 0, "sample=1 period=1 cycle=1916 experiment=1") },
              {"tic;s=0", "xic;s=0"} },
            { { 268336, 1, 268336 },
              7,
              -1,
              16771,
              5813,
              { 157.7088000, 2, "sample=1 period=1 cycle=16458 experiment=3", PeakPickingProfile,
                ScanMethodZoomScan },
              266,
              17038,
              { PrecursorInfo(-1, 0, 0, 1, "") },
              { "tic;s=0", "xic;s=0" } } } } };

class MSReaderAbiTest : public QObject
{
    Q_OBJECT
public:
    explicit MSReaderAbiTest(const QStringList &args)
        : QObject()
        , m_args(args)
    {
        as::setValue("MSReader/UseABIGetBestScanNumber", true);
        as::setValue("MSReader/UseABIGetBestScanNumberLevel2", true);
    }

    ~MSReaderAbiTest() {}

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testChromatogramCanonicalId();

    void testIsAbiPath();
    void testOpenDirect();
    void testOpenViaMSReader();
    void testMSReaderInstance();
    void testGetXICData();
    void testGetTICData();
    void testGetBasePeakData();
    void testGetScanData();
    void testGetNumberOfSpectra();
    void testGetScanPrecursorInfo();
    void testGetScanInfo();
    void testGetBestScanNumber();
    void testGetChromatograms();

private:
    QString path() const;
    void prepareTestPaths();

    void compareChromatogramCanonicalId(const MSReaderAbi::ChromatogramCanonicalId &id,
                                        const QString &expectedString,
                                        bool compartibilityTesting = false, bool inverse = false);

private:
    const QStringList m_args;
    QString m_path;
    QSharedPointer<MSReaderAbi> m_abiReader;
};

void MSReaderAbiTest::initTestCase()
{
    prepareTestPaths();

    m_abiReader.reset(new MSReaderAbi());

    QCOMPARE(m_abiReader->openFile(path()), kNoErr);
}

void MSReaderAbiTest::testMSReaderInstance()
{
    MSReader *ms = MSReader::Instance();
    QVERIFY(ms);
}

void MSReaderAbiTest::cleanupTestCase()
{
    if (m_abiReader.data() != nullptr) {

        m_abiReader->closeFile();
    }
}

void MSReaderAbiTest::testIsAbiPath()
{
    QVERIFY(m_abiReader->canOpen(path()));
    QVERIFY(!m_abiReader->canOpen(path() + ".somethingelse"));
}

void MSReaderAbiTest::testOpenDirect()
{
    QFileInfo fi(path());
    QVERIFY(fi.isFile());
    QVERIFY(fi.exists());

    MSReaderAbi reader;
    QCOMPARE(reader.classTypeName(), MSReaderBase::MSReaderClassTypeABSCIEX);
    QVERIFY(reader.canOpen(path()));
    QCOMPARE(reader.openFile(path()), kNoErr);
    QVERIFY(!reader.getFilename().isEmpty());
    QCOMPARE(reader.closeFile(), kNoErr);
}

void MSReaderAbiTest::testOpenViaMSReader()
{
    QFileInfo fi(path());
    QVERIFY(fi.isFile());
    QVERIFY(fi.exists());

    MSReader *ms = MSReader::Instance();
    QVERIFY(ms);
    QVERIFY(!ms->isOpen());
    QCOMPARE(ms->classTypeName(), MSReaderBase::MSReaderClassTypeUnknown);
    QVERIFY(ms->getFilename().isEmpty());

    QCOMPARE(ms->openFile(path()), kNoErr);
    QVERIFY(ms->isOpen());
    QVERIFY(!ms->getFilename().isEmpty());

    QCOMPARE(ms->closeFile(), kNoErr);
    QVERIFY(!ms->isOpen());
    QCOMPARE(ms->classTypeName(), MSReaderBase::MSReaderClassTypeUnknown);
}

QString MSReaderAbiTest::path() const
{
    return m_path;
}

void MSReaderAbiTest::prepareTestPaths()
{
    QString filename = m_args[0].trimmed();
    qDebug() << filename;

    const QFileInfo fileInfo(filename);
    // args[0] can be either a plain file name or a full file path
    const bool isPath = fileInfo.exists();
    const bool isDir = fileInfo.isDir();

    static const QString testDataRootPath
        = QStringLiteral("%1/MSReaderAbiTest/").arg(PMI_TEST_FILES_OUTPUT_DIR);

    m_path = isPath ? filename : (testDataRootPath + filename);
    if (isDir) {
        m_path += "/" + QFileInfo(filename).fileName() + ".wiff";
    }
    qDebug() << m_path;

    QVERIFY(QFileInfo(m_path).exists());

    filename = QDir(testDataRootPath).relativeFilePath(m_path);
    qDebug() << filename;
    QVERIFY(s_runData.keys().contains(filename));
    QVERIFY(s_expectedData.keys().contains(filename));
    s_currentDataFile = filename;
}

void MSReaderAbiTest::testGetNumberOfSpectra()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    long totalNumber = -1;
    long startScan = -1;
    long endScan = -1;

    const Err err = m_abiReader->getNumberOfSpectra(&totalNumber, &startScan, &endScan);
    QCOMPARE(err, kNoErr);

    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    for (ExpectedDataItem expectedData : expectedDataList) {
        QCOMPARE(totalNumber, expectedData.numberOfSpectra.totalNumber);
        QCOMPARE(startScan, expectedData.numberOfSpectra.startScan);
        QCOMPARE(endScan, expectedData.numberOfSpectra.endScan);
    }
}

void MSReaderAbiTest::testGetScanData()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    point2dList points;
    PointListAsByteArrays pointListAsByteArrays;

    const QList<RunData> runDataList = s_runData[s_currentDataFile];
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    QCOMPARE(runDataList.count(), expectedDataList.count());
    for (int i = 0; i < expectedDataList.count(); ++i) {
        const long scanNumber = runDataList[i].getScanDataScanNumber;
        const bool doCentroiding = runDataList[i].getScanDataDoCentroiding;
        const ExpectedDataItem expectedData = expectedDataList[i];

        const Err err = m_abiReader->getScanData(scanNumber, &points, doCentroiding,
                                                      &pointListAsByteArrays);
        QCOMPARE(err, kNoErr);
        QCOMPARE((long)points.size(), expectedData.scanPointListSize);
    }
}

void MSReaderAbiTest::testGetTICData()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    point2dList points;
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    for (ExpectedDataItem expectedData : expectedDataList) {
        const Err err = m_abiReader->getTICData(&points);
        QCOMPARE(err, kNoErr);
        QCOMPARE((long)points.size(), expectedData.ticPointListSize);
    }
}

void MSReaderAbiTest::testGetBasePeakData()
{
    {
        point2dList points;
        const Err err = m_abiReader->getBasePeak(&points);
        QCOMPARE(err, kFunctionNotImplemented);
    }

    {
        MSReader *ms = MSReader::Instance();
        QVERIFY(ms);
        QCOMPARE(ms->openFile(path()), kNoErr);
        point2dList points;
        const Err err = ms->getBasePeak(&points);
        QCOMPARE(err, kNoErr);
    }
}

void MSReaderAbiTest::testGetXICData()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    point2dList points;
    const QList<RunData> runDataList = s_runData[s_currentDataFile];
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    QCOMPARE(runDataList.count(), expectedDataList.count());

    for (int i = 0; i < expectedDataList.count(); ++i) {
        const Err err = m_abiReader->getXICData(runDataList[i].xicWwin, &points);
        QCOMPARE(err, kNoErr);
        QCOMPARE((long)points.size(), expectedDataList[i].xicPointListSize);
    }
}

void MSReaderAbiTest::testGetBestScanNumber()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    long scanNumber = 0;
    const QList<RunData> runDataList = s_runData[s_currentDataFile];
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    QCOMPARE(runDataList.count(), expectedDataList.count());
    for (int i = 0; i < expectedDataList.count(); ++i) {
        {
            const Err err
                = m_abiReader->getBestScanNumber(1, runDataList[i].bestScanNumberTime, &scanNumber);
            QCOMPARE(err, kNoErr);
            QCOMPARE(scanNumber, expectedDataList[i].bestScanNumberLevel1);
        }
        {
            const Err err
                = m_abiReader->getBestScanNumber(2, runDataList[i].bestScanNumberTime, &scanNumber);
            QCOMPARE(err, kNoErr);
            QCOMPARE(scanNumber, expectedDataList[i].bestScanNumberLevel2);
        }
    }
}

void MSReaderAbiTest::testGetScanInfo()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    double epsilon = 0.00001;
    ScanInfo scanInfo;
    const QList<RunData> runDataList = s_runData[s_currentDataFile];
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    QCOMPARE(runDataList.count(), expectedDataList.count());
    for (int i = 0; i < expectedDataList.count(); ++i) {
        const ScanInfo expectedScanInfo = expectedDataList[i].scanInfo;
        const Err err = m_abiReader->getScanInfo(runDataList[i].scanInfoNumber, &scanInfo);
        QCOMPARE(err, kNoErr);

        QCOMPARE(scanInfo.scanLevel, expectedScanInfo.scanLevel);
        QCOMPARE(scanInfo.peakMode, expectedScanInfo.peakMode);
        QCOMPARE(scanInfo.scanMethod, expectedScanInfo.scanMethod);
        QVERIFY(std::abs(scanInfo.retTimeMinutes - expectedScanInfo.retTimeMinutes) < epsilon);
        QCOMPARE(scanInfo.nativeId, expectedScanInfo.nativeId);
    }
}

void MSReaderAbiTest::testGetScanPrecursorInfo()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    PrecursorInfo pInfo;
    const QList<RunData> runDataList = s_runData[s_currentDataFile];
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    QCOMPARE(runDataList.count(), expectedDataList.count());
    for (int i = 0; i < expectedDataList.count(); ++i) {
        const Err err = m_abiReader->getScanPrecursorInfo(
            runDataList[i].getScanPrecursorInfoScanNumber, &pInfo);
        const PrecursorInfo expectedPInfo = expectedDataList[i].precursorInfo;

        // if expected nScanNumber < 0 then we expect kBadParameterError
        if (expectedPInfo.nScanNumber < 0) {
            QCOMPARE(err, kBadParameterError);
        } else {
            QCOMPARE(err, kNoErr);
            QCOMPARE(pInfo.dIsolationMass, expectedPInfo.dIsolationMass);
            QCOMPARE(pInfo.dMonoIsoMass, expectedPInfo.dMonoIsoMass);
            QCOMPARE(pInfo.nChargeState, expectedPInfo.nChargeState);
            QCOMPARE(pInfo.nScanNumber, expectedPInfo.nScanNumber);
            QCOMPARE(pInfo.nativeId, expectedPInfo.nativeId);
        }
    }
}

void MSReaderAbiTest::testGetChromatograms()
{
    QVERIFY(!s_currentDataFile.isEmpty());
    QList<ChromatogramInfo> chroList;
    QSet<QString> chroListIds;
    const QList<RunData> runDataList = s_runData[s_currentDataFile];
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    QCOMPARE(runDataList.count(), expectedDataList.count());
    for (int i = 0; i < expectedDataList.count(); ++i) {
        Err err = m_abiReader->getChromatograms(&chroList);
        QCOMPARE(err, kNoErr);
        chroListIds.clear();
        for (auto chro : chroList) {
            chroListIds.insert(chro.internalChannelName);
        }

        const QSet<QString> expected = expectedDataList[i].chromatogramsIds;

        QCOMPARE(chroListIds, expected);

        for (auto chroId : expected) {
            err = m_abiReader->getChromatograms(&chroList, chroId);
            QCOMPARE(err, kNoErr);
            QCOMPARE(chroList.size(), 1);
            QCOMPARE(chroList[0].internalChannelName, chroId);
        }

    }
}

void MSReaderAbiTest::compareChromatogramCanonicalId(const MSReaderAbi::ChromatogramCanonicalId &id,
                                                     const QString &expectedString,
                                                     bool compartibilityTesting, bool inverse)
{
    typedef MSReaderAbi::ChromatogramCanonicalId ChromatogramCanonicalId;

    QVERIFY(inverse != (id == ChromatogramCanonicalId::fromInternalChannelName(expectedString)));

    if (!compartibilityTesting) {
        QVERIFY(inverse != (id.toString() == expectedString));
    }

    QVERIFY(inverse
            != (ChromatogramCanonicalId::fromInternalChannelName(id.toString())
                == ChromatogramCanonicalId::fromInternalChannelName(expectedString)));
    QVERIFY(inverse
            != (ChromatogramCanonicalId::fromInternalChannelName(id.toString()).toString()
                == ChromatogramCanonicalId::fromInternalChannelName(expectedString).toString()));
}

void MSReaderAbiTest::testChromatogramCanonicalId()
{
    typedef MSReaderAbi::ChromatogramCanonicalId ChromatogramCanonicalId;

    ChromatogramCanonicalId empty;
    QVERIFY(empty.isEmpty());
    compareChromatogramCanonicalId(empty, QString());

    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2"), "chroType;s=2");
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2", "3"),
                                   "chroType;s=2;a=3");
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2", "3"),
                                   "chroType;s=2;a=4", false, true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2", "3"),
                                   "chroType;s=3;a=3", false, true);

    // forward compartibility test
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2"),
                                   "chroType;s=2;somethingNew=5", true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2"),
                                   "chroType;s=2;somethingNew", true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2", "3"),
                                   "chroType;s=2;a=3;somethingNew=5", true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2", "3"),
                                   "chroType;s=2;a=3;somethingNew", true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2"),
                                   "chroType;somethingNew=5;s=2", true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2"),
                                   "chroType;somethingNew;s=2", true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2", "3"),
                                   "chroType;somethingNew;s=2;a=3", true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2", "3"),
                                   "chroType;somethingNew=5;s=2;a=3", true);

    // default value test
    compareChromatogramCanonicalId(ChromatogramCanonicalId::fromInternalChannelName("chroType"),
                                   "chroType;s=0");
    compareChromatogramCanonicalId(ChromatogramCanonicalId::fromInternalChannelName("chroType;a=3"),
                                   "chroType;s=0;a=3");

    // order test
    compareChromatogramCanonicalId(ChromatogramCanonicalId::fromInternalChannelName("chroType;a=3;s=0"),
        "chroType;s=0;a=3");

    // empty tokens test
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2"), "chroType;;s=2", true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2"), "chroType;s=2;;", true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2", "3"),
                                   "chroType;s=2;a=3", true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2", "3"),
                                   "chroType;;s=2;a=3", true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2", "3"),
                                   "chroType;s=2;;a=3", true);
    compareChromatogramCanonicalId(ChromatogramCanonicalId("chroType", "2", "3"),
                                   "chroType;s=2;a=3;;", true);
}

PMI_TEST_GUILESS_MAIN_WITH_ARGS(MSReaderAbiTest, QStringList() << "path")

#include "MSReaderAbiTest.moc"
