/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef MSREADINFO_H
#define MSREADINFO_H

#include "MSReaderBase.h"
#include "pmi_common_ms_export.h"

#include <VectorStats.h>

#include <common_math_types.h>

#include <QtGlobal>

#include <utility>
#include <vector>

namespace pmi {
    class MSReader;
}

class PMI_COMMON_MS_EXPORT MSReaderInfo
{
public:
    MSReaderInfo(pmi::MSReader *reader);
    ~MSReaderInfo();

    // finds min and max of mz values of all 
    
    //! \brief Finds min and maximum mz in all scans
    //! @param centroided - if the scans are centroided or not
    //! @param ok - status, can file if the file does not contain ms data scans
    std::pair<double, double > mzMinMax(bool centroided, bool * ok) const;
    std::pair<int, int> scanNumberMinMax() const;

    int scanNumberCount() const;
    int maxMzItemsPerScan() const;

    //! \brief triggers the statistical computation which provides output for fetch*Stats functions
    // @param scanSelection - if empty, all scans are considered, otherwise selection of the scans provided is used for stats info
    void computeMSDataStatistics(const QList<pmi::msreader::ScanInfoWrapper> &scanSelection = QList<pmi::msreader::ScanInfoWrapper>());

    //! \brief provides random sub-selection of scans 
    QList<pmi::msreader::ScanInfoWrapper> randomScanSelection(int count);

    pmi::VectorStats<double> fetchMzMinStats();
    pmi::VectorStats<double> fetchMzMaxStats();
    pmi::VectorStats<double> fetchMzStepStats();

    pmi::VectorStats<double> fetchMinIntensityStats();
    pmi::VectorStats<double> fetchMaxIntensityStats();

    void mzStats(const pmi::point2dList &scanData, qreal * mzMin, qreal *mzMax, qreal *mzStep);
    void intensityStats(const pmi::point2dList &scanData, qreal * minIntensity, qreal *maxIntensity);

    void enableCentroiding(bool on) { m_doCentroiding = on; };
    bool isCentroidingEnabled() const { return m_doCentroiding; }

    //! \brief provide the number of points in all scans in the vendor file for given msLevel
    quint32 pointCount(int msLevel, bool doCentroiding);

    //! \brief Dumps scans to CSV in format "scanNumber, mz, intensity"
    //!
    //! Intended for debugging purposes mostly. Use enableCentroiding() to dump centroided data.
    //! @param filePath - output CSV file path
    //! \sa enableCentroiding(), isCentroidingEnabled() 
    bool dumpScansToCsv(const QString &filePath);

private:
    pmi::VectorStats<double> fetchStats(const std::vector<double> &data);
    void fetchStatData();

    void selectAllScans();

private:
    pmi::MSReader *m_reader;

    bool m_dataReady;

    std::vector<double> m_mzMinData;
    std::vector<double> m_mzMaxData;
    std::vector<double> m_mzStep;

    std::vector<double> m_intensityMin;
    std::vector<double> m_intensityMax;

    bool m_doCentroiding;

    QList<pmi::msreader::ScanInfoWrapper> m_scanSelection;
};

#endif
