/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com , Yong Kil ykil@proteinmetrics.com
*/
#ifndef _MSREADERSIMULATED_H_
#define _MSREADERSIMULATED_H_

#include "CentroidOptions.h"
#include "CompressionInfoHolder.h"
#include "FauxScanCreator.h"
#include "FauxSpectraReader.h"
#include "MSReaderBase.h"

#include <QList>
#include <QMap>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

_PMI_BEGIN

class CacheFileManagerInterface;
class ProgressBarInterface;

_MSREADER_BEGIN

/*!
 * \brief The MSReaderSimulated class will read *.msfaux format
 */
class PMI_COMMON_MS_EXPORT MSReaderSimulated : public MSReaderBase
{
public:
    struct MSReaderSimulatedOptions {
        int numberOfScans = 100;
        double timePerScan = 0.01;
    };

    MSReaderSimulated();

    ~MSReaderSimulated() override;

    virtual MSReaderClassType classTypeName() const override;

    bool canOpen(const QString &filename) const override;

    /*!
     * \brief openFile
     * \param filename
     * \param convert_options for doing minimal conversion with only chromatogram
     * \return
     */
    Err openFile(const QString &filename,
                 MSConvertOption convert_options = ConvertWithCentroidButNoPeaksBlobs,
                 QSharedPointer<ProgressBarInterface> progress = NoProgress) override;

    Err closeFile() override;

    Err getBasePeak(point2dList *points) const override;

    Err getTICData(point2dList *points) const override;

    /// Note: should be const, but the updating the byspec prevents us from doing that.
    Err getScanData(long scanNumber, point2dList *points, bool do_centroiding = false,
                    PointListAsByteArrays *pointListAsByteArrays = nullptr) override;

    Err getXICData(const XICWindow &win, point2dList *points, int ms_level = 1) const override;

    Err getScanInfo(long scanNumber, ScanInfo *obj) const override;

    Err getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const override;

    Err getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const override;

    Err getFragmentType(long scanNumber, long scanLevel, QString *fragType) const override;

    Err cacheScanNumbers(const QList<int> &scanNumberList, QSharedPointer<ProgressBarInterface> progress);

    /*!
     * \brief This function will eventually generalize getTIC and getUV.  For now, this replace
     * getUVData() but not getTICData() \param chroList \return Err
     */
    Err getChromatograms(QList<ChromatogramInfo> *chroList,
                         const QString &channelInternalName = QString()) override;

    QSqlDatabase *getDatabasePointer();

    using MSReaderBase::getBestScanNumber;
    Err getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const override;

    Err getLockmassScans(QList<msreader::ScanInfoWrapper> *list) const override;
    MSReaderBase::FileInfoList filesInfoList() const override;

private:
    MSReaderSimulatedOptions m_msReaderSimulatedOptions;
    void clear() override;

private:
    int m_scansPerSecond;
    QVector<FauxScanCreator::Scan> m_scans;
    QString m_fileName;
};

_MSREADER_END
_PMI_END

#endif // _MSREADERSIMULATED_H_
