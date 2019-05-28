/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "FinderSamplesDao.h"

// common_stable
#include <QtSqlUtils.h>

// Qt
#include <QSqlQuery>

_PMI_BEGIN

FinderSamplesDao::FinderSamplesDao(QSqlDatabase *db)
    : m_db(db)
{
}

Err FinderSamplesDao::createTable() const
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(m_db, true);

    const QString createSql
        = QString("CREATE TABLE IF NOT EXISTS FinderSamples(Id INTEGER PRIMARY KEY, "
                  "FilePath TEXT)");

    e = QEXEC_CMD(q, createSql); ree;

    return e;
}

Err FinderSamplesDao::insertSamples(const QStringList &sampleNames) const
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(m_db, true);

    e = QPREPARE(q, "INSERT INTO FinderSamples(FilePath) VALUES(:FilePath)"); ree;
    
    for (const QString &sampleName : sampleNames) {
        q.bindValue(":FilePath", sampleName);
        e = QEXEC_NOARG(q); ree;
    }

    return e;
}

Err FinderSamplesDao::samplesId(const QString &sampleFilePath, int *samplesId) const
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(m_db, true);
    e = QPREPARE(q, "SELECT Id FROM FinderSamples WHERE FilePath = :FilePath"); ree;

    q.bindValue(":FilePath", sampleFilePath);

    e = QEXEC_NOARG(q); ree;
    if (q.next()) {
        bool ok;
        *samplesId = q.value(0).toInt(&ok);
        e = ok ? kNoErr : kBadParameterError; ree;
    } else {
        *samplesId = -1;
    }

    return e;
}

Err FinderSamplesDao::uniqueIds(QList<int> *ids) const
{
    Err e = kNoErr;
    ids->clear();
    QSqlQuery q = makeQuery(m_db, true);
    e = QPREPARE(q, "SELECT Distinct(Id) as Id FROM FinderSamples"); ree;

    e = QEXEC_NOARG(q); ree;

    QList<int> result;
    while (q.next()) {
        result.push_back(q.value(0).toInt());
    }

    *ids = result;

    return e;
}


_PMI_END


