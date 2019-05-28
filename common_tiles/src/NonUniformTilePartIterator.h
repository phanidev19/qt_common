/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NONUNIFORM_TILE_PART_ITERATOR_H
#define NONUNIFORM_TILE_PART_ITERATOR_H

#include "pmi_common_tiles_export.h"

#include <common_math_types.h>
#include <pmi_core_defs.h>

_PMI_BEGIN

/**
 * @brief Provides Java-style iterator to part of the NonUniform tile
 *
 * NonUniform tile contains N vectors of mz, intensity. This class abstracts iterating
 * over this vectors.
 */
class PMI_COMMON_TILES_EXPORT NonUniformTilePartIterator
{
public:
    NonUniformTilePartIterator(const point2dList &scanPart, double mzStart, double mzEnd);

    int x() const { return m_tilePartX; };

    bool isNull() const;

    bool hasNext() const { return (m_tilePartX + 1) < m_tilePartEndX; };

    point2d next()
    {
        advance();
        return m_lastValue;
    }

    point2d value() { return m_lastValue; }

    int numContiguousCols() const;

private:
    void init(double mzStart, double mzEnd);

    void advance();

private:
    int m_tilePartX;
    int m_tilePartEndX;
    point2dList m_tilePart;
    point2d m_lastValue;
};

_PMI_END

#endif // NONUNIFORM_TILE_PART_ITERATOR_H
