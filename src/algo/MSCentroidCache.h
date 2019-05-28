#ifndef MSCENTROIDCACHE_H
#define MSCENTROIDCACHE_H

#include "common_errors.h"
#include "common_math_types.h"
#include "pmi_common_ms_export.h"
#include "config-pmi_qt_common.h"

#include <QHash>

_PMI_BEGIN

//! @internal
class MSCentroidCache
{
public:
    MSCentroidCache();

    void clear();

    void setScanCache(long scanNumber, const point2dList & points);
    bool isCached(long scanNumber) const;
    QHash<long, point2dList>::const_iterator getCacheIt(long scanNumber, bool * found) const;
    bool isValidIt(QHash<long, point2dList>::const_iterator) const;
    //point2dList getCache(long scanNumber, bool * found) const;

    int getScanCacheSize() const {
        return m_scanNumber_points.size();
    }
private:
    QHash<long, point2dList> m_scanNumber_points;
};

_PMI_END

#endif // MSCENTROIDCACHE_H
