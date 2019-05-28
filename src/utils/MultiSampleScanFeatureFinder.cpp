/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "MultiSampleScanFeatureFinder.h"
#include "pmi_common_ms_debug.h"

#include "InsilicoGenerator.h"
#include <PmiMemoryInfo.h>
#include "PQMFeatureMatcher.h"
#include "ScanIterator.h"

#include <ProgressBarInterface.h>
#include <ProgressContext.h>

#include <QFileInfo>

#include <algorithm>

_PMI_BEGIN

MultiSampleScanFeatureFinder::MultiSampleScanFeatureFinder(const QVector<SampleSearch> &msFiles,
                                                           const QString &neuralNetworkDbFilePath, const SettableFeatureFinderParameters &ffUserParams)
    : m_inputFilePaths(msFiles)
    , m_neuralNetworkDbFilePath(neuralNetworkDbFilePath)
    , m_isWorkingDirectorySet(false)
    , m_ffUserParams(ffUserParams)
{
}

MultiSampleScanFeatureFinder::~MultiSampleScanFeatureFinder()
{
}

Err MultiSampleScanFeatureFinder::init()
{
    Err e = kNoErr;

    if (!QFileInfo(m_neuralNetworkDbFilePath).isReadable()) {
        setErrorMessage(QString(QObject::tr("%1 is not readable")).arg(m_neuralNetworkDbFilePath));
        rrr(kBadParameterError);
    }

    // samples
    for (const SampleSearch &item : qAsConst(m_inputFilePaths)) {
        if (!QFileInfo(item.sampleFilePath).isReadable()) {
            setErrorMessage(QString(QObject::tr("%1 is not readable.").arg(item.sampleFilePath)));
            rrr(kBadParameterError);
        }
    }

    // byrslt files
    if (m_ffUserParams.enableMS2Matching) {
        for (const SampleSearch &item : qAsConst(m_inputFilePaths)) {
            if (!QFileInfo(item.byonicFilePath).isReadable()) {
                setErrorMessage(QString(QObject::tr("%1 is not readable")).arg(item.byonicFilePath));
                rrr(kBadParameterError);
            }
        }
    }

    m_isValid = (e == kNoErr);
    return e;
}

Err MultiSampleScanFeatureFinder::findFeatures(QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;

    if (!isValid()) {
        warningMs() << "Feature finding is not properly initialized!";
        rrr(kError);
    }

    const int workflowStepsCount = 3;

    ProgressContext overallProgressContext (workflowStepsCount, progress);

    if (progress) {
        progress->setText(QObject::tr("Finding features..."));
    }

    ScanIterator iterator(m_neuralNetworkDbFilePath, m_ffUserParams);
    if (m_isWorkingDirectorySet) {
        iterator.setWorkingDirectory(m_workingDirectory);
    } else {
        debugMs() << "Working directory is not set, temporary files will be written next to vendor "
                     "file paths";
    }
    
    e = iterator.init();
    if (e != kNoErr) {
        // this is workflow, so we will properly format the message on error
        setErrorMessage(QObject::tr("Failed to initialize the feature finder"));
        rrr(e);
    }

    // this can be eventually done in threads, not sure about MSReader being used in multi-threaded
    // environment
    QVector<SampleFeaturesTurbo> msFeaturesDbPaths;

    {
        ProgressContext progressContext(m_inputFilePaths.size(), progress);
        for (const SampleSearch &item : qAsConst(m_inputFilePaths)) {
            const QString sampleFilePath = item.sampleFilePath;
            QString featuresCacheDbFilePath;
            e = iterator.iterateMSFileLinearDBSCANSelect(sampleFilePath, &featuresCacheDbFilePath, progress);
            if (e != kNoErr) {
                setErrorMessage(QObject::tr("Finding features failed for %1").arg(sampleFilePath));
                rrr(e);
            }

            SampleFeaturesTurbo featureItem;
            featureItem.featuresFilePath = featuresCacheDbFilePath;
            featureItem.sampleFilePath = sampleFilePath;
            msFeaturesDbPaths.push_back(featureItem);
            ++progressContext;
        }
    }

    if (progress) {
        progress->setText(QObject::tr("Matching MS2 ..."));
    }
    ++overallProgressContext;

    if (m_ffUserParams.enableMS2Matching) {
        e = matchMS2WithByonic(msFeaturesDbPaths, progress); ree;
    }
    

    // cross sample matching
    if (progress) {
        progress->setText(QObject::tr("Cross sample matching..."));
    }
    ++overallProgressContext;

    CrossSampleFeatureCollatorTurbo crossSampleCollator = m_isWorkingDirectorySet
        ? CrossSampleFeatureCollatorTurbo(msFeaturesDbPaths, m_workingDirectory, m_ffUserParams)
        : CrossSampleFeatureCollatorTurbo(msFeaturesDbPaths, m_ffUserParams);
    qDebug() << "Process peak memory before collation init" << pmi::MemoryInfo::peakProcessMemory();
    e = crossSampleCollator.init();
    if (e != kNoErr) {
        setErrorMessage(QObject::tr("Failed to initialize cross sampling!"));
        rrr(e);
    }
    e = crossSampleCollator.run(progress);
    if (e != kNoErr) {
        setErrorMessage(QObject::tr("Cross sampling execution failed!"));
        rrr(e);
    }
    qDebug() << "Process peak memory after Collation" << pmi::MemoryInfo::peakProcessMemory();
    InsilicoGenerator generator(crossSampleCollator.masterFeaturesDatabasePath());
    generator.setMS2MatchingEnabled(m_ffUserParams.enableMS2Matching);

    QString csvFilePath;
    e = generator.generateInsilicoFile(&csvFilePath);
    
    if (e != kNoErr) { 
        setErrorMessage("Failed to generate Insilico CSV file");
        rrr(e);
    }

    m_insilicoPeptideCsv = csvFilePath;

    ++overallProgressContext;

    return e;
}

