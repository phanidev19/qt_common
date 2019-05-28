/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
 */

#include "MSMultiSampleTimeWarp2D.h"

#include <algorithm>

#include <MSReader.h>
#include <ProgressContext.h>

_PMI_BEGIN

// Default settings
static const double MZ_MATCH_PPM_DEFAULT(100.0);
static const double MZ_WINDOW_TO_EXTRACT_MAXIMA_DEFAULT(20.0);
static const int MZ_INTENSITY_PAIR_COUNT_DEFAULT(0); // 0 = legacy warp (intensity only)

MSMultiSampleTimeWarp2D::MSMultiSampleTimeWarp2D()
    : m_centralPlot(-1)
    , m_samplesPerMinute(2.0)
    , m_mzMatchPpm(MZ_MATCH_PPM_DEFAULT)
    , m_mzWindowToExtractMaxima(MZ_WINDOW_TO_EXTRACT_MAXIMA_DEFAULT)
    , m_mzIntensityPairCount(MZ_INTENSITY_PAIR_COUNT_DEFAULT)
{
}

MSMultiSampleTimeWarp2D::~MSMultiSampleTimeWarp2D()
{
}

Err MSMultiSampleTimeWarp2D::constructTimeWarp(const QVector<QString> &msFilenames,
                                               QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;

    // Create dummy indices
    QVector<MSID> machineFileIDs(msFilenames.size());
    std::iota(machineFileIDs.begin(), machineFileIDs.end(), 0);

    e = constructTimeWarp(msFilenames, machineFileIDs, progress); ree;

    return e;
}

Err MSMultiSampleTimeWarp2D::constructTimeWarp(const QVector<QString> &msFilenames,
                                               const QVector<MSID> &msFileIDs,
                                               QSharedPointer<ProgressBarInterface> progress)
{

    Err e = kNoErr;

    if (msFilenames.size() <= 0) {
        // Nothing to warp
        return kNoErr;
    }

    m_centralPlot = -1;
    m_msFilenames.clear();
    m_timedWarpElementLists.clear();
    m_fileIndex.clear();
    m_filenameToMSID.clear();
    m_timeWarps.clear();

    // filename list must correspond to file ID list.
    if (msFilenames.size() != msFileIDs.size()) {
        rrr(kBadParameterError);
    }

    if (msFilenames.isEmpty()) {
        return e;
    }


    // Progress bar support
    int taskCount = static_cast<int>(msFilenames.size()); // for "loadFile" n times
    taskCount += 1; // for resample
    taskCount += 1; // for decideCentralPlot
    taskCount += static_cast<int>(msFilenames.size()) - 1; // for produceTimeWarp n-1 times
    ProgressContext progressContext(taskCount, progress);
    progressContext.setText("Constructing time warp");


    // read all plots
    for (int i = 0; i < static_cast<int>(msFilenames.size()); i++) {
        e = loadAlignableDataFromSample(msFilenames[i], msFileIDs[i]); ree;
        ++progressContext;
    }

    // resample to uniformly sampled plots and cache them
    e = resample(); ree;
    ++progressContext;

    // decide which plot should be used for time warping
    e = decideCentralPlot(&m_centralPlot); ree;
    ++progressContext;

    // Create time warps for all samples except m_centralPlot.
    m_timeWarps.resize(m_timedWarpElementLists.size());
    for (int i = 0; i < static_cast<int>(m_timedWarpElementLists.size()); i++) {
        if (i == m_centralPlot) {
            continue;
        }
        e = produceTimeWarp(i, &m_timeWarps[i]); ree;
        ++progressContext;
    }

    return e;
}


Err MSMultiSampleTimeWarp2D::getMSID(const QString &filename, MSID *outputMSID) const
{
    Err e = kNoErr;

    *outputMSID = 0;

    if (!m_filenameToMSID.contains(filename)) {
        return kBadParameterError;
    }

    *outputMSID = m_filenameToMSID[filename];

    return e;
}

Err MSMultiSampleTimeWarp2D::getMSFilename(const MSID &msid, QString *outputFilename) const
{
    Err e = kNoErr;

    *outputFilename = "";

    int index = 0;
    e = fileIndex(msid, &index); ree;

    *outputFilename = m_msFilenames[index];

    return e;
}

Err MSMultiSampleTimeWarp2D::getTimedWarpElementList(const MSID &machineFileID, TimedWarpElementList *outputPlot) const
{
    Err e = kNoErr;

    int index = 0;
    e = fileIndex(machineFileID, &index); ree;

    *outputPlot = m_timedWarpElementLists[index];

    return e;
}

