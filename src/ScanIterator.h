/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
*/

#ifndef SCAN_ITERATOR_H
#define SCAN_ITERATOR_H

#include "ChargeDeterminatorNN.h"
#include "CollateChargeClustersToFeatures.h"

#include "FauxScanCreator.h"
#include "FauxSpectraReader.h"

#include "FeatureFinderParameters.h"
#include "MonoisotopeDeterminatorNN.h"

#include <common_errors.h>
#include <common_math_types.h>
#include "pmi_common_ms_export.h"
#include <pmi_core_defs.h>


#include "ProgressBarInterface.h"

#include <QDir>
#include <QString>

#include <vector>

class QSqlDatabase;

_PMI_BEGIN

struct ScanDetails {
    int scanIndex = 0;
    int vendorScanNumber = 0;
    double rt = 0.0;
};

struct ChargeCluster {
    double mzFound = 0;
    int maxIntensity = 0;
    double monoOffset = 0;
    int charge = 0;
    double corr = 0;
    int istopeCount = 0;
    double scanNoiseFloor = 0;
};


/*!
 * @brief This class iterates through every scan of an MS data file.  It will identify all features
 * in the scan.
 */
class PMI_COMMON_MS_EXPORT ScanIterator
{
public:
    ScanIterator(const QString &neuralNetworkDbFilePath, const SettableFeatureFinderParameters &ffUserParams);

    ~ScanIterator();

    Err iterateMSFileLinearDBSCANSelect(const QString &msFilePath, QString *outputPath,
                                        QSharedPointer<ProgressBarInterface> progress = NoProgress);

    Err iterateMSFileLinearDBSCANSelectTestPurposes(
        const QVector<FauxScanCreator::Scan> &scansVec,
        QVector<CrossSampleFeatureTurbo> *crossSampleFeautreTurbosReturn);

    Err init();

    void setWorkingDirectory(const QDir &workingDirectory);
    QDir workingDirectory() const;

private:
    Err saveChargeClusterIntoDb(QSqlDatabase &db, const ScanDetails &scanDetails,
                                 const std::vector<ChargeCluster> vectorChargeCluster);
    Err createFeatureFinderSqliteDbSchema(QSqlDatabase &db);
    Err createFeatureFinderDB(QSqlDatabase &db, const QString &msFilePath, QString *outputFilePath);
    int determineNoiseFloor(const point2dList &scanData);
    // TODO: This is duplicated in FindMzIterators.  Put in common functions
    static double calculateSD(const std::vector<int> &data);
    // TODO: This is duplicated in FindMzIterators.  Put in common functions
    double calculateMedian(std::vector<int> data);

private:
    QString m_neuralNetworkDbFilePath;
    QDir m_workingDirectory;
    bool m_isWorkingDirectorySet;
    ImmutableFeatureFinderParameters m_ffParams;
    SettableFeatureFinderParameters m_ffUserParams;
    ChargeDeterminatorNN m_chargeDeterminator;
    MonoisotopeDeterminatorNN m_monoDeterminator;
    std::vector<MWChargeClusterPoint> m_MWChargeClusterPoints;
};

_PMI_END
#endif // SCAN_ITERATOR_H
