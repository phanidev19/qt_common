#include <QtTest>
#include "pmi_core_defs.h"
#include "Tile.h"
#include "TileTestUtils.h"


_PMI_BEGIN

class TileTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testNull();
    void testSerializeDeserialize();
    void extractColorsFromColormap();
    void testSaveToFile();
    void benchmarkSerializeData();
};

void TileTest::testNull()
{
    Tile t;
    QVERIFY(t.isNull());
}


void TileTest::testSaveToFile()
{
    TileInfo sampleTileInfo(QPoint(1,1), QPoint(0, 0));
    Tile indexedTile = TestUtils::createIndexedTile(sampleTileInfo);
    QVERIFY(saveDataToFile(indexedTile, "out.csv"));
}


void TileTest::benchmarkSerializeData()
{
    TileInfo sampleTileInfo(QPoint(1,1), QPoint(0, 0));
    Tile indexedTile = TestUtils::createIndexedTile(sampleTileInfo);

    QBENCHMARK {
        bool countIsOdd = false; 
        for (int i = 0; i < 1000; i++) {
            QByteArray data = indexedTile.serializedData(Tile::COMPRESSED_BYTES);
        }
    }

}

void TileTest::extractColorsFromColormap()
{
    QImage img("C:/work/colormap_jet.png");
    //434x27
    int countOfColors = qRound((img.width() - 2) / 7.0) - 1;
    qDebug() << countOfColors;
    int i = 0;
    for (int x = 2; x < img.width(); x += 7) {
        
        
        QRgb rgb = img.pixel(x, 13);
        QColor c = QColor(rgb);
        QString name = c.name(QColor::HexRgb);
        name.replace("#", "0x");

        
        qreal pos = i / qreal(countOfColors);
        i++;
        // addColorStop(0, 0x00008f)
        qDebug().nospace() << QString("addColorStop(%1, %2);").arg(pos).arg(name);

    }

}

void pmi::TileTest::testSerializeDeserialize()
{
    TileInfo sampleTileInfo(QPoint(1,1), QPoint(0, 0));
    Tile expected = TestUtils::createIndexedTile(sampleTileInfo);

    QByteArray serialized = expected.serializedData(Tile::COMPRESSED_BYTES);
    QVector<double> data = Tile::deserializedData(serialized, Tile::COMPRESSED_BYTES);

    Tile actual(sampleTileInfo, data);
    QCOMPARE(actual, expected);

    serialized = expected.serializedData(Tile::RAW_BYTES);
    data = Tile::deserializedData(serialized, Tile::RAW_BYTES);
    actual = Tile(sampleTileInfo, data);
    QCOMPARE(actual, expected);
}

_PMI_END

QTEST_MAIN(pmi::TileTest)

#include "TileTest.moc"
