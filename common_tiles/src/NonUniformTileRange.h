/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NON_UNIFORM_TILE_RANGE
#define NON_UNIFORM_TILE_RANGE

#include "pmi_core_defs.h"
#include "pmi_common_tiles_export.h"
#include "TileRange.h"

_PMI_BEGIN

class MzScanIndexRect;

class PMI_COMMON_TILES_EXPORT NonUniformTileRange {

public:
    NonUniformTileRange();

    //! computes how many tiles are stored horizontally in the grid of tiles
    int tileCountX() const;
    int tileCountY() const; 

    void setMz(double mzStart, double mzEnd);
    void setScanIndex(int scanIndexStart, int scanIndexEnd);

    void setMzTileLength(double length) { m_mzTileWidth = length; }
    double mzTileLength() const { return m_mzTileWidth; }

    void setScanIndexLength(int length) { m_scanIndexTileHeight = length; };
    int scanIndexTileLength() const { return m_scanIndexTileHeight; }

    //! @brief converts tile position to the first possible mz in the tile
    double mzAt(int tileX) const;
    
    //! @brief  converts tile position to it's left and right mz boundaries
    // @param tileX for tile position returns respective mz, it's within [mzStart, mzEnd) interval 
    void mzTileInterval(int tileX, double * mzStart, double * mzEnd) const;

    //! \brief converts tile position to first scan index in the tile
    //! @param tileY - tile index between 0..tileCountY()
    int scanIndexAt(int tileY) const;

    //! \brief returns the last scan index in the tile
    // @see also scanIndexInterval
    //! @param tileY - tile index between 0..tileCountY()
    int lastScanIndexAt(int tileY) const;

    //! @brief  converts tile position to it's up and down boundaries
    // The tile contains scan indexes within [scanIndexStart, scanIndexEnd) interval 
    void scanIndexInterval(int tileY, int * scanIndexStart, int * scanIndexEnd) const;
    
    //! @brief for given scanIndex computes the respective offset within tile
    //  Note: scanIndex implies tileY position, @see NonUniformTileRange::tileY()
    int tileOffset(int scanIndex) const;

    int lastTileOffset() const;

    //! @brief return true if the scan index is within particular tile
    bool hasScanIndex(int tileY, int scanIndex);

    //! converts the other way around
    int tileX(double mz) const;
    int tileY(int scanIndex) const;

    double mzMin() const { return m_mzMin; }
    double mzMax() const { return m_mzMax; }

    int scanIndexMin() const { return m_scanIndexMin; }
    int scanIndexMax() const { return m_scanIndexMax; }

    bool operator==(const NonUniformTileRange& rhs) const;

    //! @brief provides tile-index rect for given input
    QRect tileRect(double mzMin, double mzMax, int scanIndexStart, int scanIndexEnd) const;

    //! @brief overloaded function, @see tileRect
    QRect tileRect(const MzScanIndexRect &area) const;

    //! @brief provides the area defined by range in rectangular region
    //!
    //! QRectF::left() and QRectF::right() is in mz unit
    //! QRectF::top() and QRectF::bottom() is in scan index unit
    QRectF area() const;

    //! @brief provides the area defined by range in rectangular region
    // TODO: slowly deprecate area() 
    MzScanIndexRect areaRect() const;

    //! converts the tile rect area to world coordinates
    MzScanIndexRect fromTileRect(const QRect &tileRect) const;

    //! @brief returns true if the area is with-in tile range
    //! TODO: slowly deprecate QRectF based areas
    //! @see @area()
    bool contains(const QRectF &otherArea) const;

    //! @brief returns true if the area is with-in tile range
    //! @see @areaRect()
    bool contains(const MzScanIndexRect &area) const;

    //! @brief returns true for default-constructed NonUniformTileRange
    bool isNull() const;
private:
    //TileRange::computeSize;
    int computeSize(qreal min, qreal max, qreal step) const;

private:
    double m_mzMin;
    double m_mzMax; // step is ommited
    double m_mzTileWidth;

    int m_scanIndexMin;
    int m_scanIndexMax;
    int m_scanIndexTileHeight;
};

_PMI_END

#endif // NON_UNIFORM_TILE_RANGE