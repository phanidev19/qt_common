/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformTileRange.h"

#include "MzScanIndexRect.h"

#include <QRect>
#include <QtGlobal>
#include <QtMath>

_PMI_BEGIN

NonUniformTileRange::NonUniformTileRange() 
    :m_mzMin(0),
     m_mzMax(0),
     m_mzTileWidth(0),
     m_scanIndexMin(0),
     m_scanIndexMax(0),
     m_scanIndexTileHeight(0)
{
}

int NonUniformTileRange::tileCountX() const
{
    return computeSize(m_mzMin, m_mzMax, m_mzTileWidth);
}

int NonUniformTileRange::tileCountY() const
{
    return computeSize(m_scanIndexMin, m_scanIndexMax, m_scanIndexTileHeight);
}

// including this number
void NonUniformTileRange::setMz(double mzStart, double mzEnd)
{
    Q_ASSERT(mzStart <= mzEnd);
    m_mzMin = std::floor(mzStart);
    m_mzMax = std::ceil(mzEnd);
}

void NonUniformTileRange::setScanIndex(int scanIndexStart, int scanIndexEnd)
{
    Q_ASSERT(scanIndexStart <= scanIndexEnd);
    m_scanIndexMin = scanIndexStart;
    m_scanIndexMax = scanIndexEnd;
}

double NonUniformTileRange::mzAt(int tileX) const
{
    return (m_mzMin + tileX * m_mzTileWidth);
}

void NonUniformTileRange::mzTileInterval(int tileX, double * mzStart, double * mzEnd) const
{
    Q_ASSERT(mzStart);
    Q_ASSERT(mzEnd);

    *mzStart = mzAt(tileX);
    *mzEnd = mzAt(tileX + 1);
}

int NonUniformTileRange::scanIndexAt(int tileY) const
{
    return m_scanIndexMin + tileY * m_scanIndexTileHeight;
}

int NonUniformTileRange::lastScanIndexAt(int tileY) const
{
    // -1 because the interval is <firstScanIndexOfTile, lastScanIndexOfTile)
    // since granularity of scan index is 1, we take previous one    
    return scanIndexAt(tileY + 1) - 1;
}

void NonUniformTileRange::scanIndexInterval(int tileY, int * scanIndexStart, int * scanIndexEnd) const
{
    Q_ASSERT(scanIndexStart);
    Q_ASSERT(scanIndexEnd);
    
    *scanIndexStart = scanIndexAt(tileY);
    *scanIndexEnd = scanIndexAt(tileY+1);
}

int NonUniformTileRange::tileOffset(int scanIndex) const
{
    int scanIndexTileY = tileY(scanIndex);
    int offset = scanIndex - scanIndexAt(scanIndexTileY);
    return offset;
}

int NonUniformTileRange::lastTileOffset() const
{
    return m_scanIndexTileHeight - 1;
}

bool NonUniformTileRange::hasScanIndex(int tileY, int scanIndex)
{
    int scanIndexTileY = this->tileY(scanIndex);
    return (tileY == scanIndexTileY);
}

int NonUniformTileRange::tileX(double mz) const
{
    int tileX = qFloor((mz - m_mzMin) / m_mzTileWidth);

    // we need to address fixed precision of the doubles here
    // so we need to do additional work to correctly compute tile index from mz value

    // example
    //mzMin = 0.0, with = 100.0
    //[000.0, 100.0)  <-- tile 0
    //[100.0, 200.0)  <-- tile 1
    
    // mz = 100.0 
    // int tileX = (100-0)/100.0 = 1.000000001 or 0.9987999999 (expected: 1)
    // int tileX = (200-0)/100.0 = 2.000000001 or 1.999999999 (expected: 2)

    // mz = 50.0
    // int tileX = (50-0)/100.0 = 0.5000001 or 0.499999999 (expected: 0)

    double mzMin = mzAt(tileX);
    double mzMax = mzAt(tileX + 1);
    
    if (mzMin <= mz && mz < mzMax) {
        return tileX;
    } else if (mz < mzMin) {
        return tileX - 1;
    } else if (mz >= mzMax) {
        return tileX + 1;
    }
    
    qWarning() << "Unexpected state for mz" << mz;
    return tileX;
}

