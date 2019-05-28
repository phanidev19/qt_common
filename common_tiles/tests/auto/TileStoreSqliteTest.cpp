#include <QtTest>
#include "pmi_core_defs.h"

#include "Tile.h"
#include "TileStoreSqlite.h"
#include "MSReader.h"
#include "MSReaderInfo.h"
#include <limits>

_PMI_BEGIN

class TileStoreSqliteTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testRoundTrip();
    void testSaveUniqueTiles();
    void testContains();
    void testAvailableLevels();
};

QVector<double> createSinTile()
{
    QVector<double> dataOut;
    
    int count = Tile::WIDTH * Tile::HEIGHT;
    int i = 0; 
    for (int i = 0; i < count;++i)
    {
        dataOut.push_back(std::sin(i));
    }

    return dataOut;
}

void TileStoreSqliteTest::testRoundTrip()
{
    QString dbFileName = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/tiles.db3");
    TileStoreSqlite tileStore(dbFileName);
    Err e = tileStore.dropTable();
    QCOMPARE(e, kNoErr);
    
    e = tileStore.createTable();
    QCOMPARE(e, kNoErr);

    QPoint level(1,1);
    QPoint pos = QPoint(64, 64);
    QVector<double> data = createSinTile();


    Tile saved(level, pos, data);
    QCOMPARE(tileStore.saveTile(saved), true);

    Tile loaded = tileStore.loadTile(level, pos);
    QCOMPARE(loaded, saved);
}


void TileStoreSqliteTest::testSaveUniqueTiles()
{
    QString dbFileName = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/tiles.db3");
    TileStoreSqlite tileStore(dbFileName);
    QCOMPARE(tileStore.dropTable(), kNoErr);
    QCOMPARE(tileStore.createTable(), kNoErr);

    QPoint level(1,1);
    QPoint pos = QPoint(64, 64);
    QVector<double> data = createSinTile();
    Tile tile(level, pos, data);

    QVERIFY(tileStore.saveTile(tile));
    QVERIFY(!tileStore.saveTile(tile));
}

void TileStoreSqliteTest::testContains()
{
    QString dbFileName = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/tiles.db3");
    TileStoreSqlite tileStore(dbFileName);
    QCOMPARE(tileStore.dropTable(), kNoErr);
    QCOMPARE(tileStore.createTable(), kNoErr);

    QPoint level(1, 1);
    QPoint pos = QPoint(64, 64);

    QCOMPARE(tileStore.contains(level, pos), false);

    QVector<double> data = createSinTile();
    Tile tile(level, pos, data);

    QVERIFY(tileStore.saveTile(tile));
    QCOMPARE(tileStore.contains(level, pos), true);
}

void TileStoreSqliteTest::testAvailableLevels()
{
    QString dbFileName = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/tiles.db3");
    TileStoreSqlite tileStore(dbFileName);
    QCOMPARE(tileStore.dropTable(), kNoErr);
    QCOMPARE(tileStore.createTable(), kNoErr);

    QVector<double> data = createSinTile();
    QPoint pos(64,64);

    QVector<QPoint> expectedLevels = {
        QPoint(1, 1),
        QPoint(2, 2),
        QPoint(3, 3),
        QPoint(4, 4),
        QPoint(5, 5),
        QPoint(6, 6),
        QPoint(7, 7) };
    
    for (int i = 0; i < expectedLevels.size(); ++i) {
        Tile tile(expectedLevels.at(i), pos, data);
        tileStore.saveTile(tile);
    }

    QCOMPARE(tileStore.availableLevels(), expectedLevels);
}



_PMI_END

QTEST_MAIN(pmi::TileStoreSqliteTest)


#include "TileStoreSqliteTest.moc"