Err MSMultiSampleTimeWarp2D::getTimedWarpElementList(const QString &msFilename, TimedWarpElementList *outputPlot) const
{
    Err e = kNoErr;

    MSID msid = 0;
    e = getMSID(msFilename, &msid); ree;
    e = getTimedWarpElementList(msid, outputPlot); ree;

    return e;
}

Err MSMultiSampleTimeWarp2D::getTimeWarp(const MSID &machineFileID, TimeWarp2D *outputTimeWarp) const
{
    Err e = kNoErr;

    int index = 0;
    e = fileIndex(machineFileID, &index); ree;

    *outputTimeWarp = m_timeWarps[index];

    return e;
}

Err MSMultiSampleTimeWarp2D::getTimeWarp(const QString &msFilename, TimeWarp2D *outputTimeWarp) const
{
    Err e = kNoErr;

    MSID msid = 0;
    e = getMSID(msFilename, &msid); ree;
    e = getTimeWarp(msid, outputTimeWarp); ree;

    return e;
}

Err MSMultiSampleTimeWarp2D::warp(const MSID &machineFileID, double t, double *tWarped) const
{
    Err e = kNoErr;

    *tWarped = t;

    int index = 0;
    e = fileIndex(machineFileID, &index); ree;

    if (index == m_centralPlot) {
        return kNoErr;
    }

    // do warp
    *tWarped = m_timeWarps[index].warp(t);

    return e;
}

Err MSMultiSampleTimeWarp2D::warp(const QString &msFilename, double t, double *tWarped) const
{
    Err e = kNoErr;

    MSID msid = 0;
    e = getMSID(msFilename, &msid); ree;
    e = warp(msid, t, tWarped); ree;

    return e;
}

QString MSMultiSampleTimeWarp2D::pivotFilename() const
{
    return m_msFilenames[m_centralPlot];
}

Err MSMultiSampleTimeWarp2D::loadAlignableDataFromSample(const QString &filename, const MSID &machineFileID)
{
    Err e = kNoErr;

    // see if we already have the file cached
    if (m_fileIndex.contains(machineFileID)) {
        return e;
    }

    MSReader *msReader = MSReader::Instance();
    e = msReader->openFile(filename); ree;

    // get all MS1 scans
    QList<msreader::ScanInfoWrapper> scanInfos;
    e = msReader->getScanInfoListAtLevel(1, &scanInfos); ree;

    const bool doCentroiding = true;

    // produce warp element list (list of dominant mz/intensity pairs)
    TimedWarpElementList timedWarpElementList;
    for (const msreader::ScanInfoWrapper &scanInfo : scanInfos) {
        // get scan
        point2dList scanData;
        e = msReader->getScanData(scanInfo.scanNumber, &scanData, doCentroiding); ree;

        TimedWarpElement timedWarpElement;
        timedWarpElement.timeInMinutes = scanInfo.scanInfo.retTimeMinutes;
        e = WarpElement::fromScan(scanData, m_mzIntensityPairCount, m_mzWindowToExtractMaxima,
                                  &timedWarpElement.warpElement); ree;

        timedWarpElementList.push_back(timedWarpElement);
    }

    // update indexes
    m_fileIndex.insert(machineFileID, static_cast<int>(m_timedWarpElementLists.size()));
    m_timedWarpElementLists.push_back(timedWarpElementList);
    m_msFilenames.push_back(filename);

    Q_ASSERT(m_timedWarpElementLists.size() == m_msFilenames.size());

    // Maintain reverse lookup
    m_filenameToMSID.insert(filename, machineFileID);

    msReader->closeFile();
    msReader->closeAllFileConnections();

    return e;
}

Err MSMultiSampleTimeWarp2D::resample()
{
    Err e = kNoErr;

    if (m_timedWarpElementLists.size() <= 0) {
        return kError;
    }

    // get min/max range
    bool first = true;
    TimedWarpElement globalEnd;
    for (const TimedWarpElementList &plot : m_timedWarpElementLists) {
        if (plot.size() <= 0) {
            return kError;
        }

        // get last point
        TimedWarpElement lastPoint = plot.back();

        // Find the common range
        if (first || lastPoint.timeInMinutes < globalEnd.timeInMinutes) {
            globalEnd = lastPoint;
            first = false;
        }
    }

    // resample to approximate uniformly sampled plots
    const double duration = globalEnd.timeInMinutes;
    m_uniformPlots.clear();
    m_uniformPlots.resize(m_timedWarpElementLists.size());

    // resample
    for (int i = 0; i < static_cast<int>(m_timedWarpElementLists.size()); i++) {
        const TimedWarpElementList &original = m_timedWarpElementLists[i];
        WarpElementList &proxy = m_uniformPlots[i];

        double maxIntensity
            = std::max_element(original.begin(), original.end(), TimedWarpElement::compareIntensity)
                  ->warpElement.intensity();
        double normalizer = 1.0;
        if (maxIntensity > 0.0) {
            normalizer = 1.0 / maxIntensity;
        }

        for (double x = 0.0; x <= duration; x += samplePeriod()) {
            proxy.push_back(original.evaluate(m_mzMatchPpm, x).scale(normalizer));
        }
    }

    return e;
}

