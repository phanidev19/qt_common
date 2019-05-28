#include <QtTest>
#include "pmi_core_defs.h"
#include "NonUniformTileRange.h"
#include "NonUniformTileStoreMemory.h"
#include "NonUniformTileBuilder.h"

#include <ComInitializer.h>
#include <MSReader.h>
#include "MSReaderTesting.h"
#include <QDir>
#include "RandomNonUniformTileIterator.h"
#include "NonUniformTileStoreSqlite.h"
#include "db\NonUniformTilesInfoDao.h"
#include <PmiQtStablesConstants.h>

_PMI_BEGIN

static const QString CONA_RAW("cona_tmt0saxpdetd.raw");

class NonUniformTileBuilderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testBuildNonUniformTiles();

    void testBuildNonUniformTilesNG();


    void testTileContentBorderValues();

    MSReader * initReader()
    {
        return MSReader::Instance();
    }



    void cleanUpReader(MSReader * ms)
    {
        ms->releaseInstance();
        ms = nullptr;
    }

private:
    ComInitializer comInitializer;
};

void NonUniformTileBuilderTest::testBuildNonUniformTiles()
{
    QDir testDataBasePath = QDir(QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/TileBuilderTest/remoteData/"));

    QString rawFilePath = testDataBasePath.filePath(CONA_RAW);
    QVERIFY(QFileInfo(rawFilePath).isReadable());

    MSReader * reader = initReader();
    QCOMPARE(reader->openFile(rawFilePath), kNoErr);
    
    NonUniformTileRange range;
    // we are using usually minium and maximum mz found in the input MS file (*.raw)
    range.setMz(380.001, 1706.6299979999999);
    range.setMzTileLength(30.0);

    range.setScanIndex(0, 63);
    range.setScanIndexLength(64);

    NonUniformTileStore::ContentType type = NonUniformTileStore::ContentMS1Raw;
    NonUniformTileStoreMemory ram;

    ScanIndexNumberConverter converter = ScanIndexNumberConverter::fromMSReader(reader);

    NonUniformTileBuilder builder(range);
    builder.buildNonUniformTiles(reader, converter, &ram, type);
    QVERIFY(ram.count(type) > 0);
    QCOMPARE(ram.count(type), 45);
}

void NonUniformTileBuilderTest::testBuildNonUniformTilesNG()
{
    QDir testDataBasePath = QDir(QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/TileBuilderTest/remoteData/"));

    QString rawFilePath = testDataBasePath.filePath(CONA_RAW);
    QVERIFY(QFileInfo(rawFilePath).isReadable());

    MSReader * reader = initReader();
    QCOMPARE(reader->openFile(rawFilePath), kNoErr);
    ScanIndexNumberConverter converter = ScanIndexNumberConverter::fromMSReader(reader);

    NonUniformTileRange range;
    // we are using usually minium and maximum mz found in the input MS file (*.raw)
    range.setMz(380.001, 1706.6299979999999);
    range.setMzTileLength(2.95);

    range.setScanIndex(0, 63);
    range.setScanIndexLength(4);

    NonUniformTileStore::ContentType type = NonUniformTileStore::ContentMS1Raw;
    QSqlDatabase db;
    QString dbFilePath = QDir(QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/TileBuilderTest/")).filePath("TileParts.db3");
    
    if (QFileInfo(dbFilePath).exists()) {
        QVERIFY(QFile::remove(dbFilePath));
    }
    
    {
        static int instances = 0;
        db = QSqlDatabase::addDatabase(kQSQLITE, QString("NonUniformTileBuilderTest_%1").arg(instances++));
        db.setDatabaseName(dbFilePath);
        if (!db.open()) {
            qWarning() << "Could not open db file:" << dbFilePath;
        }
    }

    NonUniformTilesInfoDao rangeDao(&db);
    QCOMPARE(rangeDao.createTable(), kNoErr);
    QCOMPARE(rangeDao.save(range), kNoErr);
    
    NonUniformTileStoreSqlite dbStore(&db);
    dbStore.init();

    NonUniformTileBuilder builder(range);
    QElapsedTimer et2;
    et2.start();
    builder.buildNonUniformTilesNG(reader, converter, &dbStore, type);
    qDebug() << "Done in" << et2.elapsed();
    
}

void NonUniformTileBuilderTest::testTileContentBorderValues()
{
    NonUniformTileRange range;
    range.setMz(380.0, 440.0);
    range.setMzTileLength(30.0);

    range.setScanIndex(0, 0);
    range.setScanIndexLength(1);

    NonUniformTileStore::ContentType type = NonUniformTileStore::ContentMS1Raw;
    NonUniformTileStoreMemory ram;

    point2dList testingPoints = {
        QPointF(380.0, 0),
        QPointF(380.1, 1),
        QPointF(409.99999999999, 2),
        QPointF(410.0, 3),
        QPointF(439.99, 4),
        QPointF(440.00, 5),
    };

    MSReader * reader = initReader();
    for (QSharedPointer<MSReaderBase> item : reader->m_vendorList){
        MSReaderTesting* testReader = dynamic_cast<MSReaderTesting*> (item.data());
        if (testReader) {
            testReader->setEnableCanOpen(true);
            break;
        }
    }
    
    QString virtualFileName = QString(PMI_TEST_FILES_OUTPUT_DIR) + "/" + MSReaderTesting::MAGIC_FILENAME;

    if (!QFileInfo(virtualFileName).exists()) {
        QDir dir(PMI_TEST_FILES_OUTPUT_DIR); 
        QVERIFY(dir.mkpath(MSReaderTesting::MAGIC_FILENAME));
    }

    QCOMPARE(reader->openFile(virtualFileName), kNoErr);

    for (QSharedPointer<MSReaderBase> item : reader->m_openedReaderList) {
        MSReaderTesting* testReader = dynamic_cast<MSReaderTesting*> (item.data());
        if (testReader) {
            testReader->setScanData(testingPoints);
            break;
        }
    }

    ScanIndexNumberConverter converter = ScanIndexNumberConverter::fromMSReader(reader);

    NonUniformTileBuilder builder(range);
    builder.buildNonUniformTiles(reader, converter, &ram, type);
    
    QCOMPARE(range.tileCountX(), 3);

    NonUniformTileManager manager(&ram);
    RandomNonUniformTileIterator iterator(&manager, range, false);
    
    iterator.moveTo(0, 380.0);
    point2dList tile1 = iterator.value();
    point2dList expected1 = {
        testingPoints.at(0),
        testingPoints.at(1),
        testingPoints.at(2)
    };
    QCOMPARE(tile1, expected1);

    iterator.moveTo(0, 410.0);
    point2dList tile2 = iterator.value();
    point2dList expected2 = {
        testingPoints.at(3),
        testingPoints.at(4)
    };
    QCOMPARE(tile2, expected2);


    iterator.moveTo(0, 440.0);
    point2dList tile3 = iterator.value();
    point2dList expected3 = {
        testingPoints.at(5)
    };
    QCOMPARE(tile3, expected3);

}



_PMI_END

QTEST_MAIN(pmi::NonUniformTileBuilderTest)


#include "NonUniformTileBuilderTest.moc"
