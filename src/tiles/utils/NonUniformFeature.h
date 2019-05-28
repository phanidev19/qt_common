/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NONUNIFORM_FEATURE_H
#define NONUNIFORM_FEATURE_H

#include "NonUniformHillCluster.h"

#include "pmi_common_ms_export.h"

#include <pmi_core_defs.h>

#include <QVector>
#include <QMetaType>

_PMI_BEGIN

/*!
\class NonUniformFeature
\brief NonUniformFeature represents feature found by feature finder in MS1 scans

\ingroup Feature finder for tiles

\reentrant
*/
class PMI_COMMON_MS_EXPORT NonUniformFeature
{
public:
    NonUniformFeature();
    explicit NonUniformFeature(const QVector<NonUniformHillCluster> &chargeClusters);
    ~NonUniformFeature();

    QVector<NonUniformHillCluster> chargeClusters() const;

    void append(const NonUniformHillCluster& chargeCluster);

    void setId(int id) { m_id = id; }
    int id() const { return m_id; }

    void setUnchargedMass(double unchargedMass) { m_unchargedMass = unchargedMass; }
    double unchargedMass() const { return m_unchargedMass; }

    void setApexTime(double apexTime) { m_apexTime = apexTime; }
    double apexTime() const { return m_apexTime; }

    bool isNull() const;

    ScanIndexInterval minMaxScanIndex() const;

    double maximumIntensity() const;
    /*!
     * \brief Returns charges from clusters separated by ',' e.g. "2,3,5"
     */
    QString chargeList() const;

private:
    int m_id;
    double m_unchargedMass;
    double m_apexTime;
    QVector<NonUniformHillCluster> m_chargeClusters;
};

_PMI_END

Q_DECLARE_METATYPE(pmi::NonUniformFeature);

#endif // NONUNIFORM_FEATURE_H