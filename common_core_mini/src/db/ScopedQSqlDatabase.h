/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef SCOPED_QSQLDATABASE_H
#define SCOPED_QSQLDATABASE_H

#include "pmi_common_core_mini_export.h"

#include <common_errors.h>
#include <pmi_core_defs.h>

class QSqlDatabase;
class QString;

_PMI_BEGIN

//TODO: This class is unfinished, see LT-3664

//!\brief This class implements simple RAII for QSqlDatabase open/close action
//!
//! You can use this class so that QSqlDatabase is properly closed after QSqlDatabase::open() call.
//! Typical use-cases are with ree idiom where within function any call can be return point and
//! QSqlDatabase would have to be handled and closed.
//!
class PMI_COMMON_CORE_MINI_EXPORT ScopedQSqlDatabase
{

public:
    //!\brief Constructs the scoped guard: pointer ownership of db is not transfered
    // Caller is responsible to free @a db parameter
    ScopedQSqlDatabase(QSqlDatabase *db, const QString &sqliteFilePath,
                       const QString &connectionName);

    ~ScopedQSqlDatabase();
    
    //! init() call opens the database and then database is closed in destructor if it was opened.
    Err init();

private:
    QSqlDatabase *m_db;
    QString m_fileName;
    QString m_connectionName;
};

_PMI_END

#endif // SCOPED_QSQLDATABASE_H