Err MultiSampleScanFeatureFinder::matchMS2WithByonic(
    const QVector<SampleFeaturesTurbo> &msFeaturesDbPaths, QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;

    ProgressContext progressContext(m_inputFilePaths.size(), progress);
    for (const SampleSearch &item : qAsConst(m_inputFilePaths)) {
        if (progress) {
            progress->setText(QObject::tr("Matching MS2 in %1...").arg(item.sampleFilePath));
        }
        ++progressContext;

        // translate item.sampleFilePath to 
        QString featuresFilePath = findFeaturesDbFilePath(item.sampleFilePath, msFeaturesDbPaths);
        if (featuresFilePath.isEmpty()) {
            warningMs() << "Cannot find features db for " << item.sampleFilePath;
            rrr(kError);
        }

        if (item.byonicFilePath.isEmpty()) {
            warningMs() << item.sampleFilePath << "does not have associated byonic search file";
            continue;
        }

        PQMFeatureMatcher matcher(featuresFilePath, item.byonicFilePath, m_ffUserParams);
        matcher.setSettings(m_matchingSettings);
        Err e = matcher.init();
        if (e != kNoErr) {
            setErrorMessage(QObject::tr("Failed to match ids at %2")
                                .arg(item.sampleFilePath, item.byonicFilePath));
            rrr(e);
        }
    }

    return e;
}

void MultiSampleScanFeatureFinder::setTemporaryDatabasesEnabled(bool on)
{
    m_temporaryResultsEnabled = on;
}

bool MultiSampleScanFeatureFinder::temporaryDatabasesEnabled() const
{
    return m_temporaryResultsEnabled;
}

QString MultiSampleScanFeatureFinder::errorMessage() const
{
    return m_errorMessage;
}

bool MultiSampleScanFeatureFinder::isValid() const
{
    return m_isValid;
}

void MultiSampleScanFeatureFinder::setWorkingDirectory(const QDir &directory)
{
    m_isWorkingDirectorySet = true;
    m_workingDirectory = directory;
}

QDir MultiSampleScanFeatureFinder::workingDirectory() const
{
    return m_workingDirectory;
}

QString MultiSampleScanFeatureFinder::insilicoPeptideCsvFile() const
{
    return m_insilicoPeptideCsv;
}

FeatureMatcherSettings MultiSampleScanFeatureFinder::matchingSettings() const
{
    return m_matchingSettings;
}

void MultiSampleScanFeatureFinder::setMatchingSettings(const FeatureMatcherSettings &settings)
{
    m_matchingSettings = settings;
}

void MultiSampleScanFeatureFinder::setErrorMessage(const QString &msg)
{
    m_errorMessage = msg;
}

QString MultiSampleScanFeatureFinder::findFeaturesDbFilePath(
    const QString &sampleFilePath, const QVector<SampleFeaturesTurbo> &msFeaturesDbPaths)
{
    QString result;

    auto iter = std::find_if(msFeaturesDbPaths.begin(), msFeaturesDbPaths.end(),
                             [sampleFilePath](const SampleFeaturesTurbo &features) {
                                 return features.sampleFilePath == sampleFilePath;
                             });

    if (iter != msFeaturesDbPaths.end()) {
        result = iter->featuresFilePath;
    }

    return result;
}

_PMI_END
