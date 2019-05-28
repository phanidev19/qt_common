#include <QtTest>
#include "pmi_core_defs.h"

#include "Tile.h"
#include "RandomTileIterator.h"
#include "TileStoreMemory.h"
#include "TileManager.h"

#include "TileTestUtils.h"

_PMI_BEGIN

class RandomTileIteratorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testIterate();


};

void RandomTileIteratorTest::testIterate()
{
    QPoint level(1,1);
    Tile t = TestUtils::createIndexedTile(level, QPoint(0, 0));
    TileStoreMemory tsm;
    tsm.saveTile(t);

    TileManager tileManager(&tsm);
    RandomTileIterator rti(&tileManager);
    
    for (int y = 0; y < Tile::HEIGHT; y++) {
        for (int x = 0; x < Tile::WIDTH; x++) {
            rti.moveTo(level, x, y);
            double expected = y * Tile::WIDTH + x;
            QCOMPARE(rti.value(), expected);
        }
    }
}

_PMI_END

QTEST_MAIN(pmi::RandomTileIteratorTest)

#include "RandomTileIteratorTest.moc"
