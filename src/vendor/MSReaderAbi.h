/*
 * Copyright (C) 2015-2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Yong Joo Kil <ykil@proteinmetrics.com>
 * Convert from COM to C++ calls: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef MSREADERABI_H
#define MSREADERABI_H

#include "CommonVendor.h"
#include "MSReaderBase.h"
#include "common_errors.h"

class MSReaderAbiTest;

_PMI_BEGIN
_MSREADER_BEGIN

#ifdef Q_OS_WIN
class PMI_COMMON_MS_TEST_EXPORT MSReaderAbi : public MSReaderBase
{
public:
    MSReaderAbi();
    virtual ~MSReaderAbi();

    // Note(Yong)[disable_copy_constructor_ms_reader]:  Implementing these are a bit of a pain
    // because we need to manage the COM pointers; I don't have enough knowledge as well.  So
    // completely remove these features (using c++11 feature of delete)  And we want to prevent these
    // so that the destructor doesn't delete the private pointer multiple time.
    Q_DISABLE_COPY(MSReaderAbi);

    MSReaderClassType classTypeName() const override;

    bool canOpen(const QString &filename) const override;
    Err openFile(const QString &filename,
                 MSConvertOption convertOptions = ConvertWithCentroidButNoPeaksBlobs,
                 QSharedPointer<ProgressBarInterface> progress = NoProgress) override;
    Err closeFile() override;

    Err getBasePeak(point2dList *points) const override;
    Err getTICData(point2dList *points) const override;
    Err getScanData(long scanNumber, point2dList *points, bool doCentroiding = false,
                    PointListAsByteArrays *pointListAsByteArrays = NULL) override;
    Err getXICData(const XICWindow &win, point2dList *points,
                   int ms_level = 1) const override;
    Err getScanInfo(long scanNumber, ScanInfo *obj) const override;
    Err getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const override;

    Err getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const override;
    Err getFragmentType(long scanNumber, long scanLevel,
                        QString *fragType) const override;

    /*!
     * \brief This function will eventually generalize getTIC and getUV.  For now, this replace
     * getUVData() but not getTICData() \param chroList \return Err
     */
    Err getChromatograms(QList<ChromatogramInfo> *chroList,
                         const QString &internalChannelName = QString()) override;

    using MSReaderBase::getBestScanNumber;
    Err getBestScanNumber(int msLevel, double scanTimeMinutes,
                          long *scanNumber) const override;

protected:
    Err setupSampleList();
    Err getSampleName(int sampleNumber, QString *sampleName);

private:
    /*!
    * \brief ID parser/generator for ABI chromatograms
    */
    class PMI_COMMON_MS_TEST_EXPORT ChromatogramCanonicalId
    {
    public:
        QString chroType;
        QString sampleNo;
        QString adcSampleNo;

        ChromatogramCanonicalId();
        ChromatogramCanonicalId(const QString &chroType, const QString &sampleNo,
                                const QString &adcSampleNo = QString());
        static ChromatogramCanonicalId fromInternalChannelName(const QString &internalChannelName);
        QString toString() const;
        bool isEmpty() const;
        bool operator==(const ChromatogramCanonicalId &other) const;
    };
    friend class MSReaderAbiTest;

private:
    class PrivateData;
    PrivateData *d;
};

#else
class MSReaderAbi : public MSReaderBase
{
public:
    MSReaderAbi() {}
    ~MSReaderAbi() {}

    virtual MSReaderClassType classTypeName() const { return MSReaderClassTypeABSCIEX; }

    virtual bool canOpen(const QString &filename) const { return false; }
    virtual Err openFile(const QString &filename,
                         MSConvertOption convert_options = ConvertWithCentroidButNoPeaksBlobs,
                         QSharedPointer<ProgressBarInterface> progress = nullptr)
    {
        return kError;
    }
    virtual Err closeFile() { return kError; }

    virtual Err getTICData(point2dList &points) const { return kError; }
    virtual Err getScanData(long scanNumber, point2dList &points, bool do_centroiding = false,
                            PointListAsByteArrays *pointListAsByteArrays = NULL)
    {
        return kError;
    }
    virtual Err getXICData(const XICWindow &win, point2dList &points, int ms_level = 1) const
    {
        return kError;
    }
    virtual Err getScanInfo(long scanNumber, ScanInfo &obj) const { return kError; }
    virtual Err getScanPrecursorInfo(long scanNumber, PrecursorInfo &pinfo) const { return kError; }

    virtual Err getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const
    {
        return kError;
    }
    virtual Err getFragmentType(long scanNumber, long scanLevel, QString &fragType) const
    {
        return kError;
    }

    /*!
     * \brief This function will eventually generalize getTIC and getUV.  For now, this replace
     * getUVData() but not getTICData() \param chroList \return Err
     */
    virtual Err getChromatograms(QList<ChromatogramInfo> &chroList,
                                 const QString &channelInternalName = QString())
    {
        return kError;
    }

    Err getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const
    {
        return kError;
    }
};

#endif //Q_OS_WIN

_MSREADER_END
_PMI_END

#endif // MSREADERABI_H
