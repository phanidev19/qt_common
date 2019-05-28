/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "ScopedQSqlDatabase.h"

#include <QSqlDatabase>
#include <QString>

// common_stable
#include <QtSqlUtils.h>

_PMI_BEGIN

ScopedQSqlDatabase::ScopedQSqlDatabase(QSqlDatabase *db, const QString &sqliteFilePath,
                                       const QString &connectionName)
    : m_db(db)
    , m_fileName(sqliteFilePath)
    , m_connectionName(connectionName)
{
}

ScopedQSqlDatabase::~ScopedQSqlDatabase()
{
    if (m_db->isOpen()) {
        m_db->close();
    }
}

Err pmi::ScopedQSqlDatabase::init()
{
    Err e = kNoErr;
    e = pmi::addDatabaseAndOpen(m_connectionName, m_fileName, *m_db); ree;
    return e;
}

_PMI_END
