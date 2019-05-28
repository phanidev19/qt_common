#include "TileBuilder.h"

#include "MSReader.h"
#include "Tile.h"
#include "ResamplingIterator.h"
#include "TileStore.h"
#include "TileManager.h"
#include "RandomTileIterator.h"
#include <QElapsedTimer>
#include <QtMath>
#include "ImageTileIterator.h"
#include "ImagePatternTileIterator.h"
#include "TileDataProvider.h"

_PMI_BEGIN

TileBuilder::TileBuilder(const TileRange& range)
    :    m_range(range)
{
}

void TileBuilder::buildLevel1Tiles(TileStore * store, MSReader * reader)
{
    Q_ASSERT(store != nullptr);
    Q_ASSERT(reader != nullptr);

    int tileCountX = m_range.tileCountX();
    int tileCountY = m_range.tileCountY();
    int totalTileCount = tileCountX * tileCountY;

    ResamplingIterator ri(reader, m_range);

    const QPoint level(1, 1);
    QElapsedTimer et;
    et.start();
    
    store->start(); // begin transaction
    int processedTiles = 0;
    const int TILE_COUNT_REPORT_PROGRESS = 4000;
    for (int y = 0; y < tileCountY; ++y) {
        qDebug() << "Row" << y << "of" << tileCountY;
        int posY = y * Tile::HEIGHT;
        for (int x = 0; x < tileCountX; ++x) {
            int posX = x * Tile::WIDTH;
            QVector<double> tileData;
            tileData.reserve(Tile::WIDTH * Tile::HEIGHT);
            QRect tileRect(posX, posY, Tile::WIDTH, Tile::HEIGHT);
            ri.fetchTileData(tileRect, &tileData);

            Tile t = Tile(level, tileRect.topLeft(), tileData);
            if (!store->saveTile(t)) {
                qWarning() << "Failed to store tile at " << tileRect.topLeft();
            }
            
            processedTiles++;
            if (processedTiles % TILE_COUNT_REPORT_PROGRESS == 0) {
                qint64 milis = et.elapsed();
                emit progressChanged(level, totalTileCount, TILE_COUNT_REPORT_PROGRESS, milis);
                et.start();
            }

        }
    }
    store->end(); // end transaction
}

void TileBuilder::buildLevel1Tiles(TileStore *store, const QImage &img)
{
    int tileCountX = m_range.tileCountX();
    int tileCountY = m_range.tileCountY();

    ImageTileIterator imageIterator(img);

    const QPoint level(1,1);
    QElapsedTimer et;
    for (int y = 0; y < tileCountY; ++y) {
        qDebug() << "Row" << y;
        int posY = y * Tile::HEIGHT;
        et.start();
        for (int x = 0; x < tileCountX; ++x) {
            int posX = x * Tile::WIDTH;
            QVector<double> tileData;
            QRect tileRect(posX, posY, Tile::WIDTH, Tile::HEIGHT);
            imageIterator.fetchTileData(tileRect, &tileData);

            Tile t = Tile(level, tileRect.topLeft(), tileData);
            if (!store->saveTile(t)) {
                qWarning() << "Failed to store tile at " << tileRect.topLeft();
            }
        }
        qint64 milis = et.elapsed();
        qDebug() << tileCountX << "in" << milis << "ms";
        qDebug() << qreal(milis) / tileCountX << "ms per tile";
    }
}

void TileBuilder::buildLevel1Tiles(TileStore *store, const QImage &even, const QImage &odd)
{
    int tileCountX = m_range.tileCountX();
    int tileCountY = m_range.tileCountY();

    ImagePatternTileIterator imageIterator(even, odd);

    const QPoint level(1, 1);
    QElapsedTimer et;
    for (int y = 0; y < tileCountY; ++y) {
        qDebug() << "Row" << y;
        int posY = y * Tile::HEIGHT;
        et.start();
        for (int x = 0; x < tileCountX; ++x) {
            int posX = x * Tile::WIDTH;
            QVector<double> tileData;
            QRect tileRect(posX, posY, Tile::WIDTH, Tile::HEIGHT);
            imageIterator.fetchTileData(tileRect, &tileData);

            Tile t = Tile(level, tileRect.topLeft(), tileData);
            if (!store->saveTile(t)) {
                qWarning() << "Failed to store tile at " << tileRect.topLeft();
            }
        }
        qint64 milis = et.elapsed();
        qDebug() << tileCountX << "in" << milis << "ms";
        qDebug() << qreal(milis) / tileCountX << "ms per tile";
    }

}


