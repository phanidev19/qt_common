/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef META_INFO_DAO_H
#define META_INFO_DAO_H

#include "pmi_common_ms_export.h"

#include <common_errors.h>
#include <pmi_core_defs.h>

class QSqlDatabase;

_PMI_BEGIN

class PMI_COMMON_MS_EXPORT MetaInfoDao
{

public:
    explicit MetaInfoDao(QSqlDatabase *db);

    Err createTable();

    // insert new tag
    Err insert(const QString &key, const QString &value);
    Err update(const QString &key, const QString &value);

    Err insertOrUpdate(const QString &key, const QString &value);

    // read the tag
    Err select(const QString &key, QString *value);

private:
    QSqlDatabase *m_db;
};

_PMI_END

#endif // META_INFO_DAO_H