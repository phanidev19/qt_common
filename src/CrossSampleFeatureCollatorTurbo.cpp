/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "CrossSampleFeatureCollatorTurbo.h"

// stable
#include "ProgressContext.h"
#include "pmi_common_ms_debug.h"
#include <QtSqlUtils.h>

const QString DEFAULT_FILENAME = QStringLiteral("Comparitron.nslco");
const int VECTOR_GRANULARITY = 1000;
const int VECTOR_GRANULARITY_TIME = 10000;

_PMI_BEGIN

CrossSampleFeatureCollatorTurbo::CrossSampleFeatureCollatorTurbo(
    const QVector<SampleFeaturesTurbo> &sampleFeatures,
    const SettableFeatureFinderParameters &ffUserParams)
    : m_sampleFeatures(sampleFeatures)
    , m_ppmTolerance(ffUserParams.ppm)
    , m_ffUserParams(ffUserParams)
{
    if (!sampleFeatures.isEmpty()) {
        // let's go to the first base directory
        m_masterFeaturesDBPath = QFileInfo(sampleFeatures.at(0).sampleFilePath)
                                     .absoluteDir()
                                     .filePath(DEFAULT_FILENAME);
    }
}


CrossSampleFeatureCollatorTurbo::CrossSampleFeatureCollatorTurbo(
    const QVector<SampleFeaturesTurbo> &sampleFeatures, const QDir &projectDir,
    const SettableFeatureFinderParameters &ffUserParams)
    : CrossSampleFeatureCollatorTurbo(sampleFeatures, ffUserParams)
{
    m_masterFeaturesDBPath = projectDir.filePath(DEFAULT_FILENAME);
}


CrossSampleFeatureCollatorTurbo::~CrossSampleFeatureCollatorTurbo()
{
}


void CrossSampleFeatureCollatorTurbo::setPpmTolerance(double ppmTolerance)
{
    m_ppmTolerance = ppmTolerance;
}


double CrossSampleFeatureCollatorTurbo::ppmTolerance() const
{
    return m_ppmTolerance;
}


QString CrossSampleFeatureCollatorTurbo::masterFeaturesDatabasePath() const
{
    return m_masterFeaturesDBPath;
}


QVector<CrossSampleFeatureTurbo> CrossSampleFeatureCollatorTurbo::consolidatedCrossSampleFeatures() const
{
    return m_consolidatedCrossSampleFeatures;
}


Err CrossSampleFeatureCollatorTurbo::init()
{
    Err e = kNoErr;

    if (m_sampleFeatures.isEmpty() || m_masterFeaturesDBPath.isEmpty()) {
        rrr(kBadParameterError);
    }

    m_msFilePaths.reserve(m_sampleFeatures.size());
    m_featureFiles.reserve(m_sampleFeatures.size());
    m_msids.reserve(m_sampleFeatures.size());

    int id = 1;
    for (const SampleFeaturesTurbo &item : m_sampleFeatures) {
        m_msFilePaths.push_back(item.sampleFilePath);
        m_featureFiles.push_back(item.featuresFilePath);
        m_msids.push_back(id);
        id++;
    }

    e = m_warpCores.constructTimeWarp(m_msFilePaths, m_msids); ree;
    e = consolidateFeaturesAcrossSamplesForCollation(); ree;
    

    return e;
}


Err CrossSampleFeatureCollatorTurbo::run(QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    if (progress) {
        progress->setText(QObject::tr("Collating features across samples"));
    }

    e = crossSampleFeatureGrouping(); ree;
    e = consolidateDuplicateSampleIdsFoundInMasterFeature(); ree;
    e = transferMasterFeaturesToDatabase(); ree;

    return e;
}


Err CrossSampleFeatureCollatorTurbo::testCollation(
    const QVector<CrossSampleFeatureTurbo> &consolidatedCrossSampleFeatures, bool removeDuplicates)
{
    Err e = kNoErr;

    m_consolidatedCrossSampleFeatures = consolidatedCrossSampleFeatures;
    e = crossSampleFeatureGrouping(); ree;
    if (removeDuplicates) {
        e = consolidateDuplicateSampleIdsFoundInMasterFeature(); ree;
    }
    

    return e;
}


