/*
* Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
*/

#ifndef CROSS_SAMPLE_FEATURE_COLLATOR_TURBO_H
#define CROSS_SAMPLE_FEATURE_COLLATOR_TURBO_H

#include "TimeWarp.h"

#include <common_constants.h>
#include <common_errors.h>
#include "CommonFunctions.h"
#include "FeatureFinderParameters.h"
#include "MSMultiSampleTimeWarp2D.h"
#include "ProgressBarInterface.h"
#include "pmi_common_ms_export.h"
#include <pmi_core_defs.h>
#include <TimeWarp2D.h>


#include <QDir>
#include <QSqlDatabase>

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <Eigen/Core>

#include <string>
#include <vector>

_PMI_BEGIN

struct SampleFeaturesTurbo {
    QString sampleFilePath;
    QString featuresFilePath;
};

struct CrossSampleFeatureTurbo {
	double rt = -1;
	int feature = -1;
	double rtWarped = -1;
	double xicStart = -1;
	double xicEnd = -1;
	double xicStartWarped = -1;
	double xicEndWarped = -1;
	double mwMonoisotopic = -1;
	double corrMax = -1;
	double maxIntensity = -1;
	int ionCount = -1;
	int maxIsotopeCount = -1;
	int masterFeature = -1;
	int sampleId = -1;
	QString chargeOrder;
	QString fileName;

    void clear() {
        rt = -1;
        feature = -1;
        rtWarped = -1;
        xicStart = -1;
        xicEnd = -1;
        xicStartWarped = -1;
        xicEndWarped = -1;
        mwMonoisotopic = -1;
        corrMax = -1;
        maxIntensity = -1;
        ionCount = -1;
        maxIsotopeCount = -1;
        masterFeature = -1;
        sampleId = -1;
        chargeOrder.clear();
        fileName.clear();
    }
};

class PMI_COMMON_MS_EXPORT CrossSampleFeatureCollatorTurbo
{
    
public:

    const int NOT_SET = -1;

    CrossSampleFeatureCollatorTurbo(const QVector<SampleFeaturesTurbo> &sampleFeatures,
        const SettableFeatureFinderParameters &ffUserParams);

    CrossSampleFeatureCollatorTurbo(const QVector<SampleFeaturesTurbo> &sampleFeatures,
        const QDir &projectDir,
        const SettableFeatureFinderParameters &ffUserParams);

    ~CrossSampleFeatureCollatorTurbo();

    void setPpmTolerance(double ppmTolerance);
    double ppmTolerance() const;
    QString masterFeaturesDatabasePath() const;
    QVector<CrossSampleFeatureTurbo> consolidatedCrossSampleFeatures() const;
   
    Err init();
    Err run(QSharedPointer<ProgressBarInterface> progress = NoProgress);
    Err testCollation(const QVector<CrossSampleFeatureTurbo> &consolidatedCrossSampleFeatures,
                      bool removeDuplicates = false);

private:
    Err consolidateFeaturesAcrossSamplesForCollation();
    Err crossSampleFeatureGrouping();  
    Err createCrossSampleFeatureCollatedSqliteDbSchema(QSqlDatabase &db, const QString &comparitronFilePath);
    Err transferMasterFeaturesToDatabase();
    Err consolidateDuplicateSampleIdsFoundInMasterFeature();


private:
    QVector<CrossSampleFeatureTurbo> m_consolidatedCrossSampleFeatures;
    Eigen::SparseMatrix<int, Eigen::ColMajor> m_sparseIndexMatrix;


    const QVector<SampleFeaturesTurbo> m_sampleFeatures;
    double m_ppmTolerance;
    int m_sparseRows;
    int m_sparseCols;
    QString m_masterFeaturesDBPath;
    MSMultiSampleTimeWarp2D m_warpCores;
    QVector<QString> m_msFiles;
    QVector<QString> m_msFilePaths;
    QVector<QString> m_featureFiles;
    QVector<MSMultiSampleTimeWarp2D::MSID> m_msids;
    ImmutableFeatureFinderParameters m_ffParams;
    SettableFeatureFinderParameters m_ffUserParams;
};

_PMI_END

#endif // CROSS_SAMPLE_FEATURE_COLLATOR_TURBO_H