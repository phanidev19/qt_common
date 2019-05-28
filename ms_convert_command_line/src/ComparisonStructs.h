/*
 * Copyright (C) 2017-2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Sam Bogar (sbogar@proteinmetrics.com)
 */
#ifndef COMPARISONSTRUCTS_H
#define COMPARISONSTRUCTS_H

#include "MSReader.h"

/*
** The values in DiffTolerances are used to determine when one scan's meta-data is 'significantly' different from another's
** These values are currently placeholders
** TO-DO:
**     Specify tolerances of greater informativeness
*/
struct DiffTolerances {
    double retTimeMinutesDiffTolerance = 1;
    int    nScanNumberDiffTolerance = 1;
    double dIsolationDiffTolerance = 1;
    double dMonoIsoMassDiffTolerance = 0.1;
    double nChargeStateDiffTolerance = 0.1;

    double normalizedYDiffTolerance = 0.1;
    double xProximityToBeConsideredSamePointProfile = 0.1;
};

class ComparisonStructs
{
public:
    ComparisonStructs() {}
    ~ComparisonStructs() {}

    struct MetaInfoForScanInOneFile {
        int scanNumber = -1;
        pmi::msreader::ScanInfo scanInfo;
        pmi::msreader::PrecursorInfo precursorInfo;
    };

    struct MetaInfoForScanInBothFiles {
        MetaInfoForScanInOneFile firstFileMetaInfo;
        MetaInfoForScanInOneFile secondFileMetaInfo;
        QString nativeId;
    };

    struct MetaInfoCompare {
        // scan info differences
        double retTimeMinutesDiff = 0;
        int scanLevelDiff = 0;

        // precursorScanInfo differences
        long nScanNumberDiff = 0;
        double dIsolationMassDiff = 0;
        double dMonoIsoMassDiff = 0;
        long nChargeStateDiff = 0;
    };

    struct UserInput {
        bool writeFull = false;
        bool isCentroided = false;
        bool compareByIndex = false;
        QString firstFile;
        QString secondFile;
        QString outputPath;
    };

    struct DiffInfoForOneScan {
        point2d maxDiff;
        point2d coordinateOfMaxDiff;
        QString nativeId;
    };

    struct DiffInfo {
        MetaInfoForScanInBothFiles scanWithGreatestDiff;
        point2d biggestDiffInFile;
        point2d coordinateOfBiggestDiffInFile;
        QSet<DiffInfoForOneScan> maxDiffs;
        QSet<DiffInfoForOneScan> maxNonZeroDiffs;

        QSet<MetaInfoForScanInBothFiles> significantDifferences;
    };
};
/*
** Author's note:
**     All of the following equivalence overloads are used for generating QSets of the structures.
**     !!DO NOT MODIFY!!
*/
inline bool operator==(const ComparisonStructs::DiffInfoForOneScan &lhs,
                       const ComparisonStructs::DiffInfoForOneScan &rhs)
{
    return lhs.nativeId == rhs.nativeId;
}

inline bool operator==(const ComparisonStructs::MetaInfoForScanInOneFile &lhs,
                       const ComparisonStructs::MetaInfoForScanInOneFile &rhs)
{
    return lhs.scanInfo.nativeId == rhs.scanInfo.nativeId;
}

inline bool operator==(const ComparisonStructs::MetaInfoForScanInBothFiles &lhs,
                       const ComparisonStructs::MetaInfoForScanInBothFiles &rhs)
{
    return lhs.nativeId == rhs.nativeId;
}

uint qHash(const ComparisonStructs::MetaInfoForScanInBothFiles &toHash);
uint qHash(const ComparisonStructs::DiffInfoForOneScan &toHash);
uint qHash(const ComparisonStructs::MetaInfoForScanInOneFile &toHash);

#endif // COMPARISONSTRUCTS_H 