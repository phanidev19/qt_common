/* 
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
*/

#ifndef __MS_MULTI_SAMPLE_TIME_WARP_H__
#define __MS_MULTI_SAMPLE_TIME_WARP_H__

#include <QVector>
#include <QString>

#include <pmi_common_ms_export.h>
#include <common_errors.h>

#include <PlotBase.h>
#include <ProgressBarInterface.h>
#include <TimeWarp.h>

_PMI_BEGIN

/*!
    \brief This class loads and caches BasePeak plots from all loaded samples and produces one
        time warp for each sample. Using the time warp we can transform time values from sample space to
        a common-alignment space.

    Simple usage:
        * Prepare a list of machine filenames that are to be aligned:
            QVector<QString> msFilenames;
            msFilenames.push_back("file_a");
            msFilenames.push_back("file_b");
            ...
        * Instantiate this class and call the constructWarp function:
            MSMultiSampleTimeWarp w;
            w.constructTimeWarp(msFilenames);
        * Use warp function to transform time values from "sample" space to "reference" space
            double interestingTimeValueInSampleA = ...;
            double interestingTimeValueMappedToCommonSpace;
            w.warp("file_a", interestingTimeValueInSampleA, &interestingTimeValueMappedToCommonSpace);

    Note that this usage pattern is not optimal. If you need to transform many points, please use the
    \a getTimeWarp function and transform the points using the returned time warp.

    For a full API demonstration, check out MSMultiSampleTimeWarpTest auto-test.

*/
class PMI_COMMON_MS_EXPORT MSMultiSampleTimeWarp
{
public:

    /*! \brief Unique ID of MS file. */
    typedef int MSID;

    MSMultiSampleTimeWarp();
    virtual ~MSMultiSampleTimeWarp();

    /*! \brief Extract MS content (BPI) from files and then use to create time warps. */
    Err constructTimeWarp(const QVector<QString> &msFilenames,
                          QSharedPointer<ProgressBarInterface> progress = NoProgress);

    /*!
        \brief Extract MS content (BPI) from files and then use to create time warps.
        \param [in] msFilenames List of machine filenames. Must be in sync with msFileIDs.
        \param [in] msFileIDs List of file IDs. The IDs will be used to reference each
            sample in subsequent calls to this class.
    */
    Err constructTimeWarp(const QVector<QString> &msFilenames, const QVector<MSID> &msFileIDs,
                          QSharedPointer<ProgressBarInterface> progress = NoProgress);

    /*! \brief Get MSID associated with this machine filename. */
    Err getMSID(const QString &msFilename, MSID *outputMSID) const;

    /*! \brief Get MS filename associated with this MSID. */
    Err getMSFilename(const MSID &msid, QString *outputFilename) const;

    /*! \brief Get cached BasePeak plot corresponding to the file ID given. */
    Err getBasePeakPlot(const MSID &machineFileID, PlotBase *outputTICPlot) const;

    /*! \brief Get cached BasePeak plot corresponding to the filename given. */
    Err getBasePeakPlot(const QString &msFilename, PlotBase *outputTICPlot) const;

    /*!
    \brief Get the time warp for the given file ID. The time warp can be used to transform points
        from sample space to common-alignment space
    */
    Err getTimeWarp(const MSID &machineFileID, TimeWarp *outputTimeWarp) const;

    /*!
    \brief Get the time warp for the given file. The time warp can be used to transform points
        from sample space to common-alignment space
    */
    Err getTimeWarp(const QString &msFilename, TimeWarp *outputTimeWarp) const;

    /*!
    \brief Warp time to common space.
        Note: This is not efficient. Use getTimeWarp to transform multiple points.
    \param machineFileID MSID of the MS file from which 't' came from
    \param t Time to be warped into the common warp space
    \param [out] tWarped Warped t. If msFilename is the central ms file (to which all
    others warp), this output will receive a copy of \a t.
    */
    Err warp(const MSID &machineFileID, double t, double *tWarped) const;

    /*!
    \brief Warp time to common space.
        Note: This is not efficient. Use getTimeWarp to transform multiple points.
    \param msFilename Name of the MS file from which 't' came from
    \param t Time to be warped into the common warp space
    \param [out] tWarped Warped t. If msFilename is the central ms file (to which all
    others warp), t is used unchanged.
    */
    Err warp(const QString &msFilename, double t, double *tWarped) const;

private:
    Err loadAlignableDataFromSample(const QString &filename, const MSID &machineFileID);
    Err resample();
    Err decideCentralPlot(int *centralTIC) const;
    Err produceTimeWarp(int targetPlotIndex, TimeWarp *outputWarp) const;
    double samplePeriod() const;

    /*! \brief Maps MSID to file index. Returned index will be bounds-checked against all internal data. */
    Err fileIndex(const MSID &msid, int *outputFileIndex) const;

private:
    QVector<QString> m_msFilenames;
    QVector<PlotBase> m_basePeakPlots;
    QVector<TimeWarp> m_timeWarps;
    QMap<MSID, int> m_fileIndex;
    QMap<QString, int> m_filenameToMSID;
    int m_centralPlot;
    const double m_samplesPerMinute; // for resampled plots
    QVector<QVector<double> > m_uniformPlots;
};

_PMI_END

#endif // __MS_MULTI_SAMPLE_TIME_WARP_H__
