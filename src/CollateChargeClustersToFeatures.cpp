/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */
#include "CollateChargeClustersToFeatures.h"
#include "ClusteringDBSCAN.h"



// common_stable
#include <QtSqlUtils.h>

#include <Eigen/Core>

_PMI_BEGIN

CollateChargeClustersToFeatures::CollateChargeClustersToFeatures(
    const QSqlDatabase &db, const SettableFeatureFinderParameters &ffUserParams)
    : m_db(db)
    , m_ffUserParams(ffUserParams)
{
}

CollateChargeClustersToFeatures::CollateChargeClustersToFeatures()
{
}

CollateChargeClustersToFeatures::~CollateChargeClustersToFeatures()
{
}

Err CollateChargeClustersToFeatures::init()
{
    Err e = kNoErr;
    ////TODO check to see if db is there
    e = loadChargeClustersFromDB(); ree;
    e = collateFeatures(); ree;
    e = clusterChargeClustersByFeature(); ree;
    return e;
}

Err CollateChargeClustersToFeatures::initTestingPurposes(
    const std::vector<MWChargeClusterPoint> &chargeClusters,
    const SettableFeatureFinderParameters &ffUserParams, 
    QVector<CrossSampleFeatureTurbo> *crossSampleFeautreTurbos)
{
    Err e = kNoErr;
    m_chargeClusters = chargeClusters;
    m_ffUserParams = ffUserParams;
    e = collateFeatures(); ree;
    e = clusterChargeClustersByFeatureTestingPurposes(crossSampleFeautreTurbos); ree;
    return e;
}

Err CollateChargeClustersToFeatures::collateFeatures()
{
    Err e = kNoErr;

    Eigen::MatrixXd matrixOfAllUnchargedMassesAndTheirIntensitiesForClustering(
        m_chargeClusters.size(), 2);
    for (size_t y = 0; y < m_chargeClusters.size(); ++y) {
        matrixOfAllUnchargedMassesAndTheirIntensitiesForClustering(y, 0)
            = m_chargeClusters[y].scanIndex;
        // Multiply by 10 to rescale y axis
        matrixOfAllUnchargedMassesAndTheirIntensitiesForClustering(y, 1)
            = m_chargeClusters[y].mwMonoisotopic * m_ffParams.dbscanMultiple;
    }

    ClusteringDBSCAN dbs(m_ffParams.epsilonDBSCAN, m_ffUserParams.minScanCount);
    Eigen::VectorXi labels
        = dbs.performDBSCAN(matrixOfAllUnchargedMassesAndTheirIntensitiesForClustering);

    MWChargeClusterPoint passingMWChargeClusterPoint;
    for (size_t y = 0; y < m_chargeClusters.size(); ++y) {
        passingMWChargeClusterPoint = m_chargeClusters[y];
        passingMWChargeClusterPoint.feature = labels(y);
        m_chargeClusters[y] = passingMWChargeClusterPoint;
    }

    std::sort(m_chargeClusters.begin(), m_chargeClusters.end(),
              [](const MWChargeClusterPoint &left, const MWChargeClusterPoint &right) {
                  return (left.feature < right.feature);
              });

    return e;
}

Err CollateChargeClustersToFeatures::saveFeatureToDb(double xicStart, double xicEnd, double rt,
                                                     int feature, double mwMonoisotopic,
                                                     double corrMax, double maxIntensity,
                                                     int ionCount, QString chargeOrder,
                                                     int isotopeCount) const
{
    Err e = kNoErr;

    QString sql = QString(
        R"(INSERT INTO Features (xicStart, xicEnd, rt, feature, mwMonoisotopic, corrMax, maxIntensity, ionCount, chargeOrder, maxIsotopeCount) 
VALUES (:xicStart, :xicEnd, :rt, :feature, :mwMonoisotopic, :corrMax, :maxIntensity, :ionCount, :chargeOrder, :maxIsotopeCount))");
    QSqlQuery q = makeQuery(m_db, true);
    e = QPREPARE(q, sql); ree;

    q.bindValue(":xicStart", xicStart);
    q.bindValue(":xicEnd", xicEnd);
    q.bindValue(":rt", rt);
    q.bindValue(":feature", feature);
    q.bindValue(":mwMonoisotopic", mwMonoisotopic);
    q.bindValue(":corrMax", corrMax);
    q.bindValue(":maxIntensity", maxIntensity);
    q.bindValue(":ionCount", ionCount);
    q.bindValue(":chargeOrder", chargeOrder);
    q.bindValue(":maxIsotopeCount", isotopeCount);
    e = QEXEC_NOARG(q); ree;

    return e;
}