//TODO (Drew Nichols 2019-04-14) This is repeated constantly. Find a home for this method.
inline double calculatePPMTolerance(const double &mw, const int &ppm)
{
    return (mw * ppm) / 1000000;
}


inline double roundValue(double val, int precision) {
    return std::round(val * precision) / static_cast<double>(precision);
}


Err CrossSampleFeatureCollatorTurbo::consolidateFeaturesAcrossSamplesForCollation()
{
    Err e = kNoErr;

    for (int i = 0; i < static_cast<int>(m_featureFiles.size()); ++i) {
     
        TimeWarp2D warpCore;
        e = m_warpCores.getTimeWarp(i + 1, &warpCore); ree;

        QString fileName = m_featureFiles[i];

        QSqlDatabase featureDB;
        e = addDatabaseAndOpen(fileName, fileName, featureDB);
        if (e != kNoErr) {
            qDebug() << "Failed to load or create database"
                     << QString::fromStdString(pmi::convertErrToString(e));
        }

        QSqlQuery q = makeQuery(featureDB, true);
        QString sql = "SELECT xicStart, xicEnd, rt, mwMonoisotopic, corrMax, maxIntensity, "
                      "ionCount, maxIsotopeCount, chargeOrder, feature FROM Features WHERE "
                      "mwMonoisotopic > "
                      ":mw;";
        e = QPREPARE(q, sql); ree;
        q.bindValue(":mw", m_ffUserParams.minFeatureMass); 
        if (!q.exec()) {
            qDebug() << "Error getting charge cluster:" << q.lastError();
            return e;
        }

        bool ok = true;
        while (q.next()) {
            double xicStart = q.value(0).toDouble(&ok);
            Q_ASSERT(ok);
            double xicEnd = q.value(1).toDouble(&ok);
            Q_ASSERT(ok);
            double rt = q.value(2).toDouble(&ok);
            Q_ASSERT(ok);
            double mwMonoisotopic = q.value(3).toDouble(&ok);
            Q_ASSERT(ok);
            double corrMax = q.value(4).toDouble(&ok);
            Q_ASSERT(ok);
            double maxIntensity = q.value(5).toDouble(&ok);
            Q_ASSERT(ok);
            int ionCount = q.value(6).toInt(&ok);
            Q_ASSERT(ok);
            int maxIsotopeCount = q.value(7).toInt(&ok);
            Q_ASSERT(ok);
            QVariant chargeOrder = q.value(8);
            int feature = q.value(9).toInt(&ok);
            Q_ASSERT(ok);

            if (mwMonoisotopic > m_ffUserParams.maxFeatureMass 
                || mwMonoisotopic < m_ffUserParams.minFeatureMass
                || (xicEnd - xicStart) < m_ffUserParams.minPeakWidthMinutes) {
                continue;
            }

            CrossSampleFeatureTurbo crossSampleFeature;
            crossSampleFeature.xicStart = roundValue(xicStart, VECTOR_GRANULARITY_TIME);
            crossSampleFeature.xicEnd = roundValue(xicEnd, VECTOR_GRANULARITY_TIME);
            crossSampleFeature.xicStartWarped
                = roundValue(warpCore.warp(xicStart), VECTOR_GRANULARITY_TIME);
            crossSampleFeature.xicEndWarped
                = roundValue(warpCore.warp(xicEnd), VECTOR_GRANULARITY_TIME);
            crossSampleFeature.rt = roundValue(rt, VECTOR_GRANULARITY_TIME);
            crossSampleFeature.rtWarped = roundValue(warpCore.warp(rt), VECTOR_GRANULARITY_TIME);
            crossSampleFeature.mwMonoisotopic = roundValue(mwMonoisotopic, VECTOR_GRANULARITY);
            crossSampleFeature.corrMax = corrMax;
            crossSampleFeature.maxIntensity = maxIntensity;
            crossSampleFeature.maxIsotopeCount = maxIsotopeCount;
            crossSampleFeature.ionCount = ionCount;
            crossSampleFeature.chargeOrder = chargeOrder.toString();
            crossSampleFeature.fileName = fileName;
            crossSampleFeature.sampleId = i + 1;
            crossSampleFeature.feature = feature;

            m_consolidatedCrossSampleFeatures.push_back(crossSampleFeature);
        }
        featureDB.close();
    }

	debugMs() << "Number Of Features to Collate:" << m_consolidatedCrossSampleFeatures.size();
    return e;
}


