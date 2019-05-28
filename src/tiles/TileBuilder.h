#ifndef TILE_BUILDER_H
#define TILE_BUILDER_H

#include "pmi_core_defs.h"
#include <QString>
#include "Tile.h"
#include "TileRange.h"
#include "pmi_common_ms_export.h"

_PMI_BEGIN

class MSReader;
class TileDataProvider;
class TileStore;
class RandomTileIterator;

class PMI_COMMON_MS_EXPORT TileProgressCalculator{
public:
    TileProgressCalculator() :m_processedTilesTotal(0){
    
    }

    static quint64 tileBytes(quint64 tileCount) { return tileCount * Tile::WIDTH * Tile::HEIGHT * sizeof(double); };
    // approximate 
    static quint64 bytesToMegabytes(quint64 bytes) { return quint64(bytes / (1000 * 1000.0)); }

    void updateLevelInfo(const QPoint &level, int totatTileCount){
        if (level != m_level){
            m_level = level;
            m_levelTotalTilesCount = totatTileCount;
            m_processedTilesTotal = 0;
        }
    }
    
    void addProcessedTiles(qint64 processed, qint64 milisecondsElapsed) {
        m_processedTiles = processed;
        m_milisecondsElapsed = milisecondsElapsed;

        m_processedTilesTotal += processed; 
    }

    int levelProgress() { return qRound((m_processedTilesTotal / double(m_levelTotalTilesCount)) * 100.0); }

    double tilesPerSecond() { return (m_processedTiles / double(m_milisecondsElapsed)) * 1000.0; }

    qint64 remainingTiles() { return m_levelTotalTilesCount - m_processedTilesTotal; }

private:
    qint64 m_processedTilesTotal;
    qint64 m_levelTotalTilesCount;
    QPoint m_level;

    qint64 m_processedTiles;
    qint64 m_milisecondsElapsed; //ms
};

class PMI_COMMON_MS_EXPORT TileBuilderProgressReporter : public QObject {
    Q_OBJECT

public:
    TileBuilderProgressReporter() 
        :m_prevLevel(0), 
         m_processedTilesTotal(0){}
    
public slots:
    void processProgressChange(const QPoint &level, int totalTileCount, int processedTiles, qint64 milisecondsElapsed);
private:
    int m_prevLevel;
    int m_processedTilesTotal;
    TileProgressCalculator m_calc;
};

class PMI_COMMON_MS_EXPORT TileBuilder : public QObject {
    Q_OBJECT

public:
    TileBuilder(const TileRange &range);
    void buildLevel1Tiles(TileStore * store, MSReader * reader);
    void checkProgress(TileStore * store, MSReader * reader);

    void buildLevel1Tiles(TileStore * store, const QImage &img);

    void buildLevel1Tiles(TileStore * store, const QImage &even, const QImage &odd);

    void buildLevel1Tiles(TileStore * store, TileDataProvider * provider);


    void buildTilePyramid(int lastLavel, TileStore * store);

    // building document 
    void buildRipTilePyramid(const QPoint &lastLevel, TileStore * store);

    // @param parentLevel - level where we will resample input data
    // @param level - target level that we are building
    // @param store - storage for tails
    void buildLevel(const QPoint &parentLevel, const QPoint &level, TileStore * store);

signals:
    void progressChanged(const QPoint &level, int totalTileCount, int processedTiles, qint64 milisecondsElapsed);

private:
    Tile computeTile(const QPoint &pos, RandomTileIterator &iterator, const QPoint &parentLevel, const QPoint &level);
    Tile computeTileNG(const QPoint &pos, RandomTileIterator &iterator, const QPoint &parentLevel, const QPoint &level);



private:
    TileRange m_range;
};

_PMI_END

#endif // TILE_BUILDER_H
