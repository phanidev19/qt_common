#include "ResamplingIterator.h"
#include "MSReaderBase.h"
#include "MSReader.h"

#include <QtMath>

_PMI_BEGIN

static const int maxItemsInCache = 128;
static const int INVALID_SCAN_NUMBER_INDEX = -1;
static const double DEFAULT_TILE_DATA_VALUE = 0.0;

const QPoint BASE_LEVEL(1, 1);

ResamplingIterator::ResamplingIterator(MSReader *ms, const TileRange &range)
    : m_reader(ms),
    m_range(range),
    m_tileConverter(range),
    m_scanIndexNumberConverter(ScanIndexNumberConverter::fromMSReader(ms))
{
    m_cache.setMaxCost(maxItemsInCache);
}

ResamplingIterator::~ResamplingIterator()
{
}

// ineffective: effective would be to do scan lines of data and construct tiles
void ResamplingIterator::fetchTileData(const QRect &rc, QVector<double> *data)
{
    int xEnd = rc.x() + rc.width();
    int yEnd = rc.y() + rc.height();

    for (int y = rc.y(); y < yEnd; ++y) {
        // convert global tile position Y to scan number
        
        int scanNumber = INVALID_SCAN_NUMBER_INDEX;
        // @note: Why floor: scan index e.g. 500.5 should be still data from scan index 500
        int scanIndex = qFloor(m_tileConverter.levelToWorldPositionY(y, BASE_LEVEL));
        // the interval is inclusive <minY, maxY>
        if (scanIndex >= m_range.minY() && scanIndex <= m_range.maxY()) {
            scanNumber = m_scanIndexNumberConverter.toScanNumber(scanIndex);
        }
        
        if (scanNumber != INVALID_SCAN_NUMBER_INDEX) {
            PlotBase * pb = fetchPlotBase(scanNumber);
            Q_ASSERT(pb != nullptr);

            for (int x = rc.x(); x < xEnd; ++x) {
                double valueX = m_tileConverter.levelToWorldPositionX(x, BASE_LEVEL);
                double valueY = pb->evaluate(valueX);
                data->push_back(valueY);
            }

        } else {
            // compute "empty" row
            for (int x = rc.x(); x < xEnd; ++x) {
                data->push_back(DEFAULT_TILE_DATA_VALUE);
            }
        }
        
    }
}

PlotBase *ResamplingIterator::fetchPlotBase(int scanNumber)
{
    PlotBase * pb(nullptr);
    if (m_cache.contains(scanNumber)) {
        pb = m_cache.object(scanNumber);
    } else {
        point2dList scanDataRow;
        m_reader->getScanData(scanNumber, &scanDataRow);
        pb = new PlotBase(scanDataRow);
        m_cache.insert(scanNumber, pb);
    }

    return pb;
}




_PMI_END

