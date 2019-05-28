/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#ifndef COLLATE_CHARGE_CLUSTERS_TO_FEATURES_H
#define COLLATE_CHARGE_CLUSTERS_TO_FEATURES_H

#include "CrossSampleFeatureCollatorTurbo.h"
#include "FeatureFinderParameters.h"
#include "pmi_common_core_mini_export.h"


#include <common_errors.h>
#include <pmi_core_defs.h>

#include <QSqlDatabase>

_PMI_BEGIN

struct MWChargeClusterPoint {
    int ID = -1;
    int scanIndex = -1;
    int vendorScanNumber = -1;
    double rt = -1.0;
    double mzFound = -1.0;
    double maxIntensity = -1.0;
    double mwMonoisotopic = -1.0;
    int monoOffset = -1;
    double corr = -1.0;
    int charge = -1;
    int feature = -1;
    int isotopeCount = -1;
};

class PMI_COMMON_MS_EXPORT CollateChargeClustersToFeatures
{

public:
    CollateChargeClustersToFeatures(const QSqlDatabase &db,
                                    const SettableFeatureFinderParameters &ffUserParams);

    CollateChargeClustersToFeatures();

    ~CollateChargeClustersToFeatures();

    Err init();
    Err initTestingPurposes(const std::vector<MWChargeClusterPoint> & chargeClusters,
        const SettableFeatureFinderParameters &ffUserParams, 
        QVector<CrossSampleFeatureTurbo> *crossSampleFeautreTurbos);

private:
    Err loadChargeClustersFromDB();
    Err collateFeatures();
    Err clusterChargeClustersByFeature();
    Err clusterChargeClustersByFeatureTestingPurposes(QVector<CrossSampleFeatureTurbo> *crossSampleFeatureTurbosReturn);
    // TODO: create structure FeatureInfo for this one
    Err saveFeatureToDb(double xicStart, double xicEnd, double rt, int feature,
                        double mwMonoisotopic, double corrMax, double maxIntensity, int ionCount,
                        QString chargeOrder, int isotopeCount) const;

private:
    QSqlDatabase m_db;
    std::vector<MWChargeClusterPoint> m_chargeClusters;
    ImmutableFeatureFinderParameters m_ffParams;
    SettableFeatureFinderParameters m_ffUserParams;
};

_PMI_END

#endif // COLLATE_CHARGE_CLUSTERS_TO_FEATURES_H