inline double calculateIntersectionOverUnion(double start1, double end1, double start2, double end2)
{

    QVector<double> points = { start1, start2, end1, end2 };
    std::sort(points.begin(), points.end());

    double min = points.front();
    double max = points.back();

    double intersection = start1 < start2 ? end1 - start2 : end2 - start1;

    return intersection / (max - min);
}


Err CrossSampleFeatureCollatorTurbo::crossSampleFeatureGrouping()
{
    Err e = kNoErr;

    std::sort(m_consolidatedCrossSampleFeatures.begin(), m_consolidatedCrossSampleFeatures.end(),
        [](const CrossSampleFeatureTurbo &a, const CrossSampleFeatureTurbo &b) {return a.rtWarped < b.rtWarped; });

    int masterFeature = 0;
    for (int i = 0; i < static_cast<int>(m_consolidatedCrossSampleFeatures.size()); ++i) {

        CrossSampleFeatureTurbo &row = m_consolidatedCrossSampleFeatures[i];
        if (row.masterFeature != NOT_SET) {
            continue;
        }

        row.masterFeature = masterFeature;
        double massTolerance = calculatePPMTolerance(row.mwMonoisotopic, m_ffUserParams.ppm);

        for (int j = i + 1; j < m_consolidatedCrossSampleFeatures.size(); ++j) {
            
            CrossSampleFeatureTurbo &toCompareCurrentRow 
                = m_consolidatedCrossSampleFeatures[j];
            if (toCompareCurrentRow.masterFeature != NOT_SET) {
                continue;
            }

            if (std::abs(toCompareCurrentRow.mwMonoisotopic - row.mwMonoisotopic) <= massTolerance
                && std::abs(toCompareCurrentRow.rtWarped - row.rtWarped) <= m_ffParams.maxTimeToleranceWarped) {
                toCompareCurrentRow.masterFeature = masterFeature;
            }
        }

        masterFeature++;
    }

    std::sort(m_consolidatedCrossSampleFeatures.begin(), m_consolidatedCrossSampleFeatures.end(),
              [](const CrossSampleFeatureTurbo &left, const CrossSampleFeatureTurbo &right) {
                  return (left.masterFeature < right.masterFeature);
              });

    return e;
}


