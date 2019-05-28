/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Alex Prikhodko alex.prikhodko@proteinmetrics.com
 */

#include "MSReaderBrukerTims.h"
#include "PlotBase.h"
#include "VendorPathChecker.h"
#include "pmi_common_ms_debug.h"
#include <pmi_core_defs.h>

#include "timsdata.h"

#include <stdexcept>
#include <string>
#include <cstdint>
#include <numeric>
#include <vector>
#include <limits>
#include <queue>

#include "boost/throw_exception.hpp"
#include "boost/noncopyable.hpp"
#include "boost/shared_array.hpp"
#include "boost/range/iterator_range.hpp"
#include "boost/optional.hpp"

#include <QFileInfo>
#include "QtSqlUtils.h"
#include <comdef.h>

namespace timsdata
{

/// Proxy object to conveniently access the data read via tims_read_scans() of the C
/// API. (Copies of this object share the same underlying data buffer.)
class FrameProxy
{
public:
    FrameProxy()
        : num_scans(0)
    {

    }

    void resize(int numScans, int newSize) {
        num_scans = numScans;

        // only resize - no initialization
        pData.resize(newSize);

        // only resize - no initialization
        scan_offsets.resize(newSize + 1);
    }

    void recalculate() {
        scan_offsets[0] = 0;
        std::partial_sum(pData.data() + 0, pData.data() + num_scans, scan_offsets.begin() + 1);
    }

    uint32_t* data()
    {
        return pData.data();
    }

    size_t dataSize()
    {
        return pData.size();
    }

    /// Get number of scans represented by this object (if this was produced by
    /// TimsData::readScans(), it contains only the requested scan range).
    size_t getNbrScans() const
    {
        return num_scans;
    }

    size_t getTotalNbrPeaks() const
    {
        return scan_offsets.back();
    }

    size_t getNbrPeaks(size_t scan_num) const
    {
        throwIfInvalidScanNumber(scan_num);
        return pData[scan_num];
    }

    boost::iterator_range<const uint32_t *> getScanX(size_t scan_num) const
    {
        return makeRange(scan_num, 0);
    }

    boost::iterator_range<const uint32_t *> getScanY(size_t scan_num) const
    {
        return makeRange(scan_num, pData[scan_num]);
    }

private:
    size_t num_scans;
    std::vector<uint32_t> pData; //< data layout as described in tims_read_scans()
    std::vector<uint32_t> scan_offsets;

    void throwIfInvalidScanNumber(size_t scan_num) const
    {
        if (scan_num >= getNbrScans())
            BOOST_THROW_EXCEPTION(std::invalid_argument("Scan number out of range."));
    }

    boost::iterator_range<const uint32_t *> makeRange(size_t scan_num, size_t offset) const
    {
        throwIfInvalidScanNumber(scan_num);
        const uint32_t *p = pData.data() + num_scans + 2 * scan_offsets[scan_num] + offset;
        return boost::make_iterator_range(p, p + pData[scan_num]);
    }

};

/// \throws std::runtime_error containing the last timsdata.dll error string.
inline void throwLastError()
{
    uint32_t len = tims_get_last_error_string(0, 0);

    boost::shared_array<char> buf(new char[len]);
    tims_get_last_error_string(buf.get(), len);

    BOOST_THROW_EXCEPTION(std::runtime_error(buf.get()));
}

/// Reader for TIMS binary data (.tdf_bin). (The SQLite file (.tdf) containing all the
/// metadata may be opened separately using any desired SQLite API.)
class TimsData : public boost::noncopyable
{
public:
    /// Open specified TIMS analysis.
    ///
    /// \param[in] analysis_directory_name in UTF-8 encoding.
    /// \param[in] use_recalibration if DA re-calibration shall be used if available
    ///
    /// \throws std::exception in case of an error
    explicit TimsData(const std::string &analysis_directory_name, bool use_recalibration = false)
        : handle(0)
        , initial_frame_buffer_size(128)
    {
        if (analysis_directory_name.size() <= 0) {
            return;
        }
        handle = tims_open(analysis_directory_name.c_str(), use_recalibration);
        if (handle == 0)
            throwLastError();
    }

    /// Close TIMS analysis.
    ~TimsData()
    {
        tims_close(handle);

    }

    /// Get the C-API handle corresponding to this instance. (Caller does not get
    /// ownership of the handle.) (This call is here for the case that the user wants
    /// to call C-library functions directly.)
    uint64_t getHandle() const
    {
        return handle;
    }