void TileBuilder::buildLevel1Tiles(TileStore * store, TileDataProvider * provider)
{
    int tileCountX = m_range.tileCountX();
    int tileCountY = m_range.tileCountY();
    
    const QPoint level(1,1);
    QElapsedTimer et;
    for (int y = 0; y < tileCountY; ++y) {
        qDebug() << "Row" << y;
        int posY = y * Tile::HEIGHT;
        et.start();
        for (int x = 0; x < tileCountX; ++x) {
            int posX = x * Tile::WIDTH;
            QVector<double> tileData;
            QRect tileRect(posX, posY, Tile::WIDTH, Tile::HEIGHT);
            provider->fetchTileData(tileRect, &tileData);

            Tile t = Tile(level, tileRect.topLeft(), tileData);
            if (!store->saveTile(t)) {
                qWarning() << "Failed to store tile at " << tileRect.topLeft();
            }
        }
        qint64 milis = et.elapsed();
        qDebug() << tileCountX << "in" << milis << "ms";
        qDebug() << qreal(milis) / tileCountX << "ms per tile";
    }

}

void TileBuilder::buildLevel(const QPoint &parentLevel, const QPoint &level, TileStore * store)
{
    int width = m_range.levelWidth(level);
    int height = m_range.levelHeigth(level);

    // TODO: see TileRange, move to TileManager or Tile
    int tileCountX = qCeil(qreal(width) / Tile::WIDTH);
    int tileCountY = qCeil(qreal(height) / Tile::HEIGHT);
    int totalTileCount = tileCountX * tileCountY;

    qDebug() << "For level" << level << "size is" << QSize(width, height) << "Tile size" << QSize(tileCountX, tileCountY);
    qDebug() << "Building level " << level << "tile count:" << tileCountX * tileCountY << "WxH" << width << height;
    
    TileManager tm(store);
    RandomTileIterator iterator(&tm);

    QElapsedTimer et;
    et.start();
    store->start();
    int processedTiles = 0;
    const int TILE_COUNT_REPORT_PROGRESS = 400;

    for (int y = 0; y < tileCountY; ++y) {
        qDebug() << "level=" << level << "row=" << y << "/" << tileCountY;
        int posY = y * Tile::HEIGHT;
        for (int x = 0; x < tileCountX; ++x) {
            QPoint tilePos(x * Tile::WIDTH, posY);

            if (!store->contains(level, tilePos)) {
                iterator.clearCache();
                Tile t = computeTileNG(tilePos, iterator, parentLevel, level);

                bool saved = store->saveTile(t);
                if (!saved) {
                    qWarning() << "Cannot save tile on level" << level << "at" << x << "," << y;
                }

                processedTiles++;
                if (processedTiles % TILE_COUNT_REPORT_PROGRESS == 0) {
                    qint64 milis = et.elapsed();
                    emit progressChanged(level, totalTileCount, TILE_COUNT_REPORT_PROGRESS, milis);
                    et.start();
                }

            }
        }
    }
    store->end();

    qint64 milis = et.elapsed();
    emit progressChanged(level, totalTileCount, processedTiles % TILE_COUNT_REPORT_PROGRESS, milis);
    et.start();


}