Err MSMultiSampleTimeWarp2D::decideCentralPlot(int *centralPlot) const
{
    Err e = kNoErr;

    // calculate total dot similarity of each plot with all the other plots
    // TODO: Try to minimize WarpElement penalty instead
    std::vector<double> dot(m_uniformPlots.size(), 0.0);
    
    for (int i = 0; i < static_cast<int>(m_uniformPlots.size()); i++) {
        const WarpElementList &a = m_uniformPlots[i];
        for (int j = 0; j < static_cast<int>(m_uniformPlots.size()); j++) {
            if (i == j) {
                continue;
            }
            const WarpElementList &b = m_uniformPlots[j];
            dot[i] = 0.0;
            for (int j = 0; j < static_cast<int>(a.size()) && j < static_cast<int>(b.size()); j++) {
                dot[i] += a[j].intensity() * b[j].intensity();
            }
        }
    }

    *centralPlot
        = static_cast<int>(std::distance(dot.begin(), std::max_element(dot.begin(), dot.end())));

    return e;
}

Err MSMultiSampleTimeWarp2D::produceTimeWarp(int targetPlotIndex, TimeWarp2D *outputWarp) const
{
    Err e = kNoErr;

    if (m_centralPlot < 0 || m_centralPlot >= static_cast<int>(m_timedWarpElementLists.size())
        || targetPlotIndex < 0
        || targetPlotIndex >= static_cast<int>(m_timedWarpElementLists.size())) {
        rrr(kBadParameterError);
    }

    if (m_timedWarpElementLists.size() != m_uniformPlots.size()) {
        rrr(kError);
    }

    if (targetPlotIndex == m_centralPlot) {
        // no self-project
        return e;
    }

    // plot B will be projected to plot A
    TimedWarpElementList plotA = m_timedWarpElementLists[m_centralPlot];
    TimedWarpElementList plotB = m_timedWarpElementLists[targetPlotIndex];

    // crop them to the minimum duration
    double duration = std::min(plotA.back().timeInMinutes, plotB.back().timeInMinutes);
    auto cropCondition
        = [duration](const TimedWarpElement &p) { return p.timeInMinutes >= duration; };
    plotA.erase(std::remove_if(plotA.begin(), plotA.end(), cropCondition), plotA.end());
    plotB.erase(std::remove_if(plotB.begin(), plotB.end(), cropCondition), plotB.end());

    // normalize
    double maxA = std::max_element(plotA.begin(), plotA.end(), TimedWarpElement::compareIntensity)
                      ->warpElement.intensity();
    double normalizerA = 1.0;
    if (maxA > 0.0) {
        normalizerA = 1.0 / maxA;
    }
    for (TimedWarpElement &p : plotA) {
        p.warpElement.scale(normalizerA);
    }
    double maxB = std::max_element(plotB.begin(), plotB.end(), TimedWarpElement::compareIntensity)
                      ->warpElement.intensity();
    double normalizerB = 1.0;
    if (maxB > 0.0) {
        normalizerB = 1.0 / maxB;
    }
    for (TimedWarpElement &p : plotB) {
        p.warpElement.scale(normalizerB);
    }

    // warp
    TimeWarp2D::TimeWarpOptions options;
    options.mzMatchPpm = m_mzMatchPpm;
    outputWarp->clear();
    e = outputWarp->constructWarp(plotA, plotB, options); ree;

    return e;
}

double MSMultiSampleTimeWarp2D::samplePeriod() const
{
    double ret = 1.0;
    if (m_samplesPerMinute != 0.0) {
        ret /= m_samplesPerMinute;
    }

    return ret;
}

Err MSMultiSampleTimeWarp2D::fileIndex(const MSID &msid, int *outputFileIndex) const
{
    Err e = kNoErr;

    *outputFileIndex = 0;

    if (!m_fileIndex.contains(msid)) {
        return kBadParameterError;
    }

    int index = m_fileIndex[msid];

    if (index < 0) {
        return kError;
    }

    if (index >= static_cast<int>(m_timedWarpElementLists.size())) {
        return kError;
    }

    if (index >= static_cast<int>(m_timeWarps.size())) {
        return kError;
    }

    if (index >= static_cast<int>(m_msFilenames.size())) {
        return kError;
    }

    *outputFileIndex = index;

    return e;
}

_PMI_END