    /// Read a range of scans from a single frame. Not thread-safe.
    ///
    /// \returns a proxy object that represents only the requested scan range of the
    /// specified frame (i.e., FrameProxy::getNbrScans() will return 'scan_end -
    /// scan_begin', and scan #0 in the proxy will correspond to 'scan_begin').
    void readScans(int64_t frame_id, //< frame index
                   uint32_t scan_begin, //< first scan number to read (inclusive)
                   uint32_t scan_end, //< last scan number (exclusive)
                   ::timsdata::FrameProxy* result)
    {
        if (scan_end < scan_begin) {
            BOOST_THROW_EXCEPTION(std::runtime_error("scan_end must be >= scan_begin"));
        }
        const uint32_t num_scans = scan_end - scan_begin;

        // buffer-growing loop
        for (;;) {
            const uint32_t required_len
                = tims_read_scans_v2(handle, frame_id, scan_begin, scan_end, (void *)result->data(),
                                     uint32_t(4 * result->dataSize()));
            if (required_len == 0) {
                throwLastError();
            }

            if (4 * result->dataSize() > required_len) {
                if (required_len < 4 * num_scans) {
                    BOOST_THROW_EXCEPTION(std::runtime_error("Data array too small."));
                }
                result->resize(num_scans, required_len / 4);
                result->recalculate();
                return;
            }

            // arbitrary limit for now...
            if (required_len > 16777216) {
                BOOST_THROW_EXCEPTION(std::runtime_error("Maximum expected frame size exceeded."));
            }

            result->resize(num_scans, required_len / 4 + 1); // grow buffer
        }
    }

#define BDAL_TIMS_DEFINE_CONVERSION_FUNCTION(CPPNAME, CNAME) \
    void CPPNAME ( \
    int64_t frame_id,               /**< frame index */ \
    const std::vector<double> & in, /**< vector of input values (can be empty) */ \
    std::vector<double> & out )     /**< vector of corresponding output values (will be resized automatically) */ \
    { \
    doTransformation(frame_id, in, out, CNAME); \
    }

    BDAL_TIMS_DEFINE_CONVERSION_FUNCTION(indexToMz, tims_index_to_mz)
    BDAL_TIMS_DEFINE_CONVERSION_FUNCTION(mzToIndex, tims_mz_to_index)
    BDAL_TIMS_DEFINE_CONVERSION_FUNCTION(scanNumToOneOverK0, tims_scannum_to_oneoverk0)
    BDAL_TIMS_DEFINE_CONVERSION_FUNCTION(oneOverK0ToScanNum, tims_oneoverk0_to_scannum)
    BDAL_TIMS_DEFINE_CONVERSION_FUNCTION(scanNumToVoltage, tims_scannum_to_voltage)
    BDAL_TIMS_DEFINE_CONVERSION_FUNCTION(voltageToScanNum, tims_voltage_to_scannum)

    private:
        uint64_t handle;
    size_t initial_frame_buffer_size; // number of uint32_t elements

    void doTransformation(
            int64_t frame_id,
            const std::vector<double> & in,
            std::vector<double> & out,
            BdalTimsConversionFunction * func)
    {
        if (in.empty()) {
            out.clear();
            return;
        }
        if (in.size() > (std::numeric_limits<uint32_t>::max)())
            BOOST_THROW_EXCEPTION(std::runtime_error("Input range too large."));
        out.resize(in.size());
        func(handle, frame_id, &in[0], &out[0], uint32_t(in.size()));
    }
};

} // namespace timsdata



_PMI_BEGIN

_MSREADER_BEGIN

static const double MILLIONTH = 1E-6;
static const double MinPpmThreshold = 0.02;
static const int desired_num_samples = 400000; // <============= force for now
static const int WRONG_ID = -1;

struct Triplet {
    double x;
    double y;
    int index;

    Triplet(double x1, double y1, int index1) {
        x = x1;
        y = y1;
        index = index1;
    }
};

class AscendingCompare
{
public:
    bool operator() (const Triplet& aa, const Triplet& bb) const
    {
        return (aa.x > bb.x);
    }
};

bool decreasingIntensity(const point2d &c1, const point2d &c2)
{
    return (c1.y() > c2.y());
}

bool increasingMz(const point2d &c1, const point2d &c2)
{
    return (c1.x() < c2.x());
}

static void getSpectraIndicies(const std::map<uint32_t, float>& map, std::vector<double>& result)
{
    result.reserve(map.size());
    for (const auto& it : map) {
        result.emplace_back(it.first);
    }
}

static void getSpectraValues(const std::map<uint32_t, float>& map, std::vector<float>& result)
{
    result.reserve(map.size());
    for (const auto& it : map) {
        result.emplace_back(it.second);
    }
}

struct MSReaderBrukerTims::Private final
{
public:
    Private(const QString& filename)
        : tdfDirectory(filename)
        , tdfData(filename.toStdString())
    {
        const IonMobilityOptions options = IonMobilityOptions::defaultValues();
        setOptions(options);
    }

    ~Private() { }
    void setOptions(const IonMobilityOptions &opt) { options = opt; }
    void setNumberRetainedPeaks(int value) { options.retainNumberOfPeaks = value; }

public:
    QSqlDatabase tdfDB;
    timsdata::TimsData tdfData;
    QString tdfDirectory;

