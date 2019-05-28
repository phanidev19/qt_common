/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#ifndef NONUNIFORM_TILES_METAINFO_DAO_H
#define NONUNIFORM_TILES_METAINFO_DAO_H

#include "pmi_core_defs.h"

#include "common_errors.h"
#include "pmi_common_tiles_export.h"

#include <QPoint>

class QSqlDatabase;

class NonUniformTile;

_PMI_BEGIN

struct NonUniformTilesMetaInfoEntry {
    qulonglong id;
    QPoint position;
    QString contentType;
};


class PMI_COMMON_TILES_EXPORT NonUniformTilesMetaInfoDao {

public:    
    explicit NonUniformTilesMetaInfoDao(QSqlDatabase * db);
    
    Err createTable();
    Err dropTable();

    Err saveTileInfo(const QPoint &position, const QString &contentType);
    Err tileInfoId(const QPoint &position, const QString &contentType, qulonglong * tileId) const;

    Err allTileInfo(QVector<NonUniformTilesMetaInfoEntry> * all);

private:
    QSqlDatabase * m_db;

};

_PMI_END

#endif // NONUNIFORM_TILES_METAINFO_DAO_H