Err CollateChargeClustersToFeatures::clusterChargeClustersByFeature()
{
    Err e = kNoErr;

    bool ok = m_db.transaction();
    if (!ok) {
        return kError;
    }

    int feature = -1;

    MWChargeClusterPoint tCluster;
    std::vector<int> chargeOrder;

    QVector<MWChargeClusterPoint> chargeClustersOfFeature;
    for (size_t i = 0; i < m_chargeClusters.size(); ++i) {

        tCluster = m_chargeClusters[i];
        if (tCluster.feature < 0) {
            continue;
        }

        if (feature != tCluster.feature) {
            if (feature != -1) {
                std::stable_sort(
                    chargeClustersOfFeature.begin(), chargeClustersOfFeature.end(),
                    [](const MWChargeClusterPoint &left, const MWChargeClusterPoint &right) {
                        return (left.rt < right.rt);
                    });
                double xicStart = chargeClustersOfFeature[0].rt;
                double xicEnd = chargeClustersOfFeature.back().rt;

                std::stable_sort(
                    chargeClustersOfFeature.begin(), chargeClustersOfFeature.end(),
                    [](const MWChargeClusterPoint &left, const MWChargeClusterPoint &right) {
                        return (left.maxIntensity > right.maxIntensity);
                    });
                double maxIntensity = chargeClustersOfFeature[0].maxIntensity;
                double mwMonoitotopic = chargeClustersOfFeature[0].mwMonoisotopic;
                double rt = chargeClustersOfFeature[0].rt;
                double corr = chargeClustersOfFeature[0].corr;
                int maxIsotopeCount = chargeClustersOfFeature[0].isotopeCount;
                int ionCount = chargeClustersOfFeature.size();

                QList<int> chargeOrder;
                for (int j = 0; j < static_cast<int>(chargeClustersOfFeature.size()); ++j) {
                    int charge = chargeClustersOfFeature[j].charge;
                    if (!std::count(chargeOrder.begin(), chargeOrder.end(), charge)) {
                        chargeOrder.push_back(charge);
                    }
                }

                QString chargeOrderString;
                for (int charge : chargeOrder) {
                    chargeOrderString += QString::number(charge) + ',';
                }

                e = saveFeatureToDb(xicStart, xicEnd, rt, feature + 1, mwMonoitotopic, corr,
                    maxIntensity, ionCount, chargeOrderString, maxIsotopeCount);  ree;

                chargeClustersOfFeature.clear();
            }
            chargeClustersOfFeature.push_back(tCluster);
            feature = tCluster.feature;
        } else {
            chargeClustersOfFeature.push_back(tCluster);
        }
    }

    if (!chargeClustersOfFeature.empty()) {

        std::sort(chargeClustersOfFeature.begin(), chargeClustersOfFeature.end(),
                  [](const MWChargeClusterPoint &left, const MWChargeClusterPoint &right) {
                      return (left.rt < right.rt);
                  });
        double xicStart = chargeClustersOfFeature[0].rt;
        double xicEnd = chargeClustersOfFeature.back().rt;

        std::sort(chargeClustersOfFeature.begin(), chargeClustersOfFeature.end(),
                  [](const MWChargeClusterPoint &left, const MWChargeClusterPoint &right) {
                      return (left.maxIntensity > right.maxIntensity);
                  });
        double maxIntensity = chargeClustersOfFeature[0].maxIntensity;
        double mwMonoitotopic = chargeClustersOfFeature[0].mwMonoisotopic;
        double rt = chargeClustersOfFeature[0].rt;
        double corr = chargeClustersOfFeature[0].corr;
        int maxIsotopeCount = chargeClustersOfFeature[0].isotopeCount;
        int ionCount = chargeClustersOfFeature.size();

        for (int j = 0; j < static_cast<int>(chargeClustersOfFeature.size()); ++j) {
            int charge = chargeClustersOfFeature[j].charge;
            if (!std::count(chargeOrder.begin(), chargeOrder.end(), charge)) {
                chargeOrder.push_back(charge);
            }
        }

        QString chargeOrderString;
        for (int charge : chargeOrder) {
            chargeOrderString += QString::number(charge) + ',';
        }

        e = saveFeatureToDb(xicStart, xicEnd, rt, feature + 1, mwMonoitotopic, corr, maxIntensity,
                            ionCount, chargeOrderString, maxIsotopeCount);  ree;
    }

    ok = m_db.commit();
    if (!ok) {
        return kError;
    }
    return e;
}