    QVector<ScanRec> scans;
    int numSamples;
    double mzAcquisionLowRange;
    double mzAcquisionHighRange;
    IonMobilityOptions options;

    double m_cacheLastMobility = 0.0;
    int m_cacheLastPrecursorId = WRONG_ID;

    Q_DISABLE_COPY(Private);
};

MSReaderBrukerTims::MSReaderBrukerTims()
    : d(new Private(QString()))
{
}

MSReaderBrukerTims::~MSReaderBrukerTims()
{
}

void MSReaderBrukerTims::setIonMobilityOptions(const IonMobilityOptions &opt)
{
    MSReaderBase::setIonMobilityOptions(opt);
    d->setOptions(ionMobilityOptions());
}

void MSReaderBrukerTims::setNumberRetainedPeaks(int value)
{
    d->setNumberRetainedPeaks(value);
}

bool MSReaderBrukerTims::initialize()
{
    HRESULT hr = S_OK;

    const bool success = SUCCEEDED(hr);
    debugMs() << "MSReaderBrukerTims::initialize success=" << success;

    return success;
}

bool MSReaderBrukerTims::isInitialized() const
{
    return false;
}

bool MSReaderBrukerTims::canOpen(const QString &filename) const
{
    QFile tdfFile(filename + "/analysis.tdf");
    QFile binFile(filename + "/analysis.tdf_bin");
    if (!tdfFile.exists() || !binFile.exists()) {
        return false;
    }
    return VendorPathChecker::formatBruker(filename)
            == VendorPathChecker::ReaderBrukerFormatTims;
}

MSReaderBase::MSReaderClassType MSReaderBrukerTims::classTypeName() const
{
    return MSReaderClassTypeBrukerTims;
}

void getReducedMsData(uint32_t num_samples, const timsdata::FrameProxy& scans, std::map<uint32_t, float> &spectra)
{
    const auto maxScan = scans.getNbrScans();
    for (size_t scan = 0; scan < maxScan; ++scan) {
        const auto maxPeaks = scans.getNbrPeaks(scan);
        if (maxPeaks == 0) {
            continue;
        }

        const auto xAxis = scans.getScanX(scan);
        const auto yAxis = scans.getScanY(scan);
        for (size_t ii = 0; ii < maxPeaks; ++ii) {
            const uint32_t idxi = xAxis[ii];
            if (idxi >= num_samples) {
                continue;
            }

            // see https://en.cppreference.com/w/cpp/language/value_initialization
            // "Since C++11, value-initializing a class without a user-provided constructor,
            //  which has a member of a class type with a user-provided constructor zeroes out
            //  the member before calling its constructor"
            // default value is 0.0
            spectra[idxi] += yAxis[ii];
        }
    }
}

Err MSReaderBrukerTims::openFile(const QString &filename, MSConvertOption convert_options,
                                 QSharedPointer<ProgressBarInterface> progress)
{
    debugMs() << "Trying to open " << filename;
    debugMs() << "Options: summing_tolerance = " << d->options.summingTolerance << " "
              << (d->options.summingUnit == 0 ? "PPM" : "Dalton");
    debugMs() << "         merging_tolerance = " << d->options.mergingTolerance << " "
              << (d->options.mergingUnit == 0 ? "PPM" : "Dalton");
    debugMs() << "         retain_top_peaks = " << d->options.retainNumberOfPeaks;

    QSqlQuery q;
    const QFileInfo fileInfo(filename);
    setFilename(fileInfo.fileName());

    d.reset(new Private(filename));

    const QString tdfFile = filename + "\\analysis.tdf";

    // open the database connection
    Err e = kNoErr;
    {
        d->tdfDB = QSqlDatabase::addDatabase("QSQLITE", "tdfDB");
        d->tdfDB.setDatabaseName(tdfFile);
        if (!d->tdfDB.open()) {
            qWarning() << "Could not open file:" << tdfFile << endl;
            qWarning() << d->tdfDB.lastError().text();
            rrr(kError);
        }
    }

    d->numSamples = desired_num_samples; // set default values
    d->mzAcquisionLowRange = 0;
    d->mzAcquisionHighRange = 0;

    q = makeQuery(d->tdfDB, true);
    e = QEXEC_CMD(q, "SELECT Key,Value from GlobalMetadata;"); ree;
    while (q.next()) {
        auto key = q.value(0).toString();
        if (key != nullptr) {
            if (key.compare("DigitizerNumSamples") == 0) {
                int numSamples = q.value(1).toInt();
                if (numSamples > desired_num_samples) {
                    numSamples = desired_num_samples;
                }
                d->numSamples = numSamples;
            }
            if (key.compare("MzAcqRangeLower") == 0) {
                d->mzAcquisionLowRange = q.value(1).toDouble();
            }
            if (key.compare("DigitizerNumSamples") == 0) {
                d->mzAcquisionHighRange = q.value(1).toDouble();
            }
        }
    }

    if (buildScanTable() != kNoErr) {
        qWarning() << "Could not build scan table:" << tdfFile << endl;
        qWarning() << d->tdfDB.lastError().text();
        rrr(kError);
    }
    return e;
}

