/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NONUNIFORM_TILES_PART_DAO_H
#define NONUNIFORM_TILES_PART_DAO_H

#include "pmi_core_defs.h"
#include "pmi_common_tiles_export.h"

#include "common_errors.h"

#include <QVector>

class QSqlDatabase;

_PMI_BEGIN

struct NonUniformTilesPartEntry 
{
    qulonglong nonUniformTilesPartInfoId;
    qulonglong id;  // writeOrder

    int pointCount;
    QVector<int> index;
    QVector<double> mz;
    QVector<double> intensity;

    qint32 scanCount;

    //! \brief append the right tile to the current one and returns reference
    NonUniformTilesPartEntry &append(const NonUniformTilesPartEntry &rhs);
};


class PMI_COMMON_TILES_EXPORT NonUniformTilesPartDao 
{

public:    
    explicit NonUniformTilesPartDao(QSqlDatabase * db);

    Err createTable();
    Err dropTable();

    Err saveTilePart(const NonUniformTilesPartEntry &entry);
    Err loadTileParts(qulonglong nonUniformTilesPartInfoId, QVector<NonUniformTilesPartEntry> * entries);

private:
    QSqlDatabase * m_db;
};

_PMI_END

#endif // NONUNIFORM_TILES_PART_DAO_H