/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "InsilicoGenerator.h"
#include "CsvWriter.h"
#include "pmi_common_core_mini_debug.h"
#include "pmi_common_core_mini_export.h"

// common_stable
#include <common_constants.h>
#include <common_errors.h>
#include <sqlite_utils.h>
#include <QtSqlUtils.h>

#include <QDir>
#include <QString>

#include <string>
#include <vector>

#include "PmiQtCommonConstants.h"

_PMI_BEGIN

static const QString UNKNOWN_SEQUENCE_TEMPLATE = QStringLiteral("UNKNOWN_%1");

InsilicoGenerator::InsilicoGenerator(const QString &masterFileDBPath)
    : m_masterFeaturesDBPath(masterFileDBPath)
    , m_enableMS2Matching(true)
{
}

InsilicoGenerator::~InsilicoGenerator()
{
    closeConnections();
}

Err InsilicoGenerator::generateInsilicoFile(QString *outputFilePath)
{
    Err e = kNoErr;

    closeConnections();

    // todo:sync with InsilicoPeptidesCsvFormatter
    enum EntryColumns {
        Sequence,
        UnchargedMass,
        ModificationsPositionList,
        ModificationsNameList,
        StartTime,
        EndTime,
        ChargeList,
        Comment,
        GlycanList,
        ApexTime,
        CoarseIntensity,
        DominantMz,
        IsotopeCount,
        AveragineCorr,
        ColumnCount
    };

    const QStringList header = { kSequence,
                                 kUnchargedMass,
                                 kModificationsPositionList,
                                 kModificationsNameList,
                                 kStartTime,
                                 kEndTime,
                                 kChargeList,
                                 kComment,
                                 kGlycanList,
                                 kApexTime,
                                 QStringLiteral("CoarseIntensity"),
                                 QStringLiteral("DominantMz"),
                                 QStringLiteral("IsotopeCount"),
                                 QStringLiteral("AveragineCorr") };

    QStringList emptyRow;
    emptyRow.reserve(header.size());
    std::generate_n(std::back_inserter(emptyRow), header.size(), [] { return QString(); });

    QList<QStringList> rowList = { header };

    //// Load Master Features DB
    QSqlDatabase db;
    QSqlDatabase currentFtrsDB;

    const QString masterConnectionName
        = QStringLiteral("InsilicoGenerator_master_%1")
              .arg(QFileInfo(m_masterFeaturesDBPath).completeBaseName());

    m_connectionNames.push_back(masterConnectionName);
    e = addDatabaseAndOpen(masterConnectionName, m_masterFeaturesDBPath, db);
    if (e != kNoErr) {
        warningCoreMini() << "Failed to load or create database"
                          << QString::fromStdString(pmi::convertErrToString(e));
        rrr(e);
    }

    ////Load initial features DB
    QString sql = R"(SELECT fileName, fullPath FROM FeatureFiles ORDER BY ID ASC LIMIT 1)";
    QSqlQuery qdb = makeQuery(db, true);
    e = QPREPARE(qdb, sql); ree;
    e = QEXEC_NOARG(qdb); ree;

    while (qdb.next()) {
        QString filePath = qdb.value(1).toString();

        const QString initialFtrsConnectionName = QStringLiteral("InsilicoGenerator_initial_ftrs");
        m_connectionNames.push_back(initialFtrsConnectionName);
        e = addDatabaseAndOpen(initialFtrsConnectionName, filePath, currentFtrsDB);
        if (e != kNoErr) {
            debugCoreMini() << "Failed to load or create database"
                     << QString::fromStdString(pmi::convertErrToString(e));
            rrr(e);
        }
    }

    QSqlQuery qcurrentFtrsDB = makeQuery(currentFtrsDB, true);

    //// Load all FFMatchesCross table from masterFeaturesDB
    sql = R"(SELECT 
                xicStart, 
                xicEnd, 
                mwMonoisotopic, 
                sampleID, 
                feature, 
                masterFeature, 
                corr, 
                chargeOrder, 
                MAX(maxIntensity) AS maxIntensity, 
                maxIsotopeCount, 
                rt 
            FROM FFMatchesCross 
            GROUP BY masterFeature 
            ORDER BY sampleID ASC)";

    qdb = makeQuery(db, true);
    e = QPREPARE(qdb, sql); ree;
    e = QEXEC_NOARG(qdb); ree;

    ////Begin iterating all FFMatchesCross Table
    bool ok = true;
    int currentSampleID = 0;
    while (qdb.next()) {

        double xicStart = qdb.value(0).toDouble(&ok);
        Q_ASSERT(ok);
        double xicEnd = qdb.value(1).toDouble(&ok);
        Q_ASSERT(ok);
        double mwMonoisotopic = qdb.value(2).toDouble(&ok);
        Q_ASSERT(ok);
        int sampleID = qdb.value(3).toInt(&ok);
        Q_ASSERT(ok);
        int feature = qdb.value(4).toInt(&ok);
        Q_ASSERT(ok);
        int masterFeature = qdb.value(5).toInt(&ok);
        Q_ASSERT(ok);
        double corr = qdb.value(6).toDouble(&ok);
        Q_ASSERT(ok);
        QVariant chargeOrder = qdb.value(7);
        double maxIntensity = qdb.value(8).toDouble(&ok);
        Q_ASSERT(ok);
        int maxIsotopeCount = qdb.value(9).toInt(&ok);
        Q_ASSERT(ok);
        double rt = qdb.value(10).toDouble(&ok);
        Q_ASSERT(ok);

        //// Change features table when the sampleID changes
        if (currentSampleID != sampleID) {
            currentSampleID = sampleID;

            sql = R"(SELECT fileName, fullPath FROM FeatureFiles WHERE ID = :ID)";
            QSqlQuery qdb2 = makeQuery(db, true);
            e = QPREPARE(qdb2, sql); ree;
            qdb2.bindValue(":ID", currentSampleID);
            e = QEXEC_NOARG(qdb2); ree;

            while (qdb2.next()) {
                QVariant fileName = qdb2.value(0);
                QVariant filePath = qdb2.value(1);
                const QString connectionName
                    = QStringLiteral("InsilicoGenerator_switch_sample_%1").arg(fileName.toString());
                m_connectionNames.push_back(connectionName);
                e = addDatabaseAndOpen(connectionName, filePath.toString(), currentFtrsDB);
                if (e != kNoErr) {
                    warningCoreMini() << "Failed to load or create database"
                             << QString::fromStdString(pmi::convertErrToString(e));
                    rrr(e);
                }
            }
        }

        QString peptide;
        if (m_enableMS2Matching) {
            sql = QStringLiteral(
                R"(SELECT peptide FROM msmsToFeatureMatches WHERE featureID = :featureID)");
            qcurrentFtrsDB = makeQuery(currentFtrsDB, true);
            e = QPREPARE(qcurrentFtrsDB, sql); ree;
            qcurrentFtrsDB.bindValue(":featureID", feature);
            e = QEXEC_NOARG(qcurrentFtrsDB); ree;

            peptide = qcurrentFtrsDB.next()
                ? qcurrentFtrsDB.value(0).toString()
                : UNKNOWN_SEQUENCE_TEMPLATE.arg(QString::number(masterFeature));
        } else {
            peptide = UNKNOWN_SEQUENCE_TEMPLATE.arg(QString::number(masterFeature));
        }

        QString chargeOrderString
            = chargeOrder.toString().left(chargeOrder.toString().lastIndexOf(QChar(',')));
        QStringList chargeOrderStringList = chargeOrderString.split(',');

        if (chargeOrderStringList.isEmpty()) {
            rrr(kError);
        }

        double dominantCharge = chargeOrderStringList[0].toDouble(&ok);
        Q_ASSERT(ok);

        double dominantMz = ((mwMonoisotopic + (HYDROGEN * dominantCharge)) / dominantCharge);

        // TODO: port away QString::fromStdString(std::to_string(double) to QString::number() with
        // correct settings
        const QString UNDEFINED;
        QStringList currentRow = emptyRow;
        currentRow[Sequence] = peptide;
        currentRow[UnchargedMass] = QString::fromStdString(std::to_string(mwMonoisotopic));

        currentRow[ModificationsPositionList] = UNDEFINED;
        currentRow[ModificationsNameList] = UNDEFINED;

        currentRow[StartTime] = QString::fromStdString(std::to_string(xicStart));
        currentRow[EndTime] = QString::fromStdString(std::to_string(xicEnd));
        currentRow[ChargeList] = chargeOrderString;

        currentRow[Comment] = UNDEFINED;
        currentRow[GlycanList] = UNDEFINED;
        currentRow[CoarseIntensity] = QString::fromStdString(std::to_string(maxIntensity));
        currentRow[DominantMz] = QString::fromStdString(std::to_string(dominantMz));

        currentRow[IsotopeCount] = QString::fromStdString(std::to_string(maxIsotopeCount));
        currentRow[AveragineCorr] = QString::fromStdString(std::to_string(corr));
        currentRow[ApexTime] = QString::number(rt);
        rowList.append(currentRow);
    }

    // TODO: fix writing the items to CSV row-by-row instead
    const QString filePath = m_masterFeaturesDBPath + ".csv";
    ok = CsvWriter::writeFile(filePath, rowList, "\r\n", ',');
    if (!ok) {
        rrr(kError);
    }

    *outputFilePath = filePath;

    currentFtrsDB.close();
    db.close();

    // some connections were leaking, so let's close them here
    // TODO: make single document/project class handling
    // the file resources (vendors, ftrs files, master db,..)
    closeConnections();

    return e;
}

void InsilicoGenerator::setMS2MatchingEnabled(bool on)
{
    m_enableMS2Matching = on;
}

void InsilicoGenerator::closeConnections()
{
    for (const QString &connectionName : qAsConst(m_connectionNames)) {
        QSqlDatabase db = QSqlDatabase::database(connectionName, false);
        if (db.isOpen()) {
            db.close();
        }
    }

    m_connectionNames.clear();
}

_PMI_END