Err MSReaderBrukerTims::closeFile()
{
    if (d->tdfDB.isOpen()) {
        d->tdfDB.close();
    }
    return kNoErr;
}

Err MSReaderBrukerTims::getBasePeak(point2dList *points) const
{
    return kFunctionNotImplemented;
}

Err MSReaderBrukerTims::getTICData(point2dList *points) const
{
    return kFunctionNotImplemented;
}

Err MSReaderBrukerTims::getScanData(long _scanNumber, point2dList *points, bool do_centroiding,
                                    PointListAsByteArrays *pointListAsByteArrays)
{
    points->clear();
    Err e = kNoErr;
    ScanRec rec;
    point2d p;
    if (_scanNumber <= 0 || _scanNumber > d->scans.size()) {
        qWarning() << "out of range scanNumber=" << _scanNumber << endl;
        rrr(kError);
    }

    rec = d->scans[_scanNumber - 1];
    if (!rec.isValid()) {
        qWarning() << "invalid scan " << _scanNumber << endl;
        rrr(kError);
    }

    if (rec.msLevel == 1) {
        getMs1Data(_scanNumber, points);
    }
    else if (rec.msLevel > 1) {
        getMsMsData(_scanNumber, points);
    }
    if ((_scanNumber % 100)==0) {
        debugMs() << "done scan=" << _scanNumber;
    }

    if (pointListAsByteArrays) {
        static int count = 0;
        if (count++ < 5) {
            debugMs() << "MSReaderBruker::getScanData compression not handled yet";
        }
    }
    return e;
}

Err MSReaderBrukerTims::getXICData(const XICWindow &xicWindow, point2dList *points,
                                   int msLevel) const
{
    return kFunctionNotImplemented;
}

Err MSReaderBrukerTims::calculateMobility(const ScanRec& scanRec, double* mobility) const
{
    Err e = kNoErr;

    if (scanRec.precursorId == d->m_cacheLastPrecursorId) {
        *mobility = d->m_cacheLastMobility;
        return e;
    }

    QSqlQuery q;
    double tdfScanNumber = 0;
    q = makeQuery(d->tdfDB, true);
    e = QEXEC_CMD(q,
        QString("SELECT ScanNumber from Precursors Where Id=%1;")
        .arg(scanRec.precursorId)
        .toStdString()
        .c_str()); ree;

    if (q.next()) {
        tdfScanNumber = q.value(0).toDouble();
    }
    else {
        qWarning() << "no tdf scan number for precursorId=" << scanRec.precursorId << endl;
        rrr(kError);
    }

    try {
        std::vector<double> tdfScans, one_over_K0;
        tdfScans.push_back(tdfScanNumber);
        d->tdfData.scanNumToOneOverK0(scanRec.firstFrameId, tdfScans, one_over_K0);
        if (one_over_K0.size() > 0) {
            *mobility = one_over_K0[0];
        }
    }
    catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        rrr(kError);
    }

    d->m_cacheLastPrecursorId = scanRec.precursorId;
    d->m_cacheLastMobility = *mobility;

    return e;
}

Err MSReaderBrukerTims::getScanInfo(long scanNumber, ScanInfo *scanInfo) const
{
    Err e = kNoErr;
    QSqlQuery q;
    ScanRec rec;
    if (scanNumber <= 0 || scanNumber > static_cast<long>(d->scans.size())) {
        qWarning() << "out of range scanNumber=" << scanNumber << endl;
        rrr(kError);
    }

    rec = d->scans[scanNumber - 1];
    if (!rec.isValid()) {
        qWarning() << "invalid scan " << scanNumber << endl;
        rrr(kError);
    }

    double retentionTimeMinutes = 0;
    q = makeQuery(d->tdfDB, true);
    e = QEXEC_CMD(q,
                  QString("SELECT Time from Frames Where ID=%1;")
                      .arg(rec.firstFrameId)
                      .toStdString()
                      .c_str()); ree;
    if (q.next()) {
        retentionTimeMinutes = q.value(0).toDouble()/60.0;
    } else {
        qWarning() << "missing frame=" << rec.firstFrameId << endl;
        rrr(kError);
    }
    scanInfo->clear();
    scanInfo->retTimeMinutes = retentionTimeMinutes;
    scanInfo->scanLevel = rec.msLevel;
    scanInfo->nativeId = QString("scan=%1").arg(scanNumber);
    scanInfo->peakMode =  PeakPickingCentroid;
    scanInfo->scanMethod = ScanMethodFullScan;

    if (rec.msLevel > 1 && rec.msLevel < 9) {
        double mobility = 0.0;
        e = calculateMobility(rec, &mobility); ree;
        scanInfo->mobility.setMobilityValue(mobility);
    }
    return e;
}

