#ifndef MSREADERBRUKER_H
#define MSREADERBRUKER_H

#include "MSReaderBase.h"
#include "CommonVendor.h"

_PMI_BEGIN
_MSREADER_BEGIN

class PMI_COMMON_MS_TEST_EXPORT MSReaderBruker : public MSReaderBase
{
public:
    MSReaderBruker();
    virtual ~MSReaderBruker();

    // Find [disable_copy_constructor_ms_reader]
    Q_DISABLE_COPY(MSReaderBruker);

    MSReaderClassType classTypeName() const override;

    bool canOpen(const QString &filename) const override;
    Err openFile(const QString &filename,
                 MSConvertOption convert_options = ConvertWithCentroidButNoPeaksBlobs,
                 QSharedPointer<ProgressBarInterface> progress = NoProgress) override;
    Err closeFile() override;

    Err getBasePeak(point2dList *points) const override;
    Err getTICData(point2dList *points) const override;
    Err getScanData(long scanNumber, point2dList *points, bool do_centroiding = false,
                    PointListAsByteArrays *pointListAsByteArrays = NULL) override;
    Err getXICData(const XICWindow &win, point2dList *points, int ms_level = 1) const override;
    Err getScanInfo(long scanNumber, ScanInfo *obj) const override;
    Err getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const override;

    Err getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const override;
    Err getFragmentType(long scanNumber, long scanLevel, QString *fragType) const override;

    /*!
     * \brief This function will eventually generalize getTIC and getUV.
     *        For now, this replace getUVData() but not getTICData()
     * \param chroList
     * \return Err
     */
    Err getChromatograms(QList<ChromatogramInfo> *chroList,
                         const QString &channelInternalName = QString()) override;

    using MSReaderBase::getBestScanNumber;
    Err getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const override;

private:
    Err extractChromatogramData(long traceId, point2dList *points) const;

    static bool hasLsData(const QString& filename);

private:
    // This contains COM related functions; we keep this as private class to avoid exposing those to rest of the code.
    class PrivateData;
    PrivateData *d;
};

_MSREADER_END
_PMI_END

#endif // MSREADERBRUKER_H
