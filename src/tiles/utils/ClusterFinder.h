/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef CLUSTER_FINDER_H
#define CLUSTER_FINDER_H

#include "pmi_common_ms_export.h"

#include "FinderFeaturesDao.h"
#include "ProgressBarInterface.h"

// common_stable
#include <common_errors.h>
#include <pmi_core_defs.h>

#include <QScopedPointer>
#include <QtGlobal>

class QSqlDatabase;

_PMI_BEGIN

/*
 * @brief Computes the GroupNumber for features. Group number is common for features with similar
 * attributes.
 *
 * Similar attributes are uncharged mass, apex time and intensity. Tolerance for similarity is set
 * through @see setClusteringSettings. Clustering is executed through @see run() function
 */
class PMI_COMMON_MS_EXPORT ClusterFinder
{

public:
    /*
     * @brief creates cluster finder for appriopriate database
     *
     * @param db - points to database containing tables with features and samples
     *        @see FinderFeaturesDao, FinderSamplesDao for details
     */
    explicit ClusterFinder(QSqlDatabase *db);
    ~ClusterFinder();

    /*
     * @brief Executes the clustering algorithm and updates GroupNumber for features
     *
     * @param progress
     */
    Err run(QSharedPointer<ProgressBarInterface> progress = NoProgress);

    void setClusteringSettings(const NeighborSettings &settings);
    NeighborSettings clusteringSettings() const;

    /*
    * @brief Exports to insilico peptide CSV format recognized by Byologic
    */
    Err exportToInsilicoPeptideCsv(const QString &csvFilePath) const;

private:
    /*
     * @brief Finds best candidates for given samplesId
     *
     * Best is the one closes to the @a parent, closest by uncharged mass, apex time and intensity
     */
    FinderFeaturesDaoEntry bestCandidate(int samplesId, const FinderFeaturesDaoEntry &parent,
                                         const QVector<FinderFeaturesDaoEntry> &neighbors);

private:
    Q_DISABLE_COPY(ClusterFinder)
    class Private;
    const QScopedPointer<Private> d;
};

_PMI_END

#endif // CLUSTER_FINDER_H