int NonUniformTileRange::tileY(int scanIndex) const
{
    return scanIndex / m_scanIndexTileHeight;
}

QRect NonUniformTileRange::tileRect(double mzMin, double mzMax, int scanIndexStart,
                                    int scanIndexEnd) const
{
    Q_ASSERT(!isNull());
    
    int tileXStart = tileX(mzMin);
    int tileXEnd = tileX(mzMax);
    int tileCountX = tileXEnd - tileXStart + 1;

    int tileYStart = tileY(scanIndexStart);
    int tileYEnd = tileY(scanIndexEnd);
    int tileCountY = tileYEnd - tileYStart + 1;

    return QRect(tileXStart, tileYStart, tileCountX, tileCountY);
}

QRect NonUniformTileRange::tileRect(const MzScanIndexRect &area) const
{
    return tileRect(area.mz.start(), area.mz.end(), area.scanIndex.start(), area.scanIndex.end());
}

QRectF NonUniformTileRange::area() const
{
    Q_ASSERT(!isNull());
    
    QRectF result;

    result.setLeft(mzMin());
    result.setRight(mzMax());
    result.setTop(scanIndexMin());
    result.setBottom(scanIndexMax());

    return result;
}

MzScanIndexRect NonUniformTileRange::areaRect() const
{
    MzScanIndexRect result;
    result.mz = MzInterval(m_mzMin, m_mzMax);
    result.scanIndex = ScanIndexInterval(m_scanIndexMin, m_scanIndexMax);
    return result;
}

MzScanIndexRect NonUniformTileRange::fromTileRect(const QRect &tileRect) const
{
    MzScanIndexRect result;
    
    int tileStartX = tileRect.left();
    int tileEndX = tileRect.right();

    // interval is [mzStart, mzEnd)
    result.mz.rstart() = mzAt(tileStartX);
    result.mz.rend() = mzAt(tileEndX + 1);

    int tileYStart = tileRect.top();
    int tileYEnd = tileRect.bottom();

    // interval is [scanIndexStart, scanIndexEnd]
    result.scanIndex.rstart() = scanIndexAt(tileYStart);
    result.scanIndex.rend() = lastScanIndexAt(tileYEnd);
 
    return result;
}

bool NonUniformTileRange::contains(const QRectF &otherArea) const
{
    QRectF currentArea = area();
    return currentArea.contains(otherArea);
}

bool NonUniformTileRange::contains(const MzScanIndexRect &area) const
{
    const MzInterval &mz = area.mz;

    if (!MzInterval(m_mzMin, m_mzMax).contains(area.mz)) {
        return false;
    }

    if (!ScanIndexInterval(m_scanIndexMin, m_scanIndexMax).contains(area.scanIndex)) {
        return false;
    }

    return true;
}

bool NonUniformTileRange::isNull() const
{
    bool scanIndexIsNull = (m_scanIndexMin == m_scanIndexMax == m_scanIndexTileHeight == 0);

    bool mzIsNull;
    if (scanIndexIsNull) {
        // check if mz is all null
        mzIsNull = qFuzzyCompare(m_mzMin, m_mzMax) && qFuzzyCompare(m_mzMax, m_mzTileWidth)
            && qFuzzyCompare(1 + m_mzTileWidth, 1 + 0.0);
    } else {
        mzIsNull = false;
    }

    return mzIsNull;
}

bool NonUniformTileRange::operator==(const NonUniformTileRange& rhs) const
{
    return m_mzMin == rhs.m_mzMin &&
        m_mzMax == rhs.m_mzMax &&
        m_mzTileWidth == rhs.m_mzTileWidth &&
        m_scanIndexMin == rhs.m_scanIndexMin &&
        m_scanIndexMax == rhs.m_scanIndexMax &&
        m_scanIndexTileHeight == rhs.m_scanIndexTileHeight;

}

int NonUniformTileRange::computeSize(qreal min, qreal max, qreal step) const
{
    return qFloor((max - min + step) / step);
}

_PMI_END


