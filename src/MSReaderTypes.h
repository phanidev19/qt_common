/*
* Copyright (C) 2015-2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*/

#ifndef MS_READER_TYPES_H
#define MS_READER_TYPES_H

#include "pmi_common_ms_export.h"

#include <math_utils_chemistry.h>

#include <common_math_types.h>
#include <pmi_core_defs.h>

#include <QDataStream>
#include <QString>

_PMI_BEGIN

class CompressionInfoHolder;

_MSREADER_BEGIN

//TODO: remove logic from data structures

//Note: Make sure to update SchemaDefinitions when updating these values
//Do not change the order of these values unless you are willing to deal with the backwards
//compatability issues
enum PeakPickingMode
{
    PeakPickingModeUnknown = 0,
    PeakPickingProfile,
    PeakPickingCentroid
};

enum ScanMethod
{
    ScanMethodUnknown = 0,
    ScanMethodFullScan,
    ScanMethodZoomScan
};

enum MSConvertOption
{
    ConvertChronosOnly = 0, //Call this to extract chromatogram only
    ConvertWithCentroidButNoPeaksBlobs, //Get chromatogram and Spectra but no Peaks blob content,
                                        //which would be populated later.
    ConvertFull //No filter
};

/*
 * \brief MobilityData contains the ion mobility data
 */
class PMI_COMMON_MS_EXPORT MobilityData
{
public:
    MobilityData();

    MobilityData(const MobilityData &) = default;
    MobilityData(MobilityData &&) = default;
    MobilityData &operator=(const MobilityData &) & = default;
    MobilityData &operator=(MobilityData &&) & = default;

    bool operator==(const MobilityData &m1) { return (m1.mobilityValue() == m_mobility); }
    bool operator!=(const MobilityData &m1) { return (m1.mobilityValue() != m_mobility); }

    void setMobilityValue(double mobility) { m_mobility = mobility; }
    double mobilityValue() const { return m_mobility; }

    bool isValid() const;

private:
    double m_mobility;
};

/*!
* \brief The MetaTextParser class helps parse .byspec2:Spectra:MetaText values.
*/
class PMI_COMMON_MS_EXPORT MetaTextParser
{
public:
    static ScanMethod getScanMethod(const QString& str);
    static PeakPickingMode getPeakPickingMode(const QString& str);
};

struct PMI_COMMON_MS_EXPORT XICWindow
{
    double mz;
    double mz_start;
    double mz_end;

    double time_start; ///assumed to be minutes
    double time_end; ///assumed to be minutes

    int iso_number;

    XICWindow();
    XICWindow(double startMz, double endMz, double startTime, double endTime);

    void setup(double mr, int charge, double ppmTolerance, double startTime, double endTime,
        int isotopeNumber);
    void clear();

    bool isValid() const;
    bool isMzRangeDefined() const;
    bool isTimeRangeDefined() const;
};

PMI_COMMON_MS_EXPORT QDataStream& operator<<(QDataStream& out, const XICWindow& win);
PMI_COMMON_MS_EXPORT QDataStream& operator>>(QDataStream& in, XICWindow& win);

struct PMI_COMMON_MS_EXPORT PrecursorInfo
{
    long nScanNumber;

    double dIsolationMass;
    double dMonoIsoMass;

    long nChargeState;
    double lowerWindowOffset;
    double upperWindowOffset;

    /// This refers to the parent native id
    QString nativeId;

    //Although the mobility data can be placed here, we don't expect two-pass mobility.
    //So, we'll place mobility in just ScanInfo instead.
    //MobilityData mobility;

    PrecursorInfo();
    PrecursorInfo(long scanNumber, double isolationMass, double monoIsoMass, long chargeState,
                  const QString &nativeId);

    void clear();
    /// Whether this data is valid; the function needs a refinement
    bool isValid() const;
};

struct PMI_COMMON_MS_EXPORT LockMassParameters
{
    double lockMassMz;
    double ppmTolerance;
    int movingAverageSize;

    LockMassParameters();
};

struct PMI_COMMON_MS_EXPORT ChromatogramInfo
{
    point2dList points;
    QString channelName;
    QString internalChannelName; // e.g. controller=1,n_controller=1,channel=1

    void clear();
};

struct PMI_COMMON_MS_EXPORT PointListAsByteArrays
{
    QByteArray dataX;
    QByteArray dataY;
    QVariant compressionInfoId;
    CompressionInfoHolder* compressionInfoHolder;

    PointListAsByteArrays();

    void clear();
};

struct PMI_COMMON_MS_EXPORT ScanInfo
{
    double retTimeMinutes;
    int scanLevel;
    QString nativeId;
    PeakPickingMode peakMode;
    ScanMethod scanMethod;
    MobilityData mobility;

    ScanInfo(double retTimeMinutes, int scanLevel, const QString& nativeId,
        PeakPickingMode peakMode, ScanMethod scanMethod);
    ScanInfo();

    void clear();
};

struct PMI_COMMON_MS_EXPORT ScanInfoWrapper {
    int scanNumber = -1;
    ScanInfo scanInfo;
};

//TODO: these mass tolerance class exists in QAL. Migrate that over and use it here.
enum MassUnit { MassUnit_PPM = 0, MassUnit_Dalton };

struct PMI_COMMON_MS_EXPORT IonMobilityOptions {
    double summingTolerance;
    double mergingTolerance; // merging tolerance may be different than summing tolerance
    MassUnit summingUnit;
    MassUnit mergingUnit;
    int retainNumberOfPeaks;

    IonMobilityOptions();

    static IonMobilityOptions defaultValues();
};

inline QDebug operator<<(QDebug dbg, const ScanInfo &c)
{
    dbg.nospace() << "(rt=" << c.retTimeMinutes << ",l=" << c.scanLevel << ",id=" << c.nativeId
        << ",m=" << c.peakMode << ",s=" << c.scanMethod << ")";

    return dbg.space();
}

_MSREADER_END

_PMI_END

#endif // MS_READER_TYPES_H
