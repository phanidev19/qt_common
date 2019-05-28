/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Lukas Tvrdy ltvrdy@milosolutions.com
 */

#ifndef MULTI_SAMPLE_SCAN_FEATURE_FINDER_H
#define MULTI_SAMPLE_SCAN_FEATURE_FINDER_H

#include "pmi_common_ms_export.h"
#include "CrossSampleFeatureCollatorTurbo.h"
#include "FeatureFinderParameters.h"

// core_mini
#include <PQMFeatureMatcher.h>

#include "ProgressBarInterface.h"

#include <common_errors.h>
#include <pmi_core_defs.h>

#include <QDir>
#include <QString>
#include <QVector>

_PMI_BEGIN

struct SampleSearch {
    QString sampleFilePath;
    QString byonicFilePath;
};

//! @brief Scan feature finder for multiple samples
class PMI_COMMON_MS_EXPORT MultiSampleScanFeatureFinder
{

public:
    MultiSampleScanFeatureFinder(const QVector<SampleSearch> &msFiles,
                                 const QString &neuralNetworkDbFilePath, const SettableFeatureFinderParameters &ffUserParams);
    ~MultiSampleScanFeatureFinder();

    Err init();

    /*!
     * @brief Triggers finding of the features and it's workflow with matching IDs and collating
     * accross samples
     *
     * Note: call @see init() before executing the feature finding to ensure all pre-requisites are
     * met, other this call will fail
     */
    Err findFeatures(QSharedPointer<ProgressBarInterface> progress = NoProgress);

    void setTemporaryDatabasesEnabled(bool on);
    bool temporaryDatabasesEnabled() const;

    QString errorMessage() const;

    bool isValid() const;

    void setWorkingDirectory(const QDir &directory);
    QDir workingDirectory() const;

    QString insilicoPeptideCsvFile() const;

    /*!
    * @brief Settings used by algorithm that decides matching of the features to MSMS ids
    */
    FeatureMatcherSettings matchingSettings() const;

    /*!
    * @brief Allows you to modify settings for matching MSMS ids. 
    */
    void setMatchingSettings(const FeatureMatcherSettings &settings);

private:
    void setErrorMessage(const QString &msg);

    QString findFeaturesDbFilePath(const QString &sampleFilePath, const QVector<SampleFeaturesTurbo> &msFeaturesDbPaths);

    Err matchMS2WithByonic(const QVector<SampleFeaturesTurbo> &msFeaturesDbPaths,
                           QSharedPointer<ProgressBarInterface> progress = NoProgress);

private:
    QVector<SampleSearch> m_inputFilePaths;
    QString m_neuralNetworkDbFilePath;
    bool m_temporaryResultsEnabled = false;
    bool m_isValid = false;
    QString m_errorMessage;
    QDir m_workingDirectory;
    bool m_isWorkingDirectorySet;
    QString m_insilicoPeptideCsv;

    FeatureMatcherSettings m_matchingSettings;
    SettableFeatureFinderParameters m_ffUserParams;

};

_PMI_END

#endif // MULTI_SAMPLE_SCAN_FEATURE_FINDER_H
