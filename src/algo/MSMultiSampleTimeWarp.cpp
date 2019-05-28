/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
*/

#include "MSMultiSampleTimeWarp.h"

#include <algorithm>

#include <MSReader.h>
#include <ProgressContext.h>

_PMI_BEGIN

MSMultiSampleTimeWarp::MSMultiSampleTimeWarp()
    : m_centralPlot(-1)
    , m_samplesPerMinute(2.0)
{
    
}

MSMultiSampleTimeWarp::~MSMultiSampleTimeWarp()
{
}

Err MSMultiSampleTimeWarp::constructTimeWarp(const QVector<QString> &msFilenames,
                                             QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;

    // Create dummy indices
    QVector<MSID> machineFileIDs(msFilenames.size());
    std::iota(machineFileIDs.begin(), machineFileIDs.end(), 0);

    e = constructTimeWarp(msFilenames, machineFileIDs, progress); ree;

    return e;
}

Err MSMultiSampleTimeWarp::constructTimeWarp(const QVector<QString> &msFilenames,
                                             const QVector<MSID> &msFileIDs,
                                             QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;

    m_centralPlot = -1;
    m_msFilenames.clear();
    m_basePeakPlots.clear();
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
    m_timeWarps.resize(m_basePeakPlots.size());
    for (int i = 0; i < static_cast<int>(m_basePeakPlots.size()); i++) {
        if (i == m_centralPlot) {
            continue;
        }
        e = produceTimeWarp(i, &m_timeWarps[i]); ree;
        ++progressContext;
    }

    return e;
}

Err MSMultiSampleTimeWarp::getMSID(const QString &filename, MSID *outputMSID) const
{
    Err e = kNoErr;

    *outputMSID = 0;

    if (!m_filenameToMSID.contains(filename)) {
        return kBadParameterError;
    }

    *outputMSID = m_filenameToMSID[filename];

    return e;
}

Err MSMultiSampleTimeWarp::getMSFilename(const MSID &msid, QString *outputFilename) const
{
    Err e = kNoErr;

    *outputFilename = "";

    int index = 0;
    e = fileIndex(msid, &index); ree;

    *outputFilename = m_msFilenames[index];

    return e;
}

Err MSMultiSampleTimeWarp::getBasePeakPlot(const MSID &machineFileID, PlotBase *outputPlot) const
{
    Err e = kNoErr;

    int index = 0;
    e = fileIndex(machineFileID, &index); ree;

    *outputPlot = m_basePeakPlots[index];

    return e;
}

Err MSMultiSampleTimeWarp::getBasePeakPlot(const QString &msFilename, PlotBase *outputPlot) const
{
    Err e = kNoErr;

    MSID msid = 0;
    e = getMSID(msFilename, &msid); ree;
    e = getBasePeakPlot(msid, outputPlot); ree;

    return e;
}

Err MSMultiSampleTimeWarp::getTimeWarp(const MSID &machineFileID, TimeWarp *outputTimeWarp) const
{
    Err e = kNoErr;

    int index = 0;
    e = fileIndex(machineFileID, &index); ree;

    *outputTimeWarp = m_timeWarps[index];

    return e;
}

Err MSMultiSampleTimeWarp::getTimeWarp(const QString &msFilename, TimeWarp *outputTimeWarp) const
{
    Err e = kNoErr;

    MSID msid = 0;
    e = getMSID(msFilename, &msid); ree;
    e = getTimeWarp(msid, outputTimeWarp); ree;

    return e;
}

Err MSMultiSampleTimeWarp::warp(const MSID &machineFileID, double t, double *tWarped) const
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

Err MSMultiSampleTimeWarp::warp(const QString &msFilename, double t, double *tWarped) const
{
    Err e = kNoErr;

    MSID msid = 0;
    e = getMSID(msFilename, &msid); ree;
    e = warp(msid, t, tWarped); ree;

    return e;
}

Err MSMultiSampleTimeWarp::loadAlignableDataFromSample(const QString &filename, const MSID &machineFileID)
{
    Err e = kNoErr;

    // see if we already have the file cached
    if (m_fileIndex.contains(machineFileID)) {
        return e;
    }

    MSReader *msReader = MSReader::Instance();
    e = msReader->openFile(filename); ree;

    PlotBase plot;
    // e = msReader->getTICData(TICPlot.getPointList()); ree;
    e = msReader->getBasePeak(&plot.getPointList()); ree;

    // update indexes
    m_fileIndex.insert(machineFileID, static_cast<int>(m_basePeakPlots.size()));
    m_basePeakPlots.push_back(plot);
    m_msFilenames.push_back(filename);

    Q_ASSERT(m_basePeakPlots.size() == m_msFilenames.size());

    // Maintain reverse lookup
    m_filenameToMSID.insert(filename, machineFileID);

    return e;
}

