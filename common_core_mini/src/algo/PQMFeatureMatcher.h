/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 * Author: Lukas Tvrdy ltvrdy@milosolutions.com
 */
#ifndef PQM_FEATURE_MATCHER_H
#define PQM_FEATURE_MATCHER_H

#include "FeatureFinderParameters.h"
#include "pmi_common_core_mini_export.h"

// common_stable
#include <common_errors.h>

#include <QString>

_PMI_BEGIN

struct PQMResultItem {
    double calcMH = -1.0;
    double rt = -1.0;
    double score = -1.0;
    double intensity = -1.0;
    int id = -1;
    QString peptide;
};

/*!
 * @brief This class represents settings that can be user-facing
 */
class PMI_COMMON_CORE_MINI_EXPORT FeatureMatcherSettings
{
public:
    /*!
     * @brief Constructs default settings
     */
    FeatureMatcherSettings();

    double score; //< minimal MS2 search score for MS MS Id matching, default value is 200.0
};

/*!
 * @brief This class matches PQM MSMS IDs from byrslt file to features in ftrs file.
 */
class PMI_COMMON_CORE_MINI_EXPORT PQMFeatureMatcher
{

public:
    explicit PQMFeatureMatcher(const QString &featuresFilePath, const QString &byrsltFilePath,
                               const SettableFeatureFinderParameters &ffUserParams);

    ~PQMFeatureMatcher();

    Err init();

    void setSettings(const FeatureMatcherSettings &settings);

    FeatureMatcherSettings settings() const;

private:
    Err startMatching() const;

    static double calculatePPMTolerance(double mw, int ppm); // TODO put this into common functions

private:
    QString m_featureDBMsFilePath;
    QString m_byrsltFilePath;
    FeatureMatcherSettings m_settings;
    SettableFeatureFinderParameters m_ffUserParams;
};

_PMI_END

#endif // PQM_FEATURE_MATCHER_H