/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include <MSReader.h>
#include <MSReaderShimadzu.h>
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
    long basePeakPointListSize;
    long ticPointListSize;
    long xicPointListSize;
    ScanInfo scanInfo;
    long bestScanNumberLevel1;
    long bestScanNumberLevel2;
    PrecursorInfo precursorInfo;
    QSet<QString> chromatogramsIds;
};

static QMap<QString, QList<RunData>> s_runData = {
    { "BSA_Digest_1pmol-uL_DDA_003.lcd",
      { { XICWindow(), 200, 2.62, 200, false, 200 },
        { XICWindow(2500, 3500, 2.9, 3.6), 100, 4.130, 100, true, 190 } } }
};

static QMap<QString, QList<ExpectedDataItem>> s_expectedData
    = { { "BSA_Digest_1pmol-uL_DDA_003.lcd",
          { { { 1129, 1, 1129 },
              154,
              403,
              403,
              403,
              { 2.471, 2, "segment=1;event=2;scan=200", PeakPickingProfile, ScanMethodFullScan },
              256,
              257,
              { PrecursorInfo(199, 557.281782139, 557.281782139, 0, "segment=1;event=1;scan=199") },
              { "tic;s=1;e=1", "tic;s=1;e=2", "tic;s=1;e=3", "tic;s=1;e=4", "xic;s=1;e=1",
                "bp;s=1;e=1" } },
            { { 1129, 1, 1129 },
              10329,
              403,
              403,
              65,
              { 2.25246666666, 1, "segment=1;event=1;scan=100", PeakPickingProfile,
                ScanMethodFullScan },
              812,
              813,
              { PrecursorInfo(187, 748.784629726, 748.784629726, 0, "segment=1;event=1;scan=187") },
              { "tic;s=1;e=1", "tic;s=1;e=2", "tic;s=1;e=3", "tic;s=1;e=4", "xic;s=1;e=1",
                "bp;s=1;e=1" } } } } };

class MSReaderShimadzuTest : public QObject
{
    Q_OBJECT
public:
    explicit MSReaderShimadzuTest(const QStringList &args)
        : QObject()
        , m_args(args)
    {
    }

    ~MSReaderShimadzuTest() {}

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testIsShimadzuPath();
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

private:
    const QStringList m_args;
    QString m_path;
    QSharedPointer<MSReaderShimadzu> m_shimadzuReader;
};

void MSReaderShimadzuTest::initTestCase()
{
    prepareTestPaths();

    m_shimadzuReader.reset(new MSReaderShimadzu());

    QCOMPARE(m_shimadzuReader->openFile(path()), kNoErr);
}

void MSReaderShimadzuTest::testMSReaderInstance()
{
    MSReader *ms = MSReader::Instance();
    QVERIFY(ms);
}

void MSReaderShimadzuTest::cleanupTestCase()
{
    if (m_shimadzuReader.data() != nullptr) {

        m_shimadzuReader->closeFile();
    }
}

void MSReaderShimadzuTest::testIsShimadzuPath()
{
    QVERIFY(m_shimadzuReader->canOpen(path()));
    QVERIFY(!m_shimadzuReader->canOpen(path() + ".somethingelse"));
}

void MSReaderShimadzuTest::testOpenDirect()
{
    QFileInfo fi(path());
    QVERIFY(fi.isFile());
    QVERIFY(fi.exists());

    MSReaderShimadzu reader;
    QCOMPARE(reader.classTypeName(), MSReaderBase::MSReaderClassTypeShimadzu);
    QVERIFY(reader.canOpen(path()));
    QCOMPARE(reader.openFile(path()), kNoErr);
    QVERIFY(!reader.getFilename().isEmpty());
    QCOMPARE(reader.closeFile(), kNoErr);
}

void MSReaderShimadzuTest::testOpenViaMSReader()
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

QString MSReaderShimadzuTest::path() const
{
    return m_path;
}

void MSReaderShimadzuTest::prepareTestPaths()
{
    QString filename = m_args[0].trimmed();

    const QFileInfo fileInfo(filename);
    // args[0] can be either a plain file name or a full file path
    const bool isPath = fileInfo.exists();
    // filename = isPath ? fileInfo.fileName() : filename;

    m_path = isPath
        ? filename
        : QString("%1/MSReaderShimadzuTest/%2").arg(PMI_TEST_FILES_OUTPUT_DIR).arg(filename);

    QVERIFY(QFileInfo(m_path).exists());

    filename = QFileInfo(m_path).fileName();
    QVERIFY(s_runData.keys().contains(filename));
    QVERIFY(s_expectedData.keys().contains(filename));
    s_currentDataFile = filename;
}

void MSReaderShimadzuTest::testGetNumberOfSpectra()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    long totalNumber = -1;
    long startScan = -1;
    long endScan = -1;

    const Err err = m_shimadzuReader->getNumberOfSpectra(&totalNumber, &startScan, &endScan);
    QCOMPARE(err, kNoErr);

    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    for (ExpectedDataItem expectedData : expectedDataList) {
        QCOMPARE(totalNumber, expectedData.numberOfSpectra.totalNumber);
        QCOMPARE(startScan, expectedData.numberOfSpectra.startScan);
        QCOMPARE(endScan, expectedData.numberOfSpectra.endScan);
    }
}

void MSReaderShimadzuTest::testGetScanData()
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

        const Err err = m_shimadzuReader->getScanData(scanNumber, &points, doCentroiding,
                                                      &pointListAsByteArrays);
        QCOMPARE(err, kNoErr);
        QCOMPARE((long)points.size(), expectedData.scanPointListSize);
    }
}

