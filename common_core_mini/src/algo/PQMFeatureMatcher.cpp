/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
*/

#include "PQMFeatureMatcher.h"

// stable
#include <common_constants.h>
#include <QtSqlUtils.h>

#include <QFileInfo>
#include <QSqlDatabase>
#include "ScopedQSqlDatabase.h"

const double DEFAULT_MS2_SEARCH_SCORE = 200.0;
const QString FEATURE_DATABASE_SUFFIX = QStringLiteral("ftrs");

_PMI_BEGIN

FeatureMatcherSettings::FeatureMatcherSettings()
    : score(DEFAULT_MS2_SEARCH_SCORE)
{
}

PQMFeatureMatcher::PQMFeatureMatcher(const QString &featuresFilePath, const QString &byrsltFilePath,
                                     const SettableFeatureFinderParameters &ffUserParams)
    : m_featureDBMsFilePath(featuresFilePath)
    , m_byrsltFilePath(byrsltFilePath)
    , m_ffUserParams(ffUserParams)
{
}

PQMFeatureMatcher::~PQMFeatureMatcher()
{
}

Err PQMFeatureMatcher::init()
{
    Err e = kNoErr;
    if (!QFileInfo::exists(m_featureDBMsFilePath) || !QFileInfo::exists(m_byrsltFilePath)) {
        rrr(kError);
    }

    e = startMatching(); ree;

    return e;
}

void PQMFeatureMatcher::setSettings(const FeatureMatcherSettings &settings)
{
    m_settings = settings;
}

FeatureMatcherSettings PQMFeatureMatcher::settings() const
{
    return m_settings;
}

Err PQMFeatureMatcher::startMatching() const
{
    Err e = kNoErr;

    QSqlDatabase featuresDB;
    QString featuresConnectionName = QStringLiteral("PQMFeatureMatcher_ftrs_%1")
                                         .arg(QFileInfo(m_featureDBMsFilePath).completeBaseName());
    ScopedQSqlDatabase featuresGuard(&featuresDB, m_featureDBMsFilePath, featuresConnectionName);
    e = featuresGuard.init(); ree;

    QSqlDatabase byrsltDB;
    QString byrsltConnectionName = QString("PQMFeatureMatcher_msms_%1").arg(QFileInfo(m_byrsltFilePath).completeBaseName());
    ScopedQSqlDatabase byrsltDBGuard(&byrsltDB, m_byrsltFilePath, byrsltConnectionName);
    e = byrsltDBGuard.init(); ree;

    QSqlQuery q = makeQuery(featuresDB, true);
    QString sql = "DELETE FROM msmsToFeatureMatches";
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    q = makeQuery(featuresDB, true);
    q.setForwardOnly(true);
    sql = "SELECT xicStart, xicEnd, rt, feature, mwMonoisotopic, maxIntensity, ID FROM Features;";

    e = QPREPARE(q, sql); ree;

    if (!q.exec()) {
        qDebug() << "Error getting charge cluster:" << q.lastError();
        return e;
    }

    bool ok = featuresDB.transaction();
    if (!ok) {
        return kError;
    }

    while (q.next()) {
        double xicStart = q.value(0).toDouble(&ok);
        Q_ASSERT(ok);
        double xicEnd = q.value(1).toDouble(&ok);
        Q_ASSERT(ok);
        int feature = q.value(3).toInt(&ok);
        Q_ASSERT(ok);
        double mwMonoisotopic = q.value(4).toDouble(&ok);
        Q_ASSERT(ok);
        int featureID = q.value(6).toInt(&ok);
        Q_ASSERT(ok);

        // std::cout << xicStart << " " << xicEnd << " " << maxIntensity << std::endl;

        QSqlQuery qByrslt = makeQuery(byrsltDB, true);
        sql = QString(
            R"(SELECT CalcMH, ApexPositTime, Peptide, Score, Peptide, Intensity, PQMs.Id
									FROM PQMs 
									INNER JOIN Queries ON PQMs.QueriesId = Queries.Id 
									INNER JOIN PQMsQueriesSummary ON PQMs.QueriesId = PQMsQueriesSummary.QueriesId 
									WHERE CalcMH >= (:CalcMH - :PPM) 
									AND CalcMH <= (:CalcMH + :PPM)
									AND Score > :Score
									AND ApexPositTime >= :xicStart
									AND ApexPositTime <= :xicEnd)");

        e = QPREPARE(qByrslt, sql); ree;
        qByrslt.bindValue(":CalcMH", mwMonoisotopic + HYDROGEN);
        qByrslt.bindValue(":PPM", calculatePPMTolerance(mwMonoisotopic, m_ffUserParams.ppm));
        qByrslt.bindValue(":xicStart", xicStart * 60);
        qByrslt.bindValue(":xicEnd", xicEnd * 60);
        qByrslt.bindValue(":Score", m_settings.score);
        e = QEXEC_NOARG(qByrslt); ree;

        std::vector<PQMResultItem> querySet;
        while (qByrslt.next()) {
            PQMResultItem resultItem;
            resultItem.peptide = qByrslt.value(4).toString();
            resultItem.calcMH = qByrslt.value(0).toDouble(&ok);
            Q_ASSERT(ok);
            resultItem.rt = qByrslt.value(1).toDouble(&ok) / 60;
            Q_ASSERT(ok);
            resultItem.score = qByrslt.value(3).toDouble(&ok);
            Q_ASSERT(ok);
            resultItem.intensity = qByrslt.value(5).toDouble(&ok);
            Q_ASSERT(ok);
            resultItem.id = qByrslt.value(6).toInt(&ok);
            Q_ASSERT(ok);
            querySet.push_back(resultItem);
        }

        if (!querySet.empty()) {
            std::sort(querySet.begin(), querySet.end(),
                      [](const PQMResultItem &left, const PQMResultItem &right) {
                          return (left.intensity > right.intensity);
                      });

            // std::cout << qByrslt.value(4).toString().toStdString() << std::endl;
            sql = QString(
                R"(INSERT INTO msmsToFeatureMatches (featureID, pqmID, peptide) VALUES (:featureID, :pqmID, :peptide))");
            QSqlQuery qFeatures = makeQuery(featuresDB, true);
            e = QPREPARE(qFeatures, sql); ree;
            qFeatures.bindValue(":featureID", featureID);
            qFeatures.bindValue(":pqmID", querySet[0].id);
            qFeatures.bindValue(":peptide", querySet[0].peptide);
            e = QEXEC_NOARG(qFeatures); ree;
        }
    }

    ok = featuresDB.commit();
    if (!ok) {
        return kError;
    }

    byrsltDB.close();
    featuresDB.close();
    return e;
}
double PQMFeatureMatcher::calculatePPMTolerance(double mw, int ppm)
{
    return (mw * ppm) / 1000000;
}

_PMI_END