Err MSReaderBrukerTims::getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const
{
    Err e = kNoErr;
    QSqlQuery q;
    ScanRec rec;
    if (scanNumber <= 0 || scanNumber > static_cast<long>(d->scans.size())) {
        qWarning() << "out of range scanNumber=" << scanNumber << endl;
        rrr(kError);
    }

    rec = d->scans[scanNumber - 1];
    if (!rec.isValid()) {
        qWarning() << "invalid scan " << scanNumber << endl;
        rrr(kError);
    }

    if (rec.precursorId <= 0) {
        qWarning() << "no precursor for MS1 scanNumber=" << scanNumber << endl;
        rrr(kError);
    }
    if (!d->tdfData.getHandle()) {
        qWarning() << "binary tdf data file not open" << endl;
        rrr(kError);
    }

    int charge = 0;
    double monoisotopicMz = 0;
    q = makeQuery(d->tdfDB, true);
    e = QEXEC_CMD(q,
                  QString("SELECT MonoisotopicMz, Charge from Precursors Where ID=%1;")
                      .arg(rec.precursorId)
                      .toStdString()
                      .c_str()); ree;

    if (q.next()) {
        monoisotopicMz = q.value(0).toDouble();
        charge = q.value(1).toDouble();
    }
    else {
        qWarning() << "no precursorId =" << rec.precursorId << endl;
        rrr(kError);
    }

    double isolationMz = 0;
    double isolationWidth = 0;
    q = makeQuery(d->tdfDB, true);
    e = QEXEC_CMD(q,
                  QString("SELECT IsolationMz, IsolationWidth from PasefFrameMsMsInfo Where "
                          "Frame=%1 AND Precursor=%2;")
                      .arg(rec.firstFrameId)
                      .arg(rec.precursorId)
                      .toStdString()
                      .c_str()); ree;

    if (q.next()) {
        isolationMz = q.value(0).toDouble();
        isolationWidth = q.value(1).toDouble();
    }
    else {
        qWarning() << "no precursorId =" << rec.precursorId << endl;
        rrr(kError);
    }

    pinfo->clear();
    pinfo->lowerWindowOffset = isolationWidth/2;
    pinfo->upperWindowOffset = isolationWidth/2;
    pinfo->nChargeState = charge;
    pinfo->nScanNumber = rec.parentScan;
    pinfo->nativeId = QString("scan=%1").arg(rec.parentScan);
    pinfo->dIsolationMass = isolationMz;
    pinfo->dMonoIsoMass = monoisotopicMz;

    return e;
}

Err MSReaderBrukerTims::getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const
{
    Err e = kNoErr;
    if (d->scans.size() == 0) {
        rrr(kError);
    }
    *totalNumber = d->scans.size();
    *startScan = 1;
    *endScan = d->scans.size();
    return e;
}

//TODO: move this to the Private class
Err MSReaderBrukerTims::buildScanTable()
{
    d->scans.clear();
    Err e = kNoErr;
    QSqlQuery q, q1;
    if (!d->tdfDB.isOpen()) {
        rrr(kFileOpenError);
    }
    bool ok1 = false;
    bool ok2 = false;

    int currentParent = 0;
    int parent_scan_number = 0;
    q = makeQuery(d->tdfDB, true);
    q1 = makeQuery(d->tdfDB, true);
    e = QEXEC_CMD(q, "SELECT ID, Parent from Precursors Order By Parent, ID;"); ree;
    while (q.next()) {
        int precursorId = q.value(0).toInt(&ok1);
        int parent = q.value(1).toInt(&ok2);
        if (!ok1 || !ok2) {
            d->scans.push_back(ScanRec::invalid());
            continue;
        }
        if (parent > currentParent) {
            d->scans.push_back(ScanRec(parent, -1, 1, 0));
            currentParent = parent;
            parent_scan_number = d->scans.size();
        }
        e = QEXEC_CMD(
            q1,
            QString("SELECT Frame from PasefFrameMsMsInfo Where Precursor=%1 Order By Frame;")
                .arg(precursorId)); ree;

        int firstFrame = 0;
        if (q1.next()) {
            firstFrame = q1.value(0).toInt();
        } else {
            d->scans.push_back(ScanRec::invalid());
            continue;
        }
        d->scans.push_back(ScanRec(firstFrame, precursorId, 2, parent_scan_number));
    }

    return e;
}

