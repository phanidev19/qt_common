/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef HILL_CLUSTER_CSV_FORMATTER_H
#define HILL_CLUSTER_CSV_FORMATTER_H

#include "pmi_common_ms_export.h"

#include <pmi_core_defs.h>

#include <QStringList>
#include <QObject>

_PMI_BEGIN

class NonUniformFeature;
class NonUniformHillCluster;
class NonUniformFeatureFindingSession;

enum HillClusterEntryColumns {
    MZ_START = 0,
    MZ_END,
    TIME_START,
    TIME_END,
    LABEL,
    INTENSITY,
    TILEX,
    TILEY,
    SCANINDEX_START,
    SCANINDEX_END,
    POINT_COUNT,
    DEBUG_TEXT,
    FEATURE_ID,
    GROUP_ID,
    GROUP_SIZE,
    STROKE,
    FILL,
    CHARGE,
    MONOISOTOPE,
    COSINE_SIMILARITY,
    COLUMNS_COUNT // reserved, keep as last item
};

// sync this with indexes above ^^
const QStringList HILL_CLUSTER_ENTRY_HEADER
    = QStringList{ "mzStart",   "mzEnd",     "timeStart", "timeEnd",
                   "label", // requested columns
                   "intensity", "tileX",     "tileY",     "scanIndexStart", "scanIndexEnd",
                   "points",    "debugText", "featureId", "group_id",       "group_size",
                   "stroke",    "fill",      "charge",    "monoisotope",    "cosine_similarity" };

/*!
 * \brief Provides formatting of the feature from Feature finder to CSV. Maps feature to detailed
 * hills format that can be visualized in PMi-MSViz
 *
 *
  */
class PMI_COMMON_MS_EXPORT HillClusterCsvFormatter : public QObject
{
    Q_OBJECT
public:
    explicit HillClusterCsvFormatter(NonUniformFeatureFindingSession *session,
                                     QObject *parent = nullptr);
    ~HillClusterCsvFormatter();

    static QStringList header() { return HILL_CLUSTER_ENTRY_HEADER; }

    void acceptFeature(const NonUniformFeature &feature);

    void formatHill(int featureId, const NonUniformHillCluster &hillCluster, int hillIndex,
                    QStringList *outputHillItem);

signals:
    void rowFormatted(const QStringList &csvRow);

private:
    Q_DISABLE_COPY(HillClusterCsvFormatter)
    class Private;
    const QScopedPointer<Private> d;
};

_PMI_END

#endif // HILL_CLUSTER_CSV_FORMATTER_H