Err CrossSampleFeatureCollatorTurbo::createCrossSampleFeatureCollatedSqliteDbSchema(
    QSqlDatabase &db, const QString &comparitronFilePath)
{
    Err e = kNoErr;

    if (QFileInfo(comparitronFilePath).exists()) {
        warningMs() << "Master nslco cache already exists. Removing " << comparitronFilePath;
        bool ok = QFile::remove(comparitronFilePath);
        if (!ok) {
            warningMs() << "Failed to remove master nslco cache!";
            rrr(kError);
        }
    }

    e = addDatabaseAndOpen(QStringLiteral("CrossSampleFeatureCollator_nslco_cache"),
                           comparitronFilePath, db); ree;
    if (e != kNoErr) {
        qDebug() << "Failed to load or create database"
                 << QString::fromStdString(pmi::convertErrToString(e));
    }

    QString sql
        = QString("CREATE TABLE IF NOT EXISTS FFMatchesCross ( ID INTEGER PRIMARY KEY NOT NULL, "
                  "masterFeature INT,"
                  "sampleID INT, "
                  "feature INT, "
                  "xicStart FLOAT, "
                  "xicEnd FLOAT, "
                  "rt FLOAT, "
                  "rtWarped FLOAT, "
                  "xicStartWarped FLOAT, "
                  "xicEndWarped FLOAT, "
                  "maxIntensity INT, "
                  "mwMonoisotopic FLOAT, "
                  "corr FLOAT, "
                  "chargeOrder VARCHAR(50), "
                  "maxIsotopeCount INT)");

    QSqlQuery q = makeQuery(db, true);
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    sql = "DELETE FROM FFMatchesCross";
    q = makeQuery(db, true);
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    sql = "CREATE TABLE IF NOT EXISTS FFMatchesCrossQuantification ( ID INTEGER PRIMARY KEY NOT "
          "NULL, "
          "masterFeature INT,"
          "sampleID INT, "
          "auc INT, "
          "xicStart FLOAT, "
          "xicEnd FLOAT, "
          "rt FLOAT, "
          "maxIntensity INT)";

    q = makeQuery(db, true);
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    sql = "DELETE FROM FFMatchesCrossQuantification";
    q = makeQuery(db, true);
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    sql = "CREATE TABLE IF NOT EXISTS FeatureFiles ( ID INTEGER PRIMARY KEY NOT NULL, "
          "fileName VARCHAR(100),"
          "fullPath VARCHAR(300),"
          "msFullPath VARCHAR(300))";

    q = makeQuery(db, true);
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    sql = "DELETE FROM FeatureFiles";
    q = makeQuery(db, true);
    e = QEXEC_CMD(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    return e;
}


Err CrossSampleFeatureCollatorTurbo::transferMasterFeaturesToDatabase()
{
    Err e = kNoErr;

    if (m_sampleFeatures.isEmpty() || m_masterFeaturesDBPath.isEmpty()) {
        rrr(kBadParameterError);
    }

    QSqlDatabase db;

    e = createCrossSampleFeatureCollatedSqliteDbSchema(db, m_masterFeaturesDBPath); ree;
    debugMs() << "Saving master features to" << m_masterFeaturesDBPath;

    // save filename - filepath
    QString sql
        = R"(INSERT INTO FeatureFiles (fileName, fullPath, msFullPath) VALUES (:fileName, :fullPath, :msFullPath))";
    QSqlQuery qFileNames = makeQuery(db, true);
    e = QPREPARE(qFileNames, sql); ree;

    for (int i = 0; i < static_cast<int>(m_msids.size()); ++i) {
        qFileNames.bindValue(":fileName", QFileInfo(m_featureFiles[i]).fileName());
        qFileNames.bindValue(":fullPath", m_featureFiles[i]);
        qFileNames.bindValue(":msFullPath", m_msFilePaths[i]);
        e = QEXEC_NOARG(qFileNames); ree;
    }

    bool ok = db.transaction();
    if (!ok) {
        rrr(kError);
    }

    sql = R"(INSERT INTO FFMatchesCross (masterFeature, sampleID, feature, xicStart, xicEnd, rt, maxIntensity, mwMonoisotopic, corr, chargeOrder, maxIsotopeCount, rtWarped, xicStartWarped, xicEndWarped)
                                VALUES (:masterFeature, :sampleID, :feature, :xicStart, :xicEnd, :rt, :maxIntensity, :mwMonoisotopic, :corr, :chargeOrder, :maxIsotopeCount, :rtWarped, :xicStartWarped, :xicEndWarped))";

    for (const CrossSampleFeatureTurbo &feature : m_consolidatedCrossSampleFeatures) {
        QSqlQuery qqq = makeQuery(db, true);
        e = QPREPARE(qqq, sql); ree;

        qqq.bindValue(":masterFeature", feature.masterFeature);
        qqq.bindValue(":sampleID", feature.sampleId);
        qqq.bindValue(":feature", feature.feature);
        qqq.bindValue(":xicStart", feature.xicStart);
        qqq.bindValue(":xicEnd", feature.xicEnd);
        qqq.bindValue(":rt", feature.rt);
        qqq.bindValue(":maxIntensity", feature.maxIntensity);
        qqq.bindValue(":mwMonoisotopic", feature.mwMonoisotopic);
        qqq.bindValue(":corr", feature.corrMax);
        qqq.bindValue(":chargeOrder", feature.chargeOrder);
        qqq.bindValue(":maxIsotopeCount", feature.maxIsotopeCount);
        qqq.bindValue(":rtWarped", feature.rtWarped);
        qqq.bindValue(":xicStartWarped", feature.xicStartWarped);
        qqq.bindValue(":xicEndWarped", feature.xicEndWarped);
        e = QEXEC_NOARG(qqq); ree;
    }

    ok = db.commit();
    if (!ok) {
        rrr(kError);
    }

    db.close();
    return e;
}