Err MSReaderBrukerTims::getMs1Data(long scanNumber, point2dList *ms1Spectra)
{
    Err e = kNoErr;
    QSqlQuery q;
    ScanRec rec;
    if (scanNumber <= 0 || scanNumber > static_cast<long>(d->scans.size())) {
        qWarning() << "out of range scanNumber=" << scanNumber << endl;
        rrr(kError);
    }

    rec = d->scans[scanNumber - 1];
    if (!rec.isValid()) {
        qWarning() << "invalid scan " << scanNumber << endl;
        rrr(kError);
    }

    if (rec.msLevel != 1 || rec.precursorId != -1) {
        qWarning() << "not MS1 scanNumber=" << scanNumber << endl;
        rrr(kError);
    }
    if (rec.firstFrameId <= 0) {
        qWarning() << "no frame id for MS1 scanNumber=" << scanNumber << endl;
        rrr(kError);
    }
    if (!d->tdfData.getHandle()) {
        qWarning() << "binary tdf data file not open" << endl;
        rrr(kError);
    }

    int numScans = 0, numPeaks = 0;
    q = makeQuery(d->tdfDB, true);
    e = QEXEC_CMD(q, QString("SELECT NumScans,NumPeaks from Frames Where ID=%1;").arg(rec.firstFrameId).toStdString().c_str()); ree;
    if (q.next()) {
        numScans = q.value(0).toDouble();
        numPeaks = q.value(1).toDouble();
    }
    else {
        qWarning() << "no frame=" << rec.firstFrameId << endl;
        rrr(kError);
    }

    try {
        std::map<uint32_t, float> spectra;

        // limit scans variable scope
        {
            ::timsdata::FrameProxy scans;
            d->tdfData.readScans(rec.firstFrameId, 0, numScans, &scans);
            getReducedMsData(d->numSamples, scans, spectra);
        }

        std::vector<double> indices;
        std::vector<float> reduced_spectra;
        getSpectraIndicies(spectra, indices);
        getSpectraValues(spectra, reduced_spectra);

        std::vector<double> xAxisMasses;
        d->tdfData.indexToMz(rec.firstFrameId, indices, xAxisMasses);

        point2dList tmpArray;
        tmpArray.reserve(reduced_spectra.size());
        for (size_t ii = 0; ii < reduced_spectra.size(); ii++) {
            const double mzi = xAxisMasses[ii];
            const double yi = reduced_spectra[ii];
            tmpArray.emplace_back(QPointF(mzi, yi));
        }
        std::vector<point2dList> ms_spectra(1);
        ms_spectra[0].swap(tmpArray);

        point2dList result;
        processPeaks(ms_spectra, result);
        retainTopIntensityPeaks(&result);
        ms1Spectra->swap(result);
    }
    catch (std::exception& e) {
        qWarning() << "Exception: " << e.what() << endl;
        rrr(kError);
    }
    return e;
}

Err MSReaderBrukerTims::getMsMsData(long scanNumber, point2dList *msMsSpectra)
{
    Err e = kNoErr;
    QString str; // debug
    QSqlQuery q;
    ScanRec rec;
    std::vector<MsMsInfo> msMsInfo;

    if (scanNumber <= 0 || scanNumber > static_cast<long>(d->scans.size())) {
        qWarning() << "out of range scanNumber=" << scanNumber << endl;
        rrr(kError);
    }

    rec = d->scans[scanNumber - 1];
    if (!rec.isValid()) {
        qWarning() << "invalid scan " << scanNumber << endl;
        rrr(kError);
    }

    if (rec.msLevel != 2 || rec.precursorId == -1) {
        qWarning() << "not MS1 scanNumber=" << scanNumber << endl;
        rrr(kError);
    }
    if (rec.firstFrameId <= 0) {
        qWarning() << "no frame id for MsMs scanNumber=" << scanNumber << endl;
        rrr(kError);
    }
    if (! d->tdfData.getHandle()) {
        qWarning() << "binary tdf data file not open" << endl;
        rrr(kError);
    }

    if (getMsMsInfo(rec.precursorId, &msMsInfo) != kNoErr){
        qWarning() << "can't get msmsinfo for precursorId=" << rec.precursorId << endl;
        rrr(kError);
    }

    double tdfScanNumber = 0;
    q = makeQuery(d->tdfDB, true);
    e = QEXEC_CMD(q,
                  QString("SELECT ScanNumber from Precursors Where Id=%1;")
                      .arg(rec.precursorId)
                      .toStdString()
                      .c_str()); ree;

    if (q.next()) {
        tdfScanNumber = q.value(0).toDouble();
    }
    else {
        qWarning() << "no tdf scan number for precursorId=" << rec.precursorId << endl;
        rrr(kError);
    }

    if (msMsInfo.size() > 0) {
        try {
            std::map<uint32_t, float> spectra;
            for (size_t ii = 0; ii < msMsInfo.size(); ii++) {
                ::timsdata::FrameProxy scans;
                const MsMsInfo msmsInfo = msMsInfo[ii];
                d->tdfData.readScans(msmsInfo.frameId, msmsInfo.scanNumBegin, msmsInfo.scanNumEnd,
                                     &scans);
                getReducedMsData(d->numSamples, scans, spectra);
            }

            std::vector<double> indices;
            std::vector<float> reduced_spectra;
            getSpectraIndicies(spectra, indices);
            getSpectraValues(spectra, reduced_spectra);

            std::vector<double> xAxisMasses;
            d->tdfData.indexToMz(msMsInfo[0].frameId, indices, xAxisMasses);

            point2dList tmpArray;
            tmpArray.reserve(reduced_spectra.size());
            for (size_t ii = 0; ii < reduced_spectra.size(); ii++) {
                const double mzi = xAxisMasses[ii];
                const double yi = reduced_spectra[ii];
                tmpArray.emplace_back(QPointF(mzi, yi));
            }
            std::vector<point2dList> msms_spectra(1);
            msms_spectra[0].swap(tmpArray);

            point2dList result;
            processPeaks(msms_spectra, result);
            retainTopIntensityPeaks(&result);
            msMsSpectra->swap(result);
        }
        catch (std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            rrr(kError);
        }

        /*
        std::ofstream myfile; //<============ debug
        myfile.open("E:/BrukerTIMS/msms_out.csv");
        for (size_t ii = 0; ii < msms_spectra.size(); ii++) {
            myfile << std::setprecision(15) << std::dec << msms_spectra[ii].first << "," << (int)msms_spectra[ii].second << std::endl;
        }
        myfile.close();
        */

    }
    return e;
}