Err CollateChargeClustersToFeatures::clusterChargeClustersByFeatureTestingPurposes(QVector<CrossSampleFeatureTurbo> *crossSampleFeatureTurbosReturn)
{
    Err e = kNoErr;

    int feature = -1;

    QVector<CrossSampleFeatureTurbo> crossSampleFeatureTurbos;

    MWChargeClusterPoint tCluster;
    std::vector<int> chargeOrder;

    QVector<MWChargeClusterPoint> chargeClustersOfFeature;
    for (size_t i = 0; i < m_chargeClusters.size(); ++i) {

        tCluster = m_chargeClusters[i];
        if (tCluster.feature < 0) {
            continue;
        }

        if (feature != tCluster.feature) {
            if (feature != -1) {
                std::stable_sort(
                    chargeClustersOfFeature.begin(), chargeClustersOfFeature.end(),
                    [](const MWChargeClusterPoint &left, const MWChargeClusterPoint &right) {
                    return (left.rt < right.rt);
                });
                double xicStart = chargeClustersOfFeature[0].rt;
                double xicEnd = chargeClustersOfFeature.back().rt;

                std::stable_sort(
                    chargeClustersOfFeature.begin(), chargeClustersOfFeature.end(),
                    [](const MWChargeClusterPoint &left, const MWChargeClusterPoint &right) {
                    return (left.maxIntensity > right.maxIntensity);
                });
                double maxIntensity = chargeClustersOfFeature[0].maxIntensity;
                double mwMonoitotopic = chargeClustersOfFeature[0].mwMonoisotopic;
                double rt = chargeClustersOfFeature[0].rt;
                double corr = chargeClustersOfFeature[0].corr;
                int maxIsotopeCount = chargeClustersOfFeature[0].isotopeCount;
                int ionCount = chargeClustersOfFeature.size();

                QList<int> chargeOrder;
                for (int j = 0; j < static_cast<int>(chargeClustersOfFeature.size()); ++j) {
                    int charge = chargeClustersOfFeature[j].charge;
                    if (!std::count(chargeOrder.begin(), chargeOrder.end(), charge)) {
                        chargeOrder.push_back(charge);
                    }
                }

                QString chargeOrderString;
                for (int charge : chargeOrder) {
                    chargeOrderString += QString::number(charge) + ',';
                }

                CrossSampleFeatureTurbo crossSampleFeatureTurbo;
                crossSampleFeatureTurbo.chargeOrder = chargeOrderString;
                crossSampleFeatureTurbo.corrMax = corr;
                crossSampleFeatureTurbo.feature = feature + 1;
                crossSampleFeatureTurbo.ionCount = ionCount;
                crossSampleFeatureTurbo.maxIntensity = maxIntensity;
                crossSampleFeatureTurbo.maxIsotopeCount = maxIsotopeCount;
                crossSampleFeatureTurbo.mwMonoisotopic = mwMonoitotopic;
                crossSampleFeatureTurbo.rt = rt;
                crossSampleFeatureTurbo.xicStart = xicStart;
                crossSampleFeatureTurbo.xicEnd = xicEnd;

                crossSampleFeatureTurbos.push_back(crossSampleFeatureTurbo);

                chargeClustersOfFeature.clear();
            }
            chargeClustersOfFeature.push_back(tCluster);
            feature = tCluster.feature;
        }
        else {
            chargeClustersOfFeature.push_back(tCluster);
        }
    }

    if (!chargeClustersOfFeature.empty()) {

        std::sort(chargeClustersOfFeature.begin(), chargeClustersOfFeature.end(),
            [](const MWChargeClusterPoint &left, const MWChargeClusterPoint &right) {
            return (left.rt < right.rt);
        });
        double xicStart = chargeClustersOfFeature[0].rt;
        double xicEnd = chargeClustersOfFeature.back().rt;

        std::sort(chargeClustersOfFeature.begin(), chargeClustersOfFeature.end(),
            [](const MWChargeClusterPoint &left, const MWChargeClusterPoint &right) {
            return (left.maxIntensity > right.maxIntensity);
        });
        double maxIntensity = chargeClustersOfFeature[0].maxIntensity;
        double mwMonoitotopic = chargeClustersOfFeature[0].mwMonoisotopic;
        double rt = chargeClustersOfFeature[0].rt;
        double corr = chargeClustersOfFeature[0].corr;
        int maxIsotopeCount = chargeClustersOfFeature[0].isotopeCount;
        int ionCount = chargeClustersOfFeature.size();

        for (int j = 0; j < static_cast<int>(chargeClustersOfFeature.size()); ++j) {
            int charge = chargeClustersOfFeature[j].charge;
            if (!std::count(chargeOrder.begin(), chargeOrder.end(), charge)) {
                chargeOrder.push_back(charge);
            }
        }

        QString chargeOrderString;
        for (int charge : chargeOrder) {
            chargeOrderString += QString::number(charge) + ',';
        }

        CrossSampleFeatureTurbo crossSampleFeatureTurbo;
        crossSampleFeatureTurbo.chargeOrder = chargeOrderString;
        crossSampleFeatureTurbo.corrMax = corr;
        crossSampleFeatureTurbo.feature = feature + 1;
        crossSampleFeatureTurbo.ionCount = ionCount;
        crossSampleFeatureTurbo.maxIntensity = maxIntensity;
        crossSampleFeatureTurbo.maxIsotopeCount = maxIsotopeCount;
        crossSampleFeatureTurbo.mwMonoisotopic = mwMonoitotopic;
        crossSampleFeatureTurbo.rt = rt;
        crossSampleFeatureTurbo.xicStart = xicStart;
        crossSampleFeatureTurbo.xicEnd = xicEnd;

        crossSampleFeatureTurbos.push_back(crossSampleFeatureTurbo);
    }

    *crossSampleFeatureTurbosReturn = crossSampleFeatureTurbos;
    return e;
}

