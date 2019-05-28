#include <QtTest>
#include "pmi_core_defs.h"
#include "TileStoreSqlite.h"
#include "TileManager.h"
#include "RandomTileIterator.h"
#include "MSReader.h"
#include "MSReaderInfo.h"
#include "TileRange.h"

#include "PMiTestUtils.h"

_PMI_BEGIN

static const QString CONA_TILES_LEVEL_1_8("cona_tmt0saxpdetd_level_1_8.db3");

class RandomTileIteratorBenchmark : public QObject
{
    Q_OBJECT

public:
    RandomTileIteratorBenchmark();
    RandomTileIteratorBenchmark(const QStringList &args);

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    
    void testIterator();
    void testIterateStore();

private:
    int m_width = 0;
    int m_height = 0;
    TileStoreSqlite * m_tileStore = nullptr;
    QDir m_testDataBasePath;
};

RandomTileIteratorBenchmark::RandomTileIteratorBenchmark(const QStringList &args) : m_testDataBasePath(args[0])
{
    
}

void RandomTileIteratorBenchmark::initTestCase()
{
    QString dbFileName = m_testDataBasePath.filePath(CONA_TILES_LEVEL_1_8);
    QVERIFY(QFileInfo(dbFileName).exists());
    m_tileStore = new TileStoreSqlite(dbFileName);
}


void RandomTileIteratorBenchmark::cleanupTestCase()
{
    delete m_tileStore;
    m_tileStore = nullptr;
}

void RandomTileIteratorBenchmark::testIterator()
{
    TileManager tileManager(m_tileStore);
    
    double min = 0;
    double max = 0;

    QPoint level(1,1);

    TileRange range;
    TileRange::loadRange(&m_tileStore->db(), &range);

    // iterate 10x10 tiles
    int realWidth = 10 * Tile::WIDTH;
    int realHeight = 10 * Tile::HEIGHT;

    qDebug() << realWidth << realHeight;

    QBENCHMARK_ONCE {
        RandomTileIterator rti(&tileManager);
        for (int y = 0; y < realHeight; ++y) {
            for (int x = 0; x < realWidth; ++x) {
                
                rti.moveTo(level, x, y);
                double currentValue = rti.value();

                if (currentValue < min) {
                    min = currentValue;
                }

                if (currentValue > max) {
                    max = currentValue;
                }
            }
        }
    }
    qDebug() << "Min" << min;
    qDebug() << "Max" << max;
}

void RandomTileIteratorBenchmark::testIterateStore()
{
    TileRange range;
    TileRange::loadRange(&m_tileStore->db(), &range);

    int realWidth = range.tileCountX() * Tile::WIDTH;
    int realHeight = range.tileCountY() * Tile::HEIGHT;

    qDebug() << realWidth << realHeight;

    double min = 0;
    double max = 0;
        
    QBENCHMARK_ONCE {
        for (int y = 0; y < range.tileCountY(); ++y) {
            int posY = y * Tile::HEIGHT;
            for (int x = 0; x < range.tileCountX(); ++x) {
                int posX = x * Tile::WIDTH;

                Tile t = m_tileStore->loadTile(QPoint(1,1), QPoint(posX, posY));
                if (t.isNull()) {
                    qWarning() << "Skipping null tile at" << x << y;
                    continue;
                }

                auto minMax = std::minmax_element(t.data().cbegin(), t.data().cend());
                if (*minMax.first < min){
                    min = *minMax.first;
                }

                if (*minMax.second > max){
                    max = *minMax.second;
                }
            }
        }
    }
    
    qDebug() << "Min" << min;
    qDebug() << "Max" << max;
}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::RandomTileIteratorBenchmark, QStringList() << "Remote Data Folder")

#include "RandomTileIteratorBenchmark.moc"
