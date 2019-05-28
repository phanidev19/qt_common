/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "MetaInfoDao.h"
#include <QSqlQuery>

#include <QtSqlUtils.h>

const QLatin1String TABLE_NAME = QLatin1String("MetaInfo");

_PMI_BEGIN

MetaInfoDao::MetaInfoDao(QSqlDatabase *db)
    : m_db(db)
{
}

Err MetaInfoDao::createTable()
{
    QSqlQuery q = makeQuery(m_db, true);

    const QString createSql
        = QString::fromLatin1("CREATE TABLE IF NOT EXISTS %1 (Key TEXT PRIMARY KEY, Value TEXT)")
              .arg(TABLE_NAME);

    return QEXEC_CMD(q, createSql);
}

Err MetaInfoDao::insert(const QString &key, const QString &value)
{
    QSqlQuery q = makeQuery(m_db, true);

    // Note: prepare is done once and re-used
    const QString insertSql
        = QString::fromLatin1("INSERT INTO %1(Key,Value) VALUES(?,?)").arg(TABLE_NAME);
    Err e = QPREPARE(q, insertSql); ree;

    q.bindValue(0, key);
    q.bindValue(1, value);

    return QEXEC_NOARG(q);
}

Err MetaInfoDao::update(const QString &key, const QString &value)
{
    QSqlQuery q = makeQuery(m_db, true);
    const QString updateSql
        = QString::fromLatin1("UPDATE %1 SET Value = ? WHERE Key = ?").arg(TABLE_NAME);

    Err e = QPREPARE(q, updateSql); ree;
    q.bindValue(0, value);
    q.bindValue(1, key);

    return QEXEC_NOARG(q);
}

Err MetaInfoDao::insertOrUpdate(const QString &key, const QString &value)
{
    QString hasValue;
    Err e = select(key, &hasValue); ree;
    if (hasValue.isNull()) {
        e = insert(key, value); ree;
    } else {
        e = update(key, value); ree;
    }

    return e;
}

Err MetaInfoDao::select(const QString &key, QString *value)
{
    QSqlQuery q = makeQuery(m_db, true);
    const QString selectSql
        = QString::fromLatin1("SELECT Value FROM %1 WHERE Key = ?").arg(TABLE_NAME);

    Err e = QPREPARE(q, selectSql); ree;
    q.bindValue(0, key);
    e = QEXEC_NOARG(q); ree;

    if (q.next()) {
        *value = q.value(0).toString();
    } else {
        *value = QString::null;
    }

    return e;
}

_PMI_END
