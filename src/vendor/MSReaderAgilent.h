/*
 * Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Alex Prikhodko alex.prikhodko@proteinmetrics.com
 */

#ifndef MSREADERAGILENT_H
#define MSREADERAGILENT_H

#include "common_errors.h"
#include "CommonVendor.h"
#include "MSReaderBase.h"

#include <limits>

enum ChromType;
enum DeviceType;
struct ISignalInfo;
template<class T> class SafeArrayWrapper;

struct tagSAFEARRAY;
typedef tagSAFEARRAY SAFEARRAY;

namespace BDA {
struct IBDAChromData;
struct IBDAChromFilter;
}

_PMI_BEGIN
_MSREADER_BEGIN

struct MSReaderAgilentPrivate;

// Important! This is a highly incomplete implementation, however
// a better solution can quickly get quite complex, please see
// http://stackoverflow.com/questions/13940316/floating-point-comparison-revisited
template<class T>
bool almostEqual(T x, T y, T maxDiff)
{
    // Note the added parentheses around min()
    // (std::min)(a, b);
    // This is to prevent the conflicts with windows.h/windef.h min macro
    return std::abs(x - y) < maxDiff
        || std::abs(x - y) < (std::numeric_limits<T>::min)();
}

class PMI_COMMON_MS_TEST_EXPORT MSReaderAgilent : public MSReaderBase
{
public:
    MSReaderAgilent();
    virtual ~MSReaderAgilent();

    Q_DISABLE_COPY(MSReaderAgilent);

    bool canOpen(const QString &filename) const override;
    MSReaderClassType classTypeName() const override;
    Err openFile(const QString &filename,
                 MSConvertOption convert_options = ConvertWithCentroidButNoPeaksBlobs,
                 QSharedPointer<ProgressBarInterface> progress = NoProgress) override;

    Err closeFile() override;
    Err getBasePeak(point2dList *points) const override;
    Err getTICData(point2dList *points) const override;
    Err getScanData(long scanNumber, point2dList *points, bool do_centroiding = false,
                    PointListAsByteArrays *pointListAsByteArrays = NULL) override;
    Err getXICData(const XICWindow &xicWindow, point2dList *points, int msLevel = 1) const override;
    Err getScanInfo(long scanNumber, ScanInfo *obj) const override;
    Err getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const override;
    Err getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const override;
    Err getFragmentType(long scanNumber, long scanLevel, QString *fragType) const override;
    Err getChromatograms(QList<ChromatogramInfo> *chroList,
                         const QString &internalChannelName = QString()) override;

    using MSReaderBase::getBestScanNumber;
    Err getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const override;

    Err isCustomCentroidingPreferred(long scanNumber, bool *customCentroidingPreferred) const;
    static void removeZeroIntensities(const SafeArrayWrapper<double> &xvals,
                                      const SafeArrayWrapper<float> &yvals,
                                      pmi::point2dList *points);

    void dbgDumpToFile(const QString &path = QString());
    void dbgDumpAllScans(const QString &path = QString());
    void dbgTest();

protected:
    bool initialize();
    bool isInitialized() const;

private:
    // used by GetBasePeak and GetTICData
    Err getDataByChromatogram(ChromType type, point2dList *points) const;

private:
    struct MSReaderAgilentPrivate;
    const QScopedPointer<MSReaderAgilentPrivate> d;
};

_MSREADER_END
_PMI_END

#endif // MSREADERAGILENT_H