Err CollateChargeClustersToFeatures::loadChargeClustersFromDB()
{
    std::vector<MWChargeClusterPoint> pointsOfReturn;

    QSqlQuery q = makeQuery(m_db, true);
    QString sql = "SELECT ID, scanIndex, vendorScanNumber, rt, mzFound, maxIntensity, "
                  "mwMonoisotopic, monoOffset, corr, charge, isotopeCount  FROM ChargeClusters;";

    Err e = QPREPARE(q, sql); ree;

    if (!q.exec()) {
        qDebug() << "Error getting charge cluster:" << q.lastError();
        return e;
    }

    bool ok;

    while (q.next()) {
        MWChargeClusterPoint tchargeCluster;
        tchargeCluster.ID = q.value(0).toInt(&ok);
        Q_ASSERT(ok);
        tchargeCluster.scanIndex = q.value(1).toInt(&ok);
        Q_ASSERT(ok);
        tchargeCluster.vendorScanNumber = q.value(2).toInt(&ok);
        Q_ASSERT(ok);
        tchargeCluster.rt = q.value(3).toDouble(&ok);
        Q_ASSERT(ok);
        tchargeCluster.mzFound = q.value(4).toDouble(&ok);
        Q_ASSERT(ok);
        tchargeCluster.maxIntensity = q.value(5).toDouble(&ok);
        Q_ASSERT(ok);
        tchargeCluster.mwMonoisotopic = q.value(6).toDouble(&ok);
        Q_ASSERT(ok);
        tchargeCluster.monoOffset = q.value(7).toInt(&ok);
        Q_ASSERT(ok);
        tchargeCluster.corr = q.value(8).toDouble(&ok);
        Q_ASSERT(ok);
        tchargeCluster.charge = q.value(9).toInt(&ok);
        Q_ASSERT(ok);
        tchargeCluster.isotopeCount = q.value(10).toInt(&ok);
        Q_ASSERT(ok);
        m_chargeClusters.push_back(tchargeCluster);
    }

    return e;
}

_PMI_END
