#ifndef TILE_STORE_SQLITE
#define TILE_STORE_SQLITE

#include "Tile.h"
#include "TileStore.h"
#include "QSqlDatabase"
#include "common_errors.h"
#include "pmi_common_tiles_export.h"

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT TileStoreSqlite : public TileStore {

public:
    TileStoreSqlite(const QString& fileName);
    virtual ~TileStoreSqlite();

    Err createTable();
    Err dropTable();

    virtual bool saveTile(const Tile &t) override;
    // pos is column, row otherwise store will not be able to find it 
    virtual Tile loadTile(const QPoint &level, const QPoint &pos) override;

    bool contains(const QPoint &level, const QPoint &pos) override;

    QSqlDatabase &db() { return m_db; }

    bool is2DLevelSchema() const;
    bool updateSchemaTo2DLevel();

    // TileStore interface
    // @return pixel space occupied with tiles for given level
    QRect boundary(const QPoint &level) override;
    QVector<QPoint> availableLevels();
    virtual bool start();
    virtual bool end();

    QString filePath() const;

private:
    Err saveToDb(const Tile &t);

    virtual void clear() override;


private:
    QSqlDatabase m_db;



};

_PMI_END

#endif
