/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NONUNIFORM_HILL_CLUSTER_H
#define NONUNIFORM_HILL_CLUSTER_H

#include "NonUniformHill.h"

#include "pmi_common_ms_export.h"

#include <pmi_core_defs.h>

#include <QSet>
#include <QVector>

_PMI_BEGIN

/*!
\class NonUniformHillCluster
\brief NonUniformHillCluster represents charge cluster, consists of N hills and has charge determined

\ingroup Feature finder for tiles

\reentrant
*/
class PMI_COMMON_MS_EXPORT NonUniformHillCluster
{

public:
    //! \brief Creates null cluster
    NonUniformHillCluster();

    //! \brief Creates cluster of hills with null charge, set charge with setCharge() after it is
    //! known \sa setCharge()
    explicit NonUniformHillCluster(const QVector<NonUniformHill> &hills);

    ~NonUniformHillCluster();

    int charge() const { return m_charge; }
    void setCharge(int clusterCharge) { m_charge = clusterCharge; }

    double monoisotopicMz() const { return m_monoisotopicMz; }
    void setMonoisotopicMz(double mz) { m_monoisotopicMz = mz; }

    QVector<NonUniformHill> hills() const { return m_hills; }
    void setHills(const QVector<NonUniformHill> &hills) { m_hills = hills; }

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    //!\brief \a hill will be added to the cluster
    void push_back(const NonUniformHill &hill);

    //!\brief returns true if cluster was created as null or does not have hills
    bool isNull() const;

    //!\brief Total points in cluster
    int totalPoints() const;

    QSet<QPoint> uniqueTilePositions() const;

    void setScanIndex(int scanIndex) { m_scanIndex = scanIndex; }
    int scanIndex() const { return m_scanIndex; }

    void setMaximumIntensity(double intensity) { m_maximumIntensity = intensity; }
    double maximumIntensity() const { return m_maximumIntensity; }

    ScanIndexInterval minMaxScanIndex() const;

    //! \brief returns true if cluster does not contain hills or hills does not contain points
    bool isEmpty() const;

private:
    int m_id;
    int m_charge;
    int m_scanIndex;
    double m_monoisotopicMz;
    double m_maximumIntensity;
    QVector<NonUniformHill> m_hills;
};

_PMI_END

#endif // NONUNIFORM_HILL_CLUSTER_H