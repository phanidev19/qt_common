#include <QtTest>
#include "pmi_core_defs.h"
#include "MSDataNonUniform.h"
#include "NonUniformTileStoreMemory.h"
#include "MSReader.h"
#include "NonUniformTileBuilder.h"
#include "ScanIndexNumberConverter.h"
#include "MSReaderInfo.h"
#include <utility>

#include "PMiTestUtils.h"
#include "NonUniformTileStoreSqlite.h"
#include <QScopedPointer>
#include "db\NonUniformTilesInfoDao.h"
#include <PmiQtStablesConstants.h>

static const QString CONA_RAW = QStringLiteral("cona_tmt0saxpdetd.raw");
static const QString DM_BIOS3_RAW = QStringLiteral("020215_DM_BioS3_trp_Glyco.raw");
static const QString DM_Av_BYSPEC2 = QStringLiteral("020215_DM_Av.byspec2");

//#define TEST_CONA_FROM_TILE_BUILDER_TEST
//#define MEASURE_TIME

_PMI_BEGIN

enum Store { StoreMemory, StoreSqlite };

class MSDataNonUniformTest : public QObject
{
    Q_OBJECT

public:
    MSDataNonUniformTest(const QStringList &args) : m_testDataBasePath(args[0])
    {
        //TODO Fix this when ML-2030 gets implemented
#ifdef TEST_CONA_FROM_TILE_BUILDER_TEST
        m_testDataBasePath = QDir(QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/TileBuilderTest/remoteData/"));
        m_rawFilePath = m_testDataBasePath.filePath(CONA_RAW);
#else
        m_rawFilePath = m_testDataBasePath.filePath(DM_Av_BYSPEC2);
#endif        
        QVERIFY(QFileInfo(m_rawFilePath).exists());
        m_reader = MSReader::Instance();
        QCOMPARE(m_reader->openFile(m_rawFilePath), kNoErr);

        QElapsedTimer et;
        et.start();
        
        MSReaderInfo info(m_reader);
        QString fileName = QFileInfo(m_rawFilePath).fileName();
        qDebug() << "Testing with" << fileName;

        m_testRange = createTestRange(info, fileName);
        qDebug() << "createTestRange -- elapsed:"  << et.elapsed();
    }

private Q_SLOTS :
    void initTestCase();

    void testGetScanData_data();
    void testGetScanData();
    void testGetXICData_data();
    void testGetXICData();

    // this test tests latest version of getXICData for particular MS File+existing NonUniform cache and particular XIC window
    void testGetXICDataNG();

private:
    NonUniformTileStore * fetchStore(Store type, const QString& filePath);
    NonUniformTileRange createTestRange(const MSReaderInfo &info, const QString &fileName);
    bool createTileStore(const NonUniformTileRange &range,const ScanIndexNumberConverter &converter, NonUniformTileStore::ContentType type, NonUniformTileStore * store);

private:
    QDir m_testDataBasePath;
    QString m_rawFilePath;
    MSReader * m_reader;
    NonUniformTileRange m_testRange;
    QSqlDatabase m_db;
};

void MSDataNonUniformTest::initTestCase()
{
    QVERIFY(QFileInfo(m_rawFilePath).isReadable());
}

void MSDataNonUniformTest::testGetScanData_data()
{
    QTest::addColumn<NonUniformTileStore::ContentType>("contentType");
    QTest::addColumn<int>("storeType");

    QTest::newRow("MS1Centroided-Memory") << NonUniformTileStore::ContentMS1Centroided << int(StoreMemory);
    QTest::newRow("MS1Raw-Memory") << NonUniformTileStore::ContentMS1Raw << int(StoreMemory);

    QTest::newRow("MS1Centroided-Sqlite") << NonUniformTileStore::ContentMS1Centroided << int(StoreSqlite);
    QTest::newRow("MS1Raw-Sqlite") << NonUniformTileStore::ContentMS1Raw << int(StoreSqlite);
}

void MSDataNonUniformTest::testGetScanData()
{
    QFETCH(NonUniformTileStore::ContentType, contentType);
    QFETCH(int, storeType);

    QString fileName = QString("testGetScanData_%1.db3").arg(contentType);
    QString filePath = QDir(PMI_TEST_FILES_OUTPUT_DIR).filePath(fileName);
    if (QFileInfo(filePath).exists()) {
        QFile::remove(filePath);
    }
    QScopedPointer<NonUniformTileStore> store(fetchStore(Store(storeType), filePath));
    
    QVERIFY(store != nullptr);
    QVERIFY(!filePath.isEmpty());
    
    NonUniformTileRange range = m_testRange;
    ScanIndexNumberConverter converter = ScanIndexNumberConverter::fromMSReader(m_reader);
    QVERIFY(createTileStore(range, converter, contentType, store.data()));
    MSDataNonUniform data(store.data(), range, converter);

    int failed = 0;
    bool doCentroiding = contentType == NonUniformTileStore::ContentMS1Centroided;
    
#ifdef MEASURE_TIME
    QElapsedTimer et;
    et.start();
#endif
    
    for (int i = range.scanIndexMin(); i < range.scanIndexMax(); ++i) {
        int scanNumber = converter.toScanNumber(i);

        point2dList actual;
        data.getScanData(scanNumber, &actual, doCentroiding);
        
        point2dList expected;
        m_reader->getScanData(scanNumber, &expected, doCentroiding);

        bool scanNumberContentEqual = (actual == expected);
        if (!scanNumberContentEqual) {
            QString dirName = (contentType == NonUniformTileStore::ContentMS1Centroided) ? "MS1Centroided" : "MS1Raw";
            QString filePath = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/%3_NonUniform_i%1_sn%2_tiles.csv").arg(i).arg(scanNumber).arg(dirName);

            PlotBase pb1(actual);
            pb1.saveDataToFile(filePath);

            QString filePath2 = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/%3_NonUniform_i%1_sn%2_msreader.csv").arg(i).arg(scanNumber).arg(dirName);

            PlotBase pb2(expected);
            pb2.saveDataToFile(filePath2);

            failed++;
            qDebug() << failed << "at" << "scan number" << scanNumber << ", index:" << i;
        }
    }
#ifdef MEASURE_TIME    
    qDebug() << "Verifying scan data took" << et.elapsed() << "ms";
#endif

    QCOMPARE(failed, 0);
    if (Store(storeType) == StoreSqlite) {
        m_db.close();
        QVERIFY(QFile::remove(filePath));
    }
}

void MSDataNonUniformTest::testGetXICData_data()
{
    QTest::addColumn<int>("storeType");
    QTest::newRow("GetXICData-Memory") << int(StoreMemory);
    QTest::newRow("GetXICData-Sqlite") << int(StoreSqlite);
}

void MSDataNonUniformTest::testGetXICData()
{
    QFETCH(int, storeType);
    QString fileName = QString("testGetXICData.db3");
    QString filePath = QDir(PMI_TEST_FILES_OUTPUT_DIR).filePath(fileName);
    if (QFileInfo(filePath).exists()) {
        QFile::remove(filePath);
    }
    QScopedPointer<NonUniformTileStore> store(fetchStore(Store(storeType), filePath));
    
    NonUniformTileRange range = m_testRange;
    ScanIndexNumberConverter converter = ScanIndexNumberConverter::fromMSReader(m_reader);
    QVERIFY(createTileStore(range, converter, NonUniformTileStore::ContentMS1Centroided, store.data()));
    
    MSDataNonUniform data(store.data(), range, converter);

    int failed = 0;

    // scan 50% of mz range with margin 25% on both sides, selected arbitrary
    double quarterMzRange = (range.mzMax() - range.mzMin()) * 0.25;

    msreader::XICWindow window;
    window.mz_start = range.mzMin() + quarterMzRange;
    window.mz_end = window.mz_start + quarterMzRange;

    //cona
    window.time_start = converter.toScanTime(converter.toScanNumber(range.scanIndexMin()));
    window.time_end = converter.toScanTime(converter.toScanNumber(range.scanIndexMax()));

    point2dList actual;
    Err e = data.getXICData(window, &actual);
    QCOMPARE(e, kNoErr);

    point2dList expected;
    e = m_reader->getXICData(window, &expected);
    QCOMPARE(e, kNoErr);

    int xicWindowIndex = 0;
    int i = 0;
    bool same = (actual == expected);
    if (!same) {

        if (actual.size() == expected.size()) {
            // make diff
            point2dList diff;
            for (size_t j = 0; j < actual.size(); ++j) {
                double diffTime = actual[j].x() - expected[j].x();
                double diffSum = actual[j].y() / expected[j].y();
                diff.push_back(QPointF(diffTime, diffSum));
            }

            QString filePath = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/XICData_i%1_sn%2_diff.csv").arg(i).arg(xicWindowIndex);
            PlotBase diffPb(diff);
            diffPb.saveDataToFile(filePath);
        }

        QString filePath = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/XICData_i%1_sn%2_tiles.csv").arg(i).arg(xicWindowIndex);
        PlotBase pb1(actual);
        pb1.saveDataToFile(filePath);

        QString filePath2 = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/XICData_i%1_sn%2_msreader.csv").arg(i).arg(xicWindowIndex);
        PlotBase pb2(expected);
        pb2.saveDataToFile(filePath2);
        
        qWarning() << "Failed for window" << window.mz_start << window.mz_end << window.time_start << window.time_end;
        failed++;
    }

    QCOMPARE(failed, 0);
    if (Store(storeType) == StoreSqlite) {
        m_db.close();
        QVERIFY(QFile::remove(filePath));
    }
}


void MSDataNonUniformTest::testGetXICDataNG()
{
    //Note: You need to run MSDataNonUniformAdapterTest test to generate some cache file first, thus this test is manual 
    QSKIP("Skipping: INPUT NonUniform cache location to be determined!");

    QDir adapterTestDir = (QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/MSDataNonUniformAdapterTest/remoteData/"));

    QString cacheFilePath = adapterTestDir.filePath("011315_DM_AvastinEu_IA_LysN.raw.NonUniform.cache");
    QString msFilePath    = adapterTestDir.filePath("011315_DM_AvastinEu_IA_LysN.raw");

    msreader::XICWindow win;
    win.mz_start = 508.230286324152;
    win.mz_end = 508.248582943798;
    win.time_start = 32.0540397242667;
    win.time_end = 76.0540397242667;

    QScopedPointer<NonUniformTileStoreSqlite> store( static_cast<NonUniformTileStoreSqlite*>(fetchStore(StoreSqlite, cacheFilePath) ));

    NonUniformTileRange range;
    NonUniformTilesInfoDao infoDao(&m_db);
    infoDao.load(&range);
    
    MSReader * reader = MSReader::Instance();
    QCOMPARE(reader->openFile(msFilePath), kNoErr);
    ScanIndexNumberConverter converter = ScanIndexNumberConverter::fromMSReader(reader);

    MSDataNonUniform data(store.data(), range, converter);

    // MSDataNonUniform doesn't find best/closest scan number (yet)
    long scanNumberStart;
    long scanNumberEnd;
    reader->getBestScanNumberFromScanTime(1, win.time_start, &scanNumberStart);
    reader->getBestScanNumberFromScanTime(1, win.time_end, &scanNumberEnd);
    win.time_start = converter.toScanTime(scanNumberStart);
    win.time_end = converter.toScanTime(scanNumberEnd);

    point2dList expected;
    reader->getXICData(win, &expected);

    point2dList actual;
    data.getXICDataNG(win, &actual);

    if (actual != expected) {
        int xicWindowIndex = 0;
        QString rawBaseName = QFileInfo(msFilePath).baseName();
        QString filePath = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/MSDataNonUniformTest/%1_%2_nonUniform.csv").arg(rawBaseName).arg(xicWindowIndex);
        PlotBase(actual).saveDataToFile(filePath);

        filePath = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/MSDataNonUniformTest/%1_%2_actual.csv").arg(rawBaseName).arg(xicWindowIndex);
        PlotBase(expected).saveDataToFile(filePath);
    }
}


pmi::NonUniformTileRange MSDataNonUniformTest::createTestRange(const MSReaderInfo &info, const QString &fileName)
{
    NonUniformTileRange range;
    std::pair<double, double> minMaxMz;
    
    // precomputed
    if (fileName == CONA_RAW) {
        minMaxMz = { 380.0001, 1706.63 };
    } else if (fileName == DM_BIOS3_RAW) {
        minMaxMz = { 195.11802005248208, 1515.1351516368195 };
    } else if (fileName == DM_Av_BYSPEC2) {
        minMaxMz = { 197.07627868652344, 1499.9744873046875 };
    } else {
        bool ok = false;
        // heavy, iterates over all scan numbers to determine min, max value
        minMaxMz = info.mzMinMax(true, &ok);
        qDebug() << "info.mzMinMax returns bad range";
    }

    int scanIndexMax = info.scanNumberCount() - 1;
    
    // we will construct only part of the document to keep test fast
    int constructIndexCount = qMin(127, scanIndexMax); // first 127 scan numbers

    range.setMz(minMaxMz.first, minMaxMz.second);
    range.setMzTileLength(30.0);

    range.setScanIndex(0, constructIndexCount);
    range.setScanIndexLength(64);
    return range;
}

bool MSDataNonUniformTest::createTileStore(const NonUniformTileRange &range,const ScanIndexNumberConverter &converter, NonUniformTileStore::ContentType type, NonUniformTileStore * store)
{
    QElapsedTimer et;
    et.start();

    NonUniformTileBuilder builder(range);
    builder.buildNonUniformTiles(m_reader, converter, store, type);
    qDebug() << "Tiles store built in" << et.elapsed() << "ms";
    return true;
}

NonUniformTileStore * MSDataNonUniformTest::fetchStore(Store type, const QString& filePath)
{
    if (type == StoreMemory) {
        return new NonUniformTileStoreMemory;
    } else if (type == StoreSqlite) {
        static int instances = 0;
        instances++;
        if (m_db.isOpen()) {
            m_db.close();
        }
        
        m_db = QSqlDatabase::addDatabase(kQSQLITE, QString("MSDataNonUniformTest_%1").arg(instances));
        m_db.setDatabaseName(filePath);
        if (!m_db.open()) {
            qWarning() << "Could not open db file:" << filePath;
        }

        NonUniformTileStoreSqlite * store = new NonUniformTileStoreSqlite(&m_db);
        store->init();
        return store;
    }

    return nullptr; //unknown store type
}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::MSDataNonUniformTest, QStringList() << "Remote Data Folder")

#include "MSDataNonUniformTest.moc"
