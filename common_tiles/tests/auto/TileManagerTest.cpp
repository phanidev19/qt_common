#include <QtTest>
#include "pmi_core_defs.h"
#include "TileManager.h"
#include "TileStoreMemory.h"
#include "TileStoreSqlite.h"
#include "RandomTileIterator.h"

_PMI_BEGIN

class TileManagerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testLoadNullTile();
    void testNormalizeRect();
    void testNormalizeRect_data();

};


void TileManagerTest::testLoadNullTile()
{
    TileStoreMemory tsm;
    TileManager tm(&tsm);
    Tile t = tm.fetchTile(QPoint(1, 1),  0, 0);
    QVERIFY(t.isNull());
}

void TileManagerTest::testNormalizeRect()
{
    
    QFETCH(QRect, area);
    QFETCH(QRect, tileNormalizedArea);
    
    QCOMPARE(TileManager::normalizeRect(area), tileNormalizedArea);
}

void TileManagerTest::testNormalizeRect_data()
{
    QTest::addColumn<QRect>("area");
    QTest::addColumn<QRect>("tileNormalizedArea");

    QTest::newRow("Exact tile") << QRect(64, 64, Tile::WIDTH, Tile::HEIGHT) << QRect(64, 64, Tile::WIDTH, Tile::HEIGHT);
    QTest::newRow("Sub-area of tile") << QRect(QPoint(2, 2), QPoint(58, 58)) << QRect(0, 0, Tile::WIDTH, Tile::HEIGHT);
    QTest::newRow("Four tiles") << QRect(QPoint(2, 2), QPoint(68, 68)) << QRect(0, 0, Tile::WIDTH * 2, Tile::HEIGHT * 2);

}


_PMI_END

QTEST_MAIN(pmi::TileManagerTest)


#include "TileManagerTest.moc"
