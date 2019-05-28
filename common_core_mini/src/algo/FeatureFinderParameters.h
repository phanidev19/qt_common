/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */
#ifndef FEATURE_FINDER_PARAMETERS_H
#define FEATURE_FINDER_PARAMETERS_H

struct ImmutableFeatureFinderParameters {
    const double vectorGranularity = 1.0 / 0.002; // unit: bucket/mz
    const double errorRange = 0.02; // Dalton (mass)
    const double augmentFactor = 1.3;
    const int mzMax = 3100;
    const int apexChargeClustering = 4; // unit mz
    const int maxChargeState = 10;
    const int combFilterTeethChargeMax = 4;
    const int mzMatchIndex = static_cast<int>(apexChargeClustering * vectorGranularity);
    const int vectorArrayLength = (mzMatchIndex * 2) + 1;
    const int apexChargeClusteringHashed = static_cast<int>(apexChargeClustering * vectorArrayLength);
    const int errorRangeHashed = static_cast<int>(errorRange * vectorGranularity);
    const double epsilonDBSCAN = 5.01; // 0.01 is added to the fractional end to include whole integer.
    const int maxIonCount = 1000;
    const double maxTimeToleranceCoarse = 2;
    const double maxTimeToleranceWarped = 0.08;
    const double isotopeCutOffClusterPercent = 0.05;
    const int dbscanMultiple = 50;
};

struct SettableFeatureFinderParameters {
    double averagineCorrelationCutOff = 0.75;
    double minFeatureMass = 500; 
    double maxFeatureMass = 8000;
    double minPeakWidthMinutes = 0.0;
    int minScanCount = 3;
    int minIsotopeCount = 3;
    int noiseFactorMultiplier = 3; 
    int ppm = 15;
    bool enableMS2Matching = false;
};


#endif // FEATURE_FINDER_PARAMETERS_H
