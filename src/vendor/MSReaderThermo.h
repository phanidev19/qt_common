#ifndef MSREADERTHERMO_H
#define MSREADERTHERMO_H

#include "CommonVendor.h"
#include "MSReaderBase.h"
#include "vendor_api/Thermo/ThermoAPITools.h"
#include "vendor_api/Thermo/ThermoMSFileReaderUtils.h"

_PMI_BEGIN
_MSREADER_BEGIN

class PMI_COMMON_MS_TEST_EXPORT MSReaderThermo : public MSReaderBase
{
public:
    MSReaderThermo();
    ~MSReaderThermo();

    // Find [disable_copy_constructor_ms_reader]
    Q_DISABLE_COPY(MSReaderThermo);

    MSReaderClassType classTypeName() const override;

    bool canOpen(const QString &filename) const override;
    Err openFile(const QString &filename,
                 MSConvertOption convert_options = ConvertWithCentroidButNoPeaksBlobs,
                 QSharedPointer<ProgressBarInterface> progress = NoProgress) override;
    Err closeFile();

    Err getBasePeak(point2dList *points) const override;
    Err getTICData(point2dList *points) const override;
    Err getScanData(long scanNumber, point2dList *points, bool do_centroiding = false,
                    PointListAsByteArrays *pointListAsByteArrays = NULL) override;
    Err getXICData(const XICWindow &win, point2dList *points, int ms_level = 1) const;
    Err getScanInfo(long scanNumber, ScanInfo *obj) const override;
    Err getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const override;

    Err getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const override;
    Err getFragmentType(long scanNumber, long scanLevel, QString *fragType) const override;

    /*!
     * \brief This function will eventually generalize getTIC and getUV.  For now, this replace
     * getUVData() but not getTICData() \param chroList \return Err
     */
    Err getChromatograms(QList<ChromatogramInfo> *chroList,
                         const QString &channelInternalName = QString()) override;

    using MSReaderBase::getBestScanNumber;
    Err getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const override;

private:
    thermo::InstrumentModelType m_modelType;
    std::string m_instModel;

protected:
    void clear();
    // TODO: move functions to MSReaderThermoRAW, once stable API
    RawFileHandle m_rawHandle;
};

_MSREADER_END
_PMI_END

#endif // MSREADERTHERMO_H
