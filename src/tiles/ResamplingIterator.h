#ifndef RESAMPLINGITERATOR
#define RESAMPLINGITERATOR

#include "pmi_core_defs.h"
#include "pmi_common_ms_export.h"

#include <PlotBase.h>
#include <TilePositionConverter.h>
#include <TileRange.h>
#include "ScanIndexNumberConverter.h"

#include <QCache>
#include <QVector>
#include <QtGlobal>

class QRect;

_PMI_BEGIN

class MSReader;

class PMI_COMMON_MS_EXPORT ResamplingIterator {
public:
    ResamplingIterator(MSReader *ms, const TileRange &range);
    ~ResamplingIterator();

    void fetchTileData(const QRect &rc, QVector<double> *data);

private:
    void initScanNumbers();
    // for given scanNumber fetches plotbase
    PlotBase *fetchPlotBase(int scanNumber);

private:
    MSReader *m_reader;
    TileRange m_range;
    TilePositionConverter m_tileConverter;
    ScanIndexNumberConverter m_scanIndexNumberConverter;

    QCache<int, PlotBase> m_cache;
};

_PMI_END

#endif
