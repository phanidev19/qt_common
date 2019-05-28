/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NON_UNIFORM_TILESTORE_SQLITE_H
#define NON_UNIFORM_TILESTORE_SQLITE_H

#include "pmi_core_defs.h"

#include "NonUniformTileStore.h"
#include "pmi_common_tiles_export.h"

#include <QRect>
#include <QSharedPointer>

class QSqlDatabase;

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT NonUniformTileStoreSqlite : public NonUniformTileStore {

public:
    NonUniformTileStoreSqlite(QSqlDatabase * db, bool ownDb = false);
    ~NonUniformTileStoreSqlite();

    bool saveTile(const NonUniformTile &t, ContentType type) override;
    const NonUniformTile loadTile(const QPoint &pos, ContentType type) override;
    bool contains(const QPoint &pos, ContentType type) override;

    // partial tiles
    bool startPartial() override;
    bool endPartial() override;
    bool savePartialTile(const NonUniformTile &t, ContentType type, quint32 writePass) override;
    virtual bool initTilePartCache() override;
    virtual bool dropTilePartCache() override;
    virtual bool defragmentTiles(NonUniformTileStore * dstStore) override;


    bool start() override;
    bool end() override;
    void clear() override;

    bool init();

    bool isSchemaValid() const;

    int count(ContentType type);

    //! \brief Clones the sqlite-based store
    //
    // New db connection is made to the same database,
    // in case it fails to be opened, nullptr is returned
    NonUniformTileStore *clone() const override;

    //! \brief if tileRect is null, than count for all tiles is provided
    quint32 pointCount(ContentType type, const QRect &tileRect = QRect());

private:
    QString contentTypeToDaoString(ContentType type) const;
    ContentType contentTypeFromDaoString(const QString& daoString) const;

    //! \brief transform tile part to memory representation close to the database memory layout 
    void serializeTilePart(const NonUniformTile &t, QVector<int> * indexes, QVector<double> * mz, QVector<double> * intensity, qint32 * pointCount);

    //! \brief transform tile part from database memory layout to tile memory layout
    void deserializePartTile(const QVector<int> &indexes, const QVector<double> &mz, const QVector<double> &intensity, qint32 pointCount, NonUniformTile * tile);

    
    QSharedPointer<QSqlDatabase> createTilePartDbConnection(const QString &dbFilePath, bool * ok);
    
    bool removePartTileDatabase();

private:
    QSqlDatabase * m_db;
    QSharedPointer<QSqlDatabase> m_partsDb;
    bool m_ownDb;
};

_PMI_END

#endif // NON_UNIFORM_TILESTORE_SQLITE_H
