#include <QtTest>
#include "pmi_core_defs.h"

#include "Tile.h"
#include "TileManager.h"
#include "TileStoreMemory.h"
#include "RandomBilinearTileIterator.h"

_PMI_BEGIN

class RandomBilinearTileIteratorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testBilinearInterpolation();
};

typedef QPair<QPoint, double> PositionValue;
void RandomBilinearTileIteratorTest::testBilinearInterpolation()
{
    QVector<double> data;
    for (int y = 0; y < Tile::HEIGHT; y++) {
        for (int x = 0; x < Tile::WIDTH; x++) {
            data.push_back(0.0); // value computed from position - index
        }
    }

    // TODO write-tile-iterator (we have only read-only one)
    // example taken from https://en.wikipedia.org/wiki/Bilinear_interpolation#Application_in_image_processing
    QVector<PositionValue> values = {
        PositionValue(QPoint(14, 20), 91.0),
        PositionValue(QPoint(15, 20), 210.0),
        PositionValue(QPoint(14, 21), 162.0),
        PositionValue(QPoint(15, 21), 95.0)
    };
    
    for (const PositionValue &v : values) {
        data[v.first.y() * Tile::WIDTH + v.first.x()] = v.second;
    }

    Tile t(TileInfo(QPoint(1,1), QPoint(0, 0)), data);
    TileStoreMemory tsm;
    tsm.saveTile(t);

    TileManager tileManager(&tsm);
    RandomBilinearTileIterator rti(&tileManager);
    rti.moveTo(QPoint(1,1), 14.5, 20.2);
    QCOMPARE(rti.value(), 146.1);
}

_PMI_END

QTEST_MAIN(pmi::RandomBilinearTileIteratorTest)

#include "RandomBilinearTileIteratorTest.moc"
