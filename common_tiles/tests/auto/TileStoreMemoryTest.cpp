#include <QtTest>
#include "pmi_core_defs.h"
#include "TileStoreMemory.h"

_PMI_BEGIN

class TileStoreMemoryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testLoadNullTile();
    void testSaveLoad();
};

void TileStoreMemoryTest::testLoadNullTile()
{
    TileStoreMemory tsm;
    Tile t = tsm.loadTile(QPoint(1,1), QPoint(0, 0));
    QVERIFY(t.isNull());
}

void TileStoreMemoryTest::testSaveLoad()
{
    TileStoreMemory tsm;
    
    // create tile with values 0..4095
    QVector<double> data;
    for (int y = 0; y < Tile::HEIGHT; y++) { 
        for (int x = 0; x < Tile::WIDTH; x++) {
            data.push_back(y * Tile::WIDTH + x);
        }
    }
    

    TileInfo ti(QPoint(1,1), QPoint(0, 0));
    Tile a(ti, data);

    tsm.saveTile(a);

    Tile b = tsm.loadTile(ti.level(), ti.pos());
    QCOMPARE(a, b);


}

_PMI_END

QTEST_MAIN(pmi::TileStoreMemoryTest)

#include "TileStoreMemoryTest.moc"