void MSReaderShimadzuTest::testGetTICData()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    point2dList points;
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    for (ExpectedDataItem expectedData : expectedDataList) {
        const Err err = m_shimadzuReader->getTICData(&points);
        QCOMPARE(err, kNoErr);
        QCOMPARE((long)points.size(), expectedData.ticPointListSize);
    }
}

void MSReaderShimadzuTest::testGetBasePeakData()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    point2dList points;
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    for (ExpectedDataItem expectedData : expectedDataList) {
        const Err err = m_shimadzuReader->getBasePeak(&points);
        QCOMPARE(err, kNoErr);
        QCOMPARE((long)points.size(), expectedData.basePeakPointListSize);
    }
}

void MSReaderShimadzuTest::testGetXICData()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    point2dList points;
    const QList<RunData> runDataList = s_runData[s_currentDataFile];
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    QCOMPARE(runDataList.count(), expectedDataList.count());

    for (int i = 0; i < expectedDataList.count(); ++i) {
        const Err err = m_shimadzuReader->getXICData(runDataList[i].xicWwin, &points);
        QCOMPARE(err, kNoErr);
        QCOMPARE((long)points.size(), expectedDataList[i].xicPointListSize);
    }
}

void MSReaderShimadzuTest::testGetBestScanNumber()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    long scanNumber = 0;
    const QList<RunData> runDataList = s_runData[s_currentDataFile];
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    QCOMPARE(runDataList.count(), expectedDataList.count());
    for (int i = 0; i < expectedDataList.count(); ++i) {
        {
            const Err err = m_shimadzuReader->getBestScanNumber(
                1, runDataList[i].bestScanNumberTime, &scanNumber);
            QCOMPARE(err, kNoErr);
            QCOMPARE(scanNumber, expectedDataList[i].bestScanNumberLevel1);
        }
        {
            const Err err = m_shimadzuReader->getBestScanNumber(
                2, runDataList[i].bestScanNumberTime, &scanNumber);
            QCOMPARE(err, kNoErr);
            QCOMPARE(scanNumber, expectedDataList[i].bestScanNumberLevel2);
        }
    }
}

void MSReaderShimadzuTest::testGetScanInfo()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    double epsilon = 0.00001;
    ScanInfo scanInfo;
    const QList<RunData> runDataList = s_runData[s_currentDataFile];
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    QCOMPARE(runDataList.count(), expectedDataList.count());
    for (int i = 0; i < expectedDataList.count(); ++i) {
        const ScanInfo expectedScanInfo = expectedDataList[i].scanInfo;
        const Err err = m_shimadzuReader->getScanInfo(runDataList[i].scanInfoNumber, &scanInfo);
        QCOMPARE(err, kNoErr);

        QCOMPARE(scanInfo.scanLevel, expectedScanInfo.scanLevel);
        QCOMPARE(scanInfo.peakMode, expectedScanInfo.peakMode);
        QCOMPARE(scanInfo.scanMethod, expectedScanInfo.scanMethod);
        QVERIFY(std::abs(scanInfo.retTimeMinutes - expectedScanInfo.retTimeMinutes) < epsilon);
        QCOMPARE(scanInfo.nativeId, expectedScanInfo.nativeId);
    }
}

void MSReaderShimadzuTest::testGetScanPrecursorInfo()
{
    QVERIFY(!s_currentDataFile.isEmpty());

    PrecursorInfo pInfo;
    const QList<RunData> runDataList = s_runData[s_currentDataFile];
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    QCOMPARE(runDataList.count(), expectedDataList.count());
    for (int i = 0; i < expectedDataList.count(); ++i) {
        const Err err = m_shimadzuReader->getScanPrecursorInfo(
            runDataList[i].getScanPrecursorInfoScanNumber, &pInfo);
        const PrecursorInfo expectedPInfo = expectedDataList[i].precursorInfo;
        // no checking for success - some precursors can returns not kNoErr
        // QCOMPARE(err, kNoErr);
        QCOMPARE(pInfo.dIsolationMass, expectedPInfo.dIsolationMass);
        QCOMPARE(pInfo.dMonoIsoMass, expectedPInfo.dMonoIsoMass);
        QCOMPARE(pInfo.nChargeState, expectedPInfo.nChargeState);
        QCOMPARE(pInfo.nScanNumber, expectedPInfo.nScanNumber);
        QCOMPARE(pInfo.nativeId, expectedPInfo.nativeId);
    }
}

void MSReaderShimadzuTest::testGetChromatograms()
{
    QVERIFY(!s_currentDataFile.isEmpty());
    QList<ChromatogramInfo> chroList;
    QSet<QString> chroListIds;
    const QList<RunData> runDataList = s_runData[s_currentDataFile];
    const QList<ExpectedDataItem> expectedDataList = s_expectedData[s_currentDataFile];
    QCOMPARE(runDataList.count(), expectedDataList.count());
    for (int i = 0; i < expectedDataList.count(); ++i) {
        const Err err = m_shimadzuReader->getChromatograms(&chroList);
        QCOMPARE(err, kNoErr);
        chroListIds.clear();
        for (auto chro : chroList) {
            chroListIds.insert(chro.internalChannelName);
        }

        const QSet<QString> expected = expectedDataList[i].chromatogramsIds;

        QCOMPARE(chroListIds, expected);
    }
}

PMI_TEST_GUILESS_MAIN_WITH_ARGS(MSReaderShimadzuTest, QStringList() << "path")

#include "MSReaderShimadzuTest.moc"