void TileBuilder::buildTilePyramid(int lastLevel, TileStore * store)
{
   Q_ASSERT(store != nullptr);

   int parentLevel = 1;
   int level = 2;

   TileManager tm(store);
   RandomTileIterator iterator(&tm);
   while (level <= lastLevel) {
       
       QPoint level2d(level, level);
       int width = m_range.levelWidth(level2d);
       int height = m_range.levelHeigth(level2d);

       // TODO: see TileRange, move to TileManager or Tile
       int tileCountX = qCeil(qreal(width) / Tile::WIDTH);
       int tileCountY = qCeil(qreal(height) / Tile::HEIGHT);
       int totalTileCount = tileCountX * tileCountY;

       qDebug() << "For level" << level << "size is" << QSize(width, height) << "Tile size" << QSize(tileCountX, tileCountY);
       qDebug() << "Building level " << level << "tile count:" << tileCountX * tileCountY << "WxH" << width << height;
       QElapsedTimer et;
       et.start();
       store->start();
       int processedTiles = 0;
       const int TILE_COUNT_REPORT_PROGRESS = 400;

       for (int y = 0; y < tileCountY; ++y) {
           qDebug() << "level=" << level << "row=" << y << "/" << tileCountY;
           int posY = y * Tile::HEIGHT;
           for (int x = 0; x < tileCountX; ++x) {
               QPoint tilePos(x * Tile::WIDTH, posY);
               
               if (!store->contains(level2d, tilePos)) {
                   iterator.clearCache();
                   Tile t = computeTile(tilePos, iterator, QPoint(parentLevel, parentLevel), QPoint(level, level));
                  
                   bool saved = store->saveTile(t);
                   if (!saved) {
                       qWarning() << "Cannot save tile on level" << level << "at" << x << "," << y;
                   }
      
                   processedTiles++;
                   if (processedTiles % TILE_COUNT_REPORT_PROGRESS == 0) {
                       qint64 milis = et.elapsed();
                       emit progressChanged(level2d, totalTileCount, TILE_COUNT_REPORT_PROGRESS, milis);
                       et.start();
                   }

               }
           }
       }
       store->end();

       qint64 milis = et.elapsed();
       emit progressChanged(level2d, totalTileCount, processedTiles % TILE_COUNT_REPORT_PROGRESS, milis);
       et.start();
       
       parentLevel = level;
       level++;
   }
}


void TileBuilder::buildRipTilePyramid(const QPoint &lastLevel, TileStore * store)
{
    // start from tile 1,1 and go 
    QPoint parentLevel(1, 1);
    QPoint nextLevel = parentLevel;

    while  ( (nextLevel.x() <= lastLevel.x()) && (nextLevel.y() <= lastLevel.y()) ) {
    
        if (nextLevel != parentLevel) {
            qDebug() << "\t" << nextLevel << "p:" << parentLevel;
            buildLevel(parentLevel, nextLevel, store);
        }

        // move to the last X level
        QPoint subParent = nextLevel;
        for (int x = nextLevel.x() + 1; x <= lastLevel.x(); ++x) {
            QPoint level = QPoint(x,nextLevel.y());
            qDebug() << level << "p:" << subParent;
            buildLevel(subParent, level, store);
            
            subParent = level;
        }
        
        qDebug() << "\n";

        // move to the last Y level
        subParent = nextLevel;
        for (int y = nextLevel.y() + 1; y <= lastLevel.y(); ++y) {
            QPoint level = QPoint(nextLevel.x(), y);
            qDebug() << level << "p:" << subParent;
            buildLevel(subParent, level, store);

            subParent = level;
        }

        // move major level
        parentLevel = nextLevel;
        nextLevel += QPoint(1, 1);
    }

}

Tile TileBuilder::computeTile(const QPoint &pos, RandomTileIterator &iterator, const QPoint &parentLevel, const QPoint &level)
{
    //TODO fix for rip map
    Q_ASSERT(parentLevel.x() == parentLevel.y());
    Q_ASSERT(level.x() == level.y());

    // compute new tile
    QVector<double> data;
    data.reserve(Tile::WIDTH * Tile::HEIGHT);

    for (int tileY = 0; tileY < Tile::HEIGHT; ++tileY) {
        int globalPosY = pos.y() + tileY;
        for (int tileX = 0; tileX < Tile::WIDTH; ++tileX) {
            int globalPosX = pos.x() + tileX;

            // box filtering , see https://graphics.ethz.ch/teaching/former/vc_master_06/Downloads/Mipmaps_1.pdf slide 10
            iterator.moveTo(parentLevel, globalPosX * 2, globalPosY * 2);
            double topLeft = iterator.value();
            
            iterator.moveTo(parentLevel, globalPosX * 2 + 1, globalPosY * 2);
            double topRight = iterator.value();

            iterator.moveTo(parentLevel, globalPosX * 2, globalPosY * 2 + 1);
            double bottomLeft = iterator.value();

            iterator.moveTo(parentLevel, globalPosX * 2 + 1, globalPosY * 2 + 1);
            double bottomRight = iterator.value();

            double value = (topLeft + topRight + bottomLeft + bottomRight) / 4.0;
            data.push_back(value);
        }
    }

    TileInfo ti(level, pos);
    return Tile(ti, data);
}


