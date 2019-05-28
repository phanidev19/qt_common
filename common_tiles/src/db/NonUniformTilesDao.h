/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NON_UNIFORM_TILES_DAO_H
#define NON_UNIFORM_TILES_DAO_H

#include "pmi_core_defs.h"

#include "common_errors.h"
#include <QBuffer>
#include "pmi_common_tiles_export.h"

#include <QVector>

class QSqlDatabase;

_PMI_BEGIN

struct PMI_COMMON_TILES_EXPORT NonUniformTilesDaoEntry {
    int posX;
    int posY;
    QString contentType;
    
    QVector<int> index;
    QVector<double> mz;
    QVector<double> intensity;
    int pointCount;
    
    QString debugText;

    static const QString CONTENT_MS1_RAW;
    static const QString CONTENT_MS1_CENTROIDED;
};

class PMI_COMMON_TILES_EXPORT NonUniformTilesDao {

public:    
    explicit NonUniformTilesDao(QSqlDatabase * db);

    Err createTable();
    Err dropTable();

    Err loadTile(const QPoint &position, const QString &contentType, NonUniformTilesDaoEntry * entry);
    Err saveTile(const NonUniformTilesDaoEntry &entry);

    // number of tiles of given type
    Err count(const QString &contentType, int * count);

    // count of points in the tiles of given content
    Err pointCount(const QString &contentType, const QRect &tileRect, quint32 *count);

    bool contains(const QPoint &position, const QString &contentType);
    bool isSchemaValid() const;

private:
    QSqlDatabase * m_db;
};


_PMI_END

#endif // NON_UNIFORM_TILES_DAO_H
