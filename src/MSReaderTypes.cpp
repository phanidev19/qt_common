/*
* Copyright (C) 2015-2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*/

#include "MSReaderTypes.h"

_PMI_BEGIN

_MSREADER_BEGIN

//Hiding this in cpp to avoid using this outside of this file.
//This will allow us to change this value to something else if needed.
const double INVALID_MOBILITY_VALUE = -1.0;

MobilityData::MobilityData()
    : m_mobility(INVALID_MOBILITY_VALUE)
{
}

bool MobilityData::isValid() const
{
    return m_mobility != INVALID_MOBILITY_VALUE;
}

ScanMethod MetaTextParser::getScanMethod(const QString& metaStr)
{
    if (metaStr.contains("<ScanMethod>")) {
        if (metaStr.contains("zoom scan")) {
            return ScanMethodZoomScan;
        }

        if (metaStr.contains("full scan")) {
            return ScanMethodFullScan;
        }
    }

    return ScanMethodUnknown;
}

PeakPickingMode MetaTextParser::getPeakPickingMode(const QString& metaStr)
{
    if (metaStr.contains("<PeakMode>")) {
        if (metaStr.contains("centroid")) {
            return PeakPickingCentroid;
        }

        if (metaStr.contains("profile")) {
            return PeakPickingProfile;
        }
    }

    return PeakPickingModeUnknown;
}


XICWindow::XICWindow()
{
    clear();
}

// Needed for auto tests but probably creates an invalid object
XICWindow::XICWindow(double startMz, double endMz, double startTime, double endTime)
{
    clear();

    mz_start = startMz;
    mz_end = endMz;
    time_start = startTime;
    time_end = endTime;

    mz = (mz_start + mz_end)*0.5;
}

void XICWindow::setup(double mr, int charge, double ppmTolerance, double startTime, double endTime,
    int isotopeNumber)
{
    if (isotopeNumber >= 0) {
        mr = mr + isotopeNumber * ISODIFF;
    }
    else {
        //Temporarly assume negatie isotopes are to extract deamidated masses
        mr = mr + isotopeNumber * DEAMIDATED_MASS;
    }

    mz = mz_from_mr(mr, charge);

    const double mzTol = compute_ppm_tolerance(mz, ppmTolerance);

    mz_start = mz - mzTol;
    mz_end = mz + mzTol;

    time_start = startTime;
    time_end = endTime;

    iso_number = isotopeNumber;
}

void XICWindow::clear()
{
    mz = -1.0;
    mz_start = -1.0;
    mz_end = -1.0;

    time_start = 0.0;
    time_end = 0.0;

    iso_number = 0;
}

bool XICWindow::isValid() const
{
    return mz != -1;
}

bool XICWindow::isMzRangeDefined() const
{
    return mz_start != -1.0 && mz_end != -1.0;
}

bool XICWindow::isTimeRangeDefined() const
{
    // Is that correct?
    return time_start != 0.0 || time_end != 0.0;
}

QDataStream& operator<<(QDataStream& out, const XICWindow& win)
{
    out << win.mz_start << win.mz_end;
    out << win.time_start << win.time_end;

    return out;
}

QDataStream& operator>>(QDataStream& in, XICWindow& win)
{
    in >> win.mz_start >> win.mz_end;
    in >> win.time_start >> win.time_end;

    return in;
}

PrecursorInfo::PrecursorInfo()
{
    clear();
}

PrecursorInfo::PrecursorInfo(long scanNumber, double isolationMass, double monoIsoMass,
    long chargeState, const QString& nativeId)
    : nScanNumber(scanNumber)
    , dIsolationMass(isolationMass)
    , dMonoIsoMass(monoIsoMass)
    , nChargeState(chargeState)
    , lowerWindowOffset(0)
    , upperWindowOffset(0)
    , nativeId(nativeId)
{
}

void PrecursorInfo::clear()
{
    nScanNumber = -1;

    dIsolationMass = 0;
    dMonoIsoMass = 0;

    nChargeState = 1;
    lowerWindowOffset = 0;
    upperWindowOffset = 0;

    nativeId.clear();
}

bool PrecursorInfo::isValid() const
{
    return nScanNumber >= 0;
}


LockMassParameters::LockMassParameters()
{
    lockMassMz = 0;
    ppmTolerance = 20;
    movingAverageSize = 50;  //NOTE: Not yet hooked from UI
}


void ChromatogramInfo::clear()
{
    points.clear();
    channelName.clear();
    internalChannelName.clear();
}

PointListAsByteArrays::PointListAsByteArrays()
{
    clear();
}

void PointListAsByteArrays::clear()
{
    dataX.clear();
    dataY.clear();
    compressionInfoId = QVariant();
    compressionInfoHolder = nullptr;
}

static const int top_k_peaks = 5000; // number of peaks to retain in re-centroiding

IonMobilityOptions::IonMobilityOptions()
{
    summingTolerance = 0.1; // Dalton
    summingUnit = MassUnit_Dalton;
    mergingTolerance = summingTolerance;
    mergingUnit = summingUnit;
    retainNumberOfPeaks = top_k_peaks;
}

IonMobilityOptions IonMobilityOptions::defaultValues()
{
    return IonMobilityOptions();
}


_MSREADER_END

_PMI_END
