/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "HillClusterCsvFormatter.h"
#include "NonUniformFeature.h"
#include "NonUniformFeatureFindingSession.h"
#include "NonUniformHillCluster.h"
#include "NonUniformTileDevice.h"
#include "NonUniformTileRange.h"

#include <QColor>

_PMI_BEGIN

class Q_DECL_HIDDEN HillClusterCsvFormatter::Private
{
public:
    Private(NonUniformFeatureFindingSession *ses)
        : session(ses)
    {
        // init output channel: QStringList for now
        outputHillItem = HILL_CLUSTER_ENTRY_HEADER;
        clearHillItem();
    }

    NonUniformFeatureFindingSession *session = nullptr;
    QStringList outputHillItem;

public:
    void clearHillItem()
    {
        for (int i = 0; i < outputHillItem.size(); ++i) {
            outputHillItem[i] = QString();
        }
    }
};

HillClusterCsvFormatter::HillClusterCsvFormatter(NonUniformFeatureFindingSession *session,
                                                 QObject *parent)
    : QObject(parent)
    , d(new Private(session))
{
    Q_ASSERT(session);
}

HillClusterCsvFormatter::~HillClusterCsvFormatter()
{
}

void HillClusterCsvFormatter::acceptFeature(const NonUniformFeature &feature)
{
    QVector<NonUniformHillCluster> chargeClusters = feature.chargeClusters();

    for (int i = 0; i < chargeClusters.size(); ++i) {
        const NonUniformHillCluster &cluster = chargeClusters.at(i);
        const QVector<NonUniformHill> hills = cluster.hills();
        for (int j = 0; j < hills.size(); ++j) {
            formatHill(feature.id(), cluster, j, &d->outputHillItem);
            emit rowFormatted(d->outputHillItem);
        }
    }
}

void HillClusterCsvFormatter::formatHill(int featureId, const NonUniformHillCluster &hillCluster,
                                         int hillIndex, QStringList *item)
{
    Q_ASSERT(item);

    const ScanIndexNumberConverter &converter = d->session->converter();

    const QVector<NonUniformHill> hills = hillCluster.hills();
    const NonUniformHill &hill = hills.at(hillIndex);
    const MzScanIndexRect &area = hill.area();

    const NonUniformTileRange &range = d->session->device()->range();

    (*item)[HillClusterEntryColumns::MZ_START] = QString::number(area.mz.start(), 'g', 12);
    (*item)[HillClusterEntryColumns::MZ_END] = QString::number(area.mz.end(), 'g', 12);

    (*item)[HillClusterEntryColumns::SCANINDEX_START] = QString::number(area.scanIndex.start());
    (*item)[HillClusterEntryColumns::SCANINDEX_END] = QString::number(area.scanIndex.end());

    double timeStart = converter.scanIndexToScanTime(area.scanIndex.start());

    // the interval is [scanIndexStart, scanIndexEnd)
    int scanBefore = std::max(range.scanIndexMin(), area.scanIndex.end() - 1);
    double timeEndIncluded = converter.scanIndexToScanTime(scanBefore);

    double timeEnd = 0.0;
    if (area.scanIndex.end() > range.scanIndexMax()) {
        // add half minute
        const double timeMarginMinutes = 0.5;
        timeEnd = timeEndIncluded + timeMarginMinutes;
    } else {
        double timeEndExcluded = converter.scanIndexToScanTime(area.scanIndex.end());
        timeEnd = (timeEndExcluded + timeEndIncluded) / 2.0;
    }

    double scanTime = converter.scanIndexToScanTime(hillCluster.scanIndex());
    int scanNumber = converter.timeToScanNumber(scanTime);
    (*item)[HillClusterEntryColumns::DEBUG_TEXT] = QString("%1 %2").arg(QString::number(scanTime), QString::number(scanNumber));
    (*item)[HillClusterEntryColumns::TIME_START] = QString::number(timeStart);
    (*item)[HillClusterEntryColumns::TIME_END] = QString::number(timeEnd);
    (*item)[HillClusterEntryColumns::LABEL] = QString::number(hill.id());
    (*item)[HillClusterEntryColumns::INTENSITY] = QString::number(hillCluster.maximumIntensity());
    (*item)[HillClusterEntryColumns::POINT_COUNT] = QString::number(hill.points().size());
    (*item)[HillClusterEntryColumns::FEATURE_ID] = QString::number(featureId);
    (*item)[HillClusterEntryColumns::GROUP_ID] = QString::number(hillCluster.id());
    (*item)[HillClusterEntryColumns::CHARGE] = QString::number(hillCluster.charge());
    (*item)[HillClusterEntryColumns::MONOISOTOPE] = QString::number(hillCluster.monoisotopicMz(), 'g', 12);
    (*item)[HillClusterEntryColumns::GROUP_SIZE] = QString::number(hillCluster.hills().size());

    // stroke for root feature
    const QColor parentHillColor = QColor(Qt::magenta);
    const QColor childHillColor = QColor(Qt::white);
    
    if (hillIndex == 0) {
        (*item)[HillClusterEntryColumns::STROKE] = parentHillColor.name(QColor::HexRgb);
    } else {
        (*item)[HillClusterEntryColumns::STROKE] = childHillColor.name(QColor::HexRgb);
    }

    // convert id (0, N) to color, i.e. index to Qt::GlobalColor 
    // in range [2,18] to exclude GlobalColor color0, color1 and transparent
    int colorIndex = (hillCluster.id() % 17) + 2;
    QColor fill = QColor(static_cast<Qt::GlobalColor>(colorIndex));

    (*item)[HillClusterEntryColumns::FILL] = fill.name(QColor::HexRgb);

    (*item)[HillClusterEntryColumns::COSINE_SIMILARITY] = QString::number(hill.correlation());
}

_PMI_END