Err MSReaderBrukerTims::getMsMsInfo(int precursorId, std::vector<MsMsInfo> *msMsInfo)
{
    (*msMsInfo).clear();
    Err e = kNoErr;
    QSqlQuery q;

    q = makeQuery(d->tdfDB, true);
    e = QEXEC_CMD(
        q,
        QString("SELECT Frame,ScanNumBegin,ScanNumEnd from PasefFrameMsMsInfo Where Precursor=%1;")
            .arg(precursorId)
            .toStdString()
            .c_str()); ree;

    while (q.next()) {
        int frame = q.value(0).toInt();
        int scanNumBegin = q.value(1).toInt();
        int scanNumEnd = q.value(2).toInt();
        (*msMsInfo).push_back(MsMsInfo(frame, scanNumBegin, scanNumEnd));
    }
    return e;
}

void MSReaderBrukerTims::sumNeighbors(const point2dList &data, point2dList& result)
{
    result.reserve(data.size());
    double epsilon;
    double ppmTolerance = 0;
    bool isPpm = (d->options.summingUnit == MassUnit::MassUnit_PPM);
    if (isPpm) {
        ppmTolerance = d->options.summingTolerance * MILLIONTH;
    } else {
        epsilon = d->options.summingTolerance; // fixed absolute tolerance
    }
    for (int ii = 0; ii < (int)data.size(); ii++) {
        const point2d pti = data[ii];
        double xi = pti.x();
        if (isPpm) {
            epsilon = xi * ppmTolerance; // relative ppm
            if (epsilon < MinPpmThreshold) {
                epsilon = MinPpmThreshold;
            }
        }

        double ysum = epsilon * pti.y();
        double xsum = xi * ysum;
        int jj = ii - 1;
        for (; jj >= 0; jj--) {
            const point2d ptj = data[jj];
            double xj = ptj.x();
            double delx = xi - xj;
            if (delx >= epsilon) {
                break;
            }
            double sy = (epsilon - delx) * ptj.y();
            ysum += sy;
            xsum += sy * xj;
        }

        jj = ii + 1;
        for (; jj < (int)data.size(); jj++) {
            const point2d ptj = data[jj];
            double xj = ptj.x();
            double delx = xj - xi;
            if (delx >= epsilon) {
                break;
            }
            double sy = (epsilon - delx) * ptj.y();
            ysum += sy;
            xsum += sy * xj;
        }
        result.push_back(point2d(xsum/ysum, ysum/epsilon));
    }
}


void MSReaderBrukerTims::filterPeaks(const point2dList &data, point2dList& result)
{
    std::vector<bool> marked(data.size());
    double epsilon;
    double ppmTolerance = 0;
    bool isPpm = (d->options.mergingUnit == MassUnit::MassUnit_PPM);
    if (isPpm) {
        ppmTolerance = d->options.mergingTolerance * MILLIONTH;
    } else {
        epsilon = d->options.mergingTolerance; // fixed absolute tolerance
    }
    for (int ii = 0; ii < (int)data.size(); ii++) {
        const point2d pti = data[ii];
        double xi = pti.x();
        if (isPpm) {
            epsilon = xi * ppmTolerance; // relative ppm
            if (epsilon < MinPpmThreshold) {
                epsilon = MinPpmThreshold;
            }
        }

        if (ii > 0) {
            const point2d ptj = data[ii - 1];
            double xj = ptj.x();
            double delx = xi - xj;
            if (delx < epsilon && pti.y() < ptj.y()) {
                marked[ii] = true;
            }
        }

        if (ii < (int)data.size() - 1) {
            const point2d ptj = data[ii + 1];
            double xj = ptj.x();
            double delx = xj - xi;
            if (delx < epsilon && pti.y() < ptj.y()) {
                marked[ii] = true;
            }
        }
    }

    for (int ii = 0; ii<(int)marked.size(); ii++) {
        if (!marked[ii]) {
            result.push_back(data[ii]);
        }
    }
}