inline CrossSampleFeatureTurbo
consolidateMultiSampleIdToSingleFeature(const QVector<CrossSampleFeatureTurbo> &multi)
{
    if (multi.size() == 1) {

        return multi[0];

    } else if (multi.size() >1) {

        CrossSampleFeatureTurbo mostIntenseFeatureWithSameId;
        QVector<double> unwarpedStartAndStopXICs;
        for (const CrossSampleFeatureTurbo &crossSampleFeatureTurbo : multi) {
            mostIntenseFeatureWithSameId
                = crossSampleFeatureTurbo.maxIntensity > mostIntenseFeatureWithSameId.maxIntensity
                ? crossSampleFeatureTurbo
                : mostIntenseFeatureWithSameId;

            unwarpedStartAndStopXICs.push_back(crossSampleFeatureTurbo.xicStart);
            unwarpedStartAndStopXICs.push_back(crossSampleFeatureTurbo.xicEnd);
        }

        std::sort(unwarpedStartAndStopXICs.begin(), unwarpedStartAndStopXICs.end());

        mostIntenseFeatureWithSameId.xicStart = unwarpedStartAndStopXICs.front();
        mostIntenseFeatureWithSameId.xicEnd = unwarpedStartAndStopXICs.back();
        mostIntenseFeatureWithSameId.xicStartWarped = unwarpedStartAndStopXICs.front();
        mostIntenseFeatureWithSameId.xicEndWarped = unwarpedStartAndStopXICs.back();

        return mostIntenseFeatureWithSameId;
    }

    return CrossSampleFeatureTurbo();
}


Err CrossSampleFeatureCollatorTurbo::consolidateDuplicateSampleIdsFoundInMasterFeature()
{
    Err e = kNoErr;

    // Should already be sorted by master feature, but let's make sure.
    std::sort(m_consolidatedCrossSampleFeatures.begin(), m_consolidatedCrossSampleFeatures.end(),
        [](const CrossSampleFeatureTurbo &left, const CrossSampleFeatureTurbo &right) {
        return (left.masterFeature < right.masterFeature);
    });

    QVector<CrossSampleFeatureTurbo> consolidatedCrossSampleFeatures;
    QHash<int, QVector<CrossSampleFeatureTurbo>> sampleIdsInMasterFeature;

    // +1 is added here so that when the last present masterFeature found is procesed it will
    // proceed to the else if conditional. Otherwise last master feature goes unprocessed.
    for (int i = 0; i <= m_consolidatedCrossSampleFeatures.back().masterFeature + 1; ++i) {

        for (int j = 0; j < static_cast<int>(m_consolidatedCrossSampleFeatures.size()); ++j) {

            const CrossSampleFeatureTurbo &currentFeature = m_consolidatedCrossSampleFeatures[j];

            if (i == currentFeature.masterFeature) {
                sampleIdsInMasterFeature[currentFeature.sampleId].push_back(currentFeature);
            } else if (i < m_consolidatedCrossSampleFeatures[j].masterFeature
                       || j == (m_consolidatedCrossSampleFeatures.size() - 1)) {
                for (auto it = sampleIdsInMasterFeature.begin();
                     it != sampleIdsInMasterFeature.end(); ++it) {
                    consolidatedCrossSampleFeatures.push_back(
                        consolidateMultiSampleIdToSingleFeature(it.value()));
                }
                sampleIdsInMasterFeature.clear();
                break;
            }
        }
    }

    m_consolidatedCrossSampleFeatures = consolidatedCrossSampleFeatures;
    return e;
}


_PMI_END