Tile TileBuilder::computeTileNG(const QPoint &pos, RandomTileIterator &iterator, const QPoint &parentLevel, const QPoint &level)
{

    bool scaleX = parentLevel.x() < level.x();
    bool scaleY = parentLevel.y() < level.y();

    // compute new tile
    QVector<double> data;
    data.reserve(Tile::WIDTH * Tile::HEIGHT);
    
    //TODO: observe how those for-loops are copy-pasted, implement iterator for tile
    if (scaleX && !scaleY) {
        // Level(1,1) -> Level(2,1) 
        for (int tileY = 0; tileY < Tile::HEIGHT; ++tileY) {
            int globalPosY = pos.y() + tileY;
            for (int tileX = 0; tileX < Tile::WIDTH; ++tileX) {
                int globalPosX = pos.x() + tileX;

                iterator.moveTo(parentLevel, globalPosX * 2, globalPosY);
                double left = iterator.value();

                iterator.moveTo(parentLevel, globalPosX * 2 + 1, globalPosY);
                double right = iterator.value();

                double value = (left + right) / 2.0;

                data.push_back(value);
            }
        }
    } else if (scaleY && !scaleX) {
        // Level(1,1) -> Level(1,2) 
        for (int tileY = 0; tileY < Tile::HEIGHT; ++tileY) {
            int globalPosY = pos.y() + tileY;
            for (int tileX = 0; tileX < Tile::WIDTH; ++tileX) {
                int globalPosX = pos.x() + tileX;

                iterator.moveTo(parentLevel, globalPosX, globalPosY * 2);
                double top = iterator.value();

                iterator.moveTo(parentLevel, globalPosX, globalPosY * 2 + 1);
                double bottom = iterator.value();

                double value = (top + bottom) / 2.0;
                data.push_back(value);
            }
        }
    } else if (scaleX && scaleY) {
        for (int tileY = 0; tileY < Tile::HEIGHT; ++tileY) {
            int globalPosY = pos.y() + tileY;
            for (int tileX = 0; tileX < Tile::WIDTH; ++tileX) {
                int globalPosX = pos.x() + tileX;

                // box filtering , see https://graphics.ethz.ch/teaching/former/vc_master_06/Downloads/Mipmaps_1.pdf slide 10
                iterator.moveTo(parentLevel, globalPosX * 2, globalPosY * 2);
                double topLeft = iterator.value();

                iterator.moveTo(parentLevel, globalPosX * 2 + 1, globalPosY * 2);
                double topRight = iterator.value();

                iterator.moveTo(parentLevel, globalPosX * 2, globalPosY * 2 + 1);
                double bottomLeft = iterator.value();

                iterator.moveTo(parentLevel, globalPosX * 2 + 1, globalPosY * 2 + 1);
                double bottomRight = iterator.value();

                double value = (topLeft + topRight + bottomLeft + bottomRight) / 4.0;
                data.push_back(value);
            }
        }
    }

    TileInfo ti(level, pos);
    return Tile(ti, data);
}

void TileBuilderProgressReporter::processProgressChange(const QPoint &level, int totalTileCount, int processedTiles, qint64 milisecondsElapsed)
{
    m_calc.updateLevelInfo(level, totalTileCount);
    m_calc.addProcessedTiles(processedTiles, milisecondsElapsed);

    qDebug() << "Building level" << level;
    qDebug() << "Level tiles" << totalTileCount;

    qDebug() << "Progress" << m_processedTilesTotal << "/" << totalTileCount << m_calc.levelProgress() << "%";
    double tilesPerSecond = m_calc.tilesPerSecond();
    qDebug() << "Speed (Tile/s)" << tilesPerSecond;

    qint64 MBPerSecond = TileProgressCalculator::bytesToMegabytes(TileProgressCalculator::tileBytes(qRound64(tilesPerSecond)));
    qDebug() << "Speed (MB/s)" << MBPerSecond;

    qint64 todoTilesInSeconds = m_calc.remainingTiles() / tilesPerSecond;

    qint64 minutes = todoTilesInSeconds / 60;
    int seconds = todoTilesInSeconds % 60;
    qDebug() << "Estimated time:" << minutes << "minutes" << seconds << "seconds";
}

_PMI_END
