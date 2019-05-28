#include <QDebug>
#include "MSCentroidCache.h"

_PMI_BEGIN

MSCentroidCache::MSCentroidCache () {
}

void MSCentroidCache::clear() {
    m_scanNumber_points.clear();
}

void MSCentroidCache::setScanCache(long scanNumber, const point2dList & points)
{
    m_scanNumber_points.insert(scanNumber, points);
}

bool MSCentroidCache::isCached(long scanNumber) const {
    return m_scanNumber_points.contains(scanNumber);
}

QHash<long, point2dList>::const_iterator MSCentroidCache::getCacheIt(long scanNumber, bool * found) const {
    *found = false;

    QHash<long, point2dList>::const_iterator returnIt = m_scanNumber_points.find(scanNumber);
    if (returnIt != m_scanNumber_points.end()) {
        *found = true;
    }
    return returnIt;
}

bool MSCentroidCache::isValidIt(QHash<long, point2dList>::const_iterator it) const {
    bool returnVal = false;

    if (it != m_scanNumber_points.end()) {
        returnVal = true;
    }
    return returnVal;
}

_PMI_END