void MSReaderBrukerTims::filterPairs(const point2dList &data, point2dList& result)
{
    double epsilon;
    double ppmTolerance = 0;
    bool isPpm = (d->options.mergingUnit == MassUnit::MassUnit_PPM);
    if (isPpm) {
        ppmTolerance = d->options.mergingTolerance * MILLIONTH;
    } else {
        epsilon = d->options.mergingTolerance; // fixed absolute tolerance
    }

    if (data.size() < 2) { // exit if no pairs
        result.clear();
        if (data.size() == 1) {
            result.push_back(data[0]);
        }
        return;
    }

    for (int ii = 0; ii < (int)data.size() - 1; ii++) {
        const point2d pti = data[ii];
        double xi = pti.x();
        if (isPpm) {
            epsilon = xi * ppmTolerance; // relative ppm
            if (epsilon < MinPpmThreshold) {
                epsilon = MinPpmThreshold;
            }
        }
        double yi = pti.y();

        const point2d ptj = data[ii + 1];
        double xj = ptj.x();
        double delx = xj - xi;
        bool found = false;
        if (delx < epsilon && yi == ptj.y()) { // found a pair of same intensity within merge distance

            bool begin_gap = true;
            if (ii > 0) {
                begin_gap = xi - data[ii - 1].x() >= epsilon;
            }
            bool end_gap = true;
            if (ii < (int)data.size() - 2) {
                end_gap = data[ii + 2].x() - xj >= epsilon;
            }
            if (begin_gap && end_gap) {
                ii++;
                found = true;
            }
        }
        if (found) {
            result.push_back(point2d((xi + xj) / 2, yi));
        }
        else {
            result.push_back(point2d(xi, yi));
        }
    }
}

void MSReaderBrukerTims::mergePointLists(const std::vector<point2dList> &data, point2dList& result)
{
    std::priority_queue<Triplet, std::vector<Triplet>, AscendingCompare> priority1;
    for (int ii=0; ii<(int)data.size(); ii++){
        if (data[ii].size()>0){
            point2d pti = data[ii].at(0);
            priority1.push(Triplet(pti.x(), pti.y(), ii));
        }
    }
    std::vector<int> next(data.size());

    Triplet prev(-1, -1, -1);
    while (priority1.size() > 0){
        Triplet min = priority1.top();
        priority1.pop();

        if (prev.x > 0){
            if (min.x == prev.x){
                prev.y += min.y;
                prev.index = min.index;
            } else {
                result.push_back(point2d(prev.x, prev.y));
                prev = min;
            }
        } else {
            prev = min;
        }

        int index = min.index;
        int nexti = next[index]+1;
        if (nexti < (int)data[index].size()){
            point2d pti = data[index].at(nexti);
            priority1.push(Triplet(pti.x(), pti.y(), index));
            next[index]++;
        }
    }
    result.push_back(point2d(prev.x, prev.y));
}

void MSReaderBrukerTims::retainTopIntensityPeaks(point2dList *result)
{
    if (d->options.retainNumberOfPeaks <= 0) {
        return;
    }
    const size_t topK = d->options.retainNumberOfPeaks;
    if (result->size() > topK) {
        std::partial_sort(result->begin(), result->begin() + topK, result->end(), decreasingIntensity);
        result->resize(topK);
        std::sort(result->begin(), result->end(), increasingMz);
    }
}

void MSReaderBrukerTims::processPeaks(const std::vector<point2dList> &data, point2dList& result)
{
    point2dList merged;
    mergePointLists(data, merged);

    point2dList summed;
    sumNeighbors(merged, summed);

    point2dList filtered;
    filterPeaks(summed, filtered);

    filterPairs(filtered, result);
}

// See PWIZ_API_DECL SpectrumPtr SpectrumList_Agilent::spectrum(size_t index, DetailLevel
// detailLevel, const pwiz::util::IntegerSet& msLevelsToCentroid) const
Err MSReaderBrukerTims::getFragmentType(long _scanNumber, long scanLevel,
                                        QString *fragType) const
{
    *fragType = "QTOF_HCD";  // following Marshall recommendation for Bruker TIMS
    // NOTE: PMI products do not yet know how to recognize this fragmentation label !!
    return kNoErr;
}

Err MSReaderBrukerTims::getChromatograms(QList<ChromatogramInfo> *chroList,
                                         const QString &internalChannelName)
{
    debugMs() << "calling "
              << "MSReaderBrukerTims::getChromatograms";

    chroList->clear();

    if (internalChannelName.isEmpty()) {
        // Get all chromotograms
    } else {
        // Get a specific chromatogram
    }

    return kFunctionNotImplemented;
}

Err MSReaderBrukerTims::getBestScanNumber(int msLevel, double scanTimeMinutes,
                                          long *scanNumber) const
{
    return kFunctionNotImplemented;
}

_MSREADER_END
_PMI_END
