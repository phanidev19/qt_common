#include <QDebug>
#include "MSCentroidCache.h"
#include "MSCentroidCacheManager.h"

_PMI_BEGIN

class MSCentroidCacheManager::Private {
public:
    Private() {}
    QHash<QString, MSCentroidCache> msPath_cache;
};

MSCentroidCacheManager::MSCentroidCacheManager()
 : d(new Private), m_pointSize(0), m_pointSizeMaxSize(5000000)
{
}

MSCentroidCacheManager::~MSCentroidCacheManager()
{
    delete d;
}

void MSCentroidCacheManager::clear() {
    //commonMsDebug() << "MSCentroidCacheManager::clear called d->msPath_cache.size()=" << d->msPath_cache.size();
    d->msPath_cache.clear();
    m_pointSize = 0;
}

void MSCentroidCacheManager::setScanCache(const QString & msPath, long scanNumber, const point2dList & points) {
    //commonMsDebug() << "points.size(),m_pointSize,m_pointSizeMaxSize,cacheSize=" << points.size() << "," << m_pointSize << "," << m_pointSizeMaxSize << "," << d->msPath_cache[msPath].getScanCacheSize();
    if ((m_pointSize + points.size()) > m_pointSizeMaxSize) {
        clear();
    }
    m_pointSize += points.size();
    d->msPath_cache[msPath].setScanCache(scanNumber, points);
}

bool MSCentroidCacheManager::isCached(const QString & msPath, long scanNumber) const {
    /*if (d->msPath_cache.contains(msPath)) {
        return d->msPath_cache[msPath].isCached(scanNumber);
    }
    return false;*/

    bool found = false;
    QHash<long, point2dList>::const_iterator iter = this->getCacheIt(msPath, scanNumber, &found);
    return found;

    /*
    MSCentroidCache tempMSCentroidCache = d->msPath_cache.value(msPath);
    return tempMSCentroidCache.isCached(scanNumber);
    */
}

QHash<long, point2dList>::const_iterator MSCentroidCacheManager::getCacheIt(const QString & msPath, long scanNumber, bool * found) const {
    QHash<long, point2dList>::const_iterator it;

    *found = false;
    QHash<QString, MSCentroidCache>::const_iterator tempIt = d->msPath_cache.find(msPath);
    if (tempIt != d->msPath_cache.end()) {
        it = tempIt.value().getCacheIt(scanNumber, found);
    }

    return it;
}

_PMI_END
