#ifndef MSCENTROIDCACHEMANAGER_H
#define MSCENTROIDCACHEMANAGER_H

#include <QHash>
#include "common_errors.h"

_PMI_BEGIN

//! @internal
class MSCentroidCacheManager
{
public:
    MSCentroidCacheManager();
    ~MSCentroidCacheManager();

    void clear();

    void setScanCache(const QString & msPath, long scanNumber, const point2dList & points);
    bool isCached(const QString & msPath, long scanNumber) const;
    QHash<long, point2dList>::const_iterator getCacheIt(const QString & msPath, long scanNumber, bool * found) const;

    void setPointSizeMax(double maxVal) {
        m_pointSizeMaxSize = maxVal;
    }

    double pointSizeMax() const {
        return m_pointSizeMaxSize;
    }

private:
    class Private;
    Private * const d;
    double m_pointSize;
    double m_pointSizeMaxSize;
};

_PMI_END

#endif // MSCENTROIDCACHEMANAGER_H