Err MSMultiSampleTimeWarp::resample()
{
    Err e = kNoErr;

    if (m_basePeakPlots.size() <= 0) {
        return kError;
    }

    // get min/max range
    bool first = true;
    point2d globalEnd;
    for (const PlotBase &plot : m_basePeakPlots) {
        const point2dList &points = plot.getPointList();
        if (points.size() <= 0) {
            return kError;
        }

        // get last point
        point2d lastPoint = points.back();

        // Find the common range
        if (first || lastPoint.x() < globalEnd.x()) {
            globalEnd = lastPoint;
            first = false;
        }
    }

    // resample to approximate uniformly sampled plots
    const double duration = globalEnd.x();
    m_uniformPlots.clear();
    m_uniformPlots.resize(m_basePeakPlots.size());

    // resample
    for (int i = 0; i < static_cast<int>(m_basePeakPlots.size()); i++) {
        const PlotBase &original = m_basePeakPlots[i];
        QVector<double> &proxy = m_uniformPlots[i];

        auto compareY = [](const point2d &a, const point2d &b) -> bool {
            return a.y() < b.y();
        };
        double maxY
            = std::max_element(original.getPointList().begin(), original.getPointList().end(), compareY)->y();
        double normalizer = 1.0;
        if (maxY > 0.0) {
            normalizer = 1.0 / maxY;
        }

        for (double x = 0.0; x <= duration; x += samplePeriod()) {
            proxy.push_back(original.evaluate(x) * normalizer);
        }
    }

    return e;
}

Err MSMultiSampleTimeWarp::decideCentralPlot(int *centralPlot) const
{
    Err e = kNoErr;

    // calculate total dot similarity of each plot with all the other plots
    std::vector<double> dot(m_uniformPlots.size(), 0.0);
    for (int i = 0; i < static_cast<int>(m_uniformPlots.size()); i++) {
        const QVector<double> &a = m_uniformPlots[i];
        for (int j = 0; j < static_cast<int>(m_uniformPlots.size()); j++) {
            if (i == j) {
                continue;
            }
            const QVector<double> &b = m_uniformPlots[j];
            dot[i] += std::inner_product(a.begin(), a.end(), b.begin(), 0.0);
        }
    }

    *centralPlot
        = static_cast<int>(std::distance(dot.begin(), std::max_element(dot.begin(), dot.end())));

    return e;
}

Err MSMultiSampleTimeWarp::produceTimeWarp(int targetPlotIndex, TimeWarp *outputWarp) const
{
    Err e = kNoErr;

    if (m_centralPlot < 0 || m_centralPlot >= static_cast<int>(m_basePeakPlots.size())
        || targetPlotIndex < 0 || targetPlotIndex >= static_cast<int>(m_basePeakPlots.size())) {
        rrr(kBadParameterError);
    }

    if (m_basePeakPlots.size() != m_uniformPlots.size()) {
        rrr(kError);
    }

    if (targetPlotIndex == m_centralPlot) {
        // no self-project
        return e;
    }

    // plot B will be projected to plot A
    point2dList plotA = m_basePeakPlots[m_centralPlot].getPointList();
    point2dList plotB = m_basePeakPlots[targetPlotIndex].getPointList();

    // crop them to the minimum duration
    double duration = std::min(plotA.back().x(), plotB.back().x());
    auto cropCondition = [duration](const point2d &p) { return p.x() >= duration; };
    plotA.erase(std::remove_if(plotA.begin(), plotA.end(), cropCondition), plotA.end());
    plotB.erase(std::remove_if(plotB.begin(), plotB.end(), cropCondition), plotB.end());

    // normalize
    auto compareY = [](const point2d &a, const point2d &b) -> bool { return a.y() < b.y(); };
    double maxA = std::max_element(plotA.begin(), plotA.end(), compareY)->y();
    double normalizerA = 1.0;
    if (maxA > 0.0) {
        normalizerA = 1.0 / maxA;
    }
    for (point2d &p : plotA) {
        p.setY(p.y() * normalizerA);
    }
    double maxB = std::max_element(plotB.begin(), plotB.end(), compareY)->y();
    double normalizerB = 1.0;
    if (maxB > 0.0) {
        normalizerB = 1.0 / maxB;
    }
    for (point2d &p : plotB) {
        p.setY(p.y() * normalizerB);
    }

    // warp
    TimeWarp::TimeWarpOptions options;
    outputWarp->clear();
    e = outputWarp->constructWarp(plotA, plotB, options); ree;

    return e;
}

double MSMultiSampleTimeWarp::samplePeriod() const
{
    double ret = 1.0;
    if (m_samplesPerMinute != 0.0) {
        ret /= m_samplesPerMinute;
    }

    return ret;
}

Err MSMultiSampleTimeWarp::fileIndex(const MSID &msid, int *outputFileIndex) const
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

    if (index >= static_cast<int>(m_basePeakPlots.size())) {
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
