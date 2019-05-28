#include "TileStoreSqlite.h"

#include "QtSqlUtils.h"
#include "Tile.h"

#include <QUuid>

_PMI_BEGIN

TileStoreSqlite::TileStoreSqlite(const QString& fileName)
{
    QString connectionString = QString("TileStoreSqlite_%1").arg(QUuid::createUuid().toString());

    m_db = QSqlDatabase::addDatabase(kQSQLITE, connectionString);
    m_db.setDatabaseName(fileName);
    if (!m_db.open()) {
        qWarning() << "Could not open db file:" << fileName;
    }
}


TileStoreSqlite::~TileStoreSqlite()
{
    m_db.close();
}

Err TileStoreSqlite::createTable()
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(&m_db, true);

    e = QEXEC_CMD(q, "CREATE TABLE IF NOT EXISTS Tiles (\
                            LevelX INT, \
                            LevelY INT, \
                            PosX INT, \
                            PosY INT, \
                            Data BLOB, \
                            PRIMARY KEY (LevelX, LevelY, PosX, PosY) \
                            )"); ree;


    return e;
}

Err pmi::TileStoreSqlite::dropTable()
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(&m_db, true);
    e = QEXEC_CMD(q, "DROP TABLE IF EXISTS Tiles;"); ree;
    return e;
}

bool TileStoreSqlite::saveTile(const Tile &t)
{
    return saveToDb(t) == kNoErr;
}

Tile TileStoreSqlite::loadTile(const QPoint &level, const QPoint &pos)
{
    QSqlQuery q = makeQuery(&m_db, true);
    Err e = QPREPARE(q, "SELECT LevelX, LevelY, PosX, PosY, Data FROM Tiles WHERE (LevelX = ?) AND (LevelY = ?) AND (PosX = ?) AND (PosY = ?);");
    q.bindValue(0, level.x());
    q.bindValue(1, level.y());
    q.bindValue(2, pos.x());
    q.bindValue(3, pos.y());

    if (!q.exec()) {
        qDebug() << "Error getting tile at:" << pos << q.lastError();
        return Tile();
    }

    if (!q.first())
    {
        //qWarning() << "No result at" << TileInfo(level, pos);
        return Tile();
    }

    Q_ASSERT(q.value(0).toInt() == level.x());
    Q_ASSERT(q.value(1).toInt() == level.y());
    Q_ASSERT(q.value(2).toInt() == pos.x());
    Q_ASSERT(q.value(3).toInt() == pos.y());

    QByteArray blob = q.value(4).toByteArray();
    QVector<double> data = Tile::deserializedData(blob, Tile::COMPRESSED_BYTES);

    return Tile(level, pos, data);
}

bool TileStoreSqlite::contains(const QPoint &level, const QPoint &pos)
{
    QSqlQuery q = makeQuery(&m_db, true);
    Err e = QPREPARE(q, "SELECT LevelX, LevelY, PosX, PosY FROM Tiles WHERE (LevelX = ?) AND (LevelY = ?) AND (PosX = ?) AND (PosY = ?);"); 
    if (e != kNoErr) {
        return false;
    }

    q.bindValue(0, level.x());
    q.bindValue(1, level.y());
    q.bindValue(2, pos.x());
    q.bindValue(3, pos.y());

    if (!q.exec()) {
        qDebug() << "Error quering tile at:" << pos << q.lastError();
        return false;
    }

    bool hasFirst = q.first();
    if (hasFirst){
        Q_ASSERT(q.value(0).toInt() == level.x());
        Q_ASSERT(q.value(1).toInt() == level.y());
        Q_ASSERT(q.value(2).toInt() == pos.x());
        Q_ASSERT(q.value(3).toInt() == pos.y());
    }

    return hasFirst;
}


bool TileStoreSqlite::is2DLevelSchema() const
{
    QSqlRecord record = m_db.record("Tiles");
    if (!record.contains("Level") &&
        record.contains("LevelX") &&
        record.contains("LevelY")) {
        return true;
    }

    return false;
}

bool TileStoreSqlite::updateSchemaTo2DLevel()
{
    // check if upgrade is needed?
    
    bool result = m_db.transaction();
    Q_ASSERT(result);

    QSqlQuery q = makeQuery(&m_db, true);
    
    Err e;
    while (true) {
        // rename old table        
        e = QEXEC_CMD(q, R"(ALTER TABLE Tiles RENAME TO tmp_Tiles;)");
        if (e != kNoErr) {
            break;
        }

        // create new table schema
        e = createTable();
        if (e != kNoErr) {
            break;
        }

        // copy data to new table with latest schema
        e = QEXEC_CMD(q, R"( INSERT INTO Tiles(LevelX, LevelY, PosX, PosY, Data) 
                                SELECT Level, Level, PosX, PosY, Data  
                                FROM tmp_Tiles; )");
        if (e != kNoErr) {
            break;
        }
        
        // drop old table
        e = QEXEC_CMD(q, R"(DROP TABLE tmp_Tiles;)");
        if (e != kNoErr) {
            break;
        }

        break;
    }
    
    if (e == kNoErr) {
        result = m_db.commit();
    } else {
        result = m_db.rollback();
    }

    e = QEXEC_CMD(q, R"(VACUUM;)");
    if (e != kNoErr) {
        qWarning() << "VACUUMING failed!";
    }


    return result;
}

QRect TileStoreSqlite::boundary(const QPoint &level)
{
    // we store tile positions in db, so fetch min,max position in every dimension
    QSqlQuery q = makeQuery(&m_db, true);
    Err e = QPREPARE(q, QString("SELECT Min(PosX) as minX, Max(PosX) as maxX, Min(PosY) as minY, Max(PosY) as maxY FROM Tiles WHERE LevelX = ? AND LevelY = ?;"));
    if (e != kNoErr) {
        return QRect();
    }

    q.bindValue(0, level.x());
    q.bindValue(1, level.y());

    if (!q.exec()) {
        qDebug() << "Error quering boundary at level:" << level << q.lastError();
        return QRect();
    }


    if (!q.first()){
        qDebug() << "Error: no result returned";
        return QRect();
    }


    if (q.value("minX").isNull()){
        qDebug() << "Error: invalid level" << level;
        return QRect();
    }

    int minX = q.value("minX").toInt();
    int maxX = q.value("maxX").toInt();
    int minY = q.value("minY").toInt();
    int maxY = q.value("maxY").toInt();



    // last tile contains Tile::WIDTH * Tile::HEIGHT values
    maxX += Tile::WIDTH;
    maxY += Tile::HEIGHT;

    return QRect(minX, minY, maxX - minX, maxY - minY);
}

QVector<QPoint> TileStoreSqlite::availableLevels()
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(&m_db, true);
    e = QEXEC_CMD(q, "SELECT DISTINCT LevelX, LevelY FROM Tiles ORDER BY LevelX, LevelY ASC;");
    if (e != kNoErr) {
        return QVector<QPoint>();
    }

    QVector<QPoint> levels;
    while (q.next()) {
        bool ok = false;
        int levelX = q.value(0).toInt(&ok); Q_ASSERT(ok);
        int levelY = q.value(1).toInt(&ok); Q_ASSERT(ok);

        levels.push_back(QPoint(levelX,levelY));
    }

    return levels;
}

Err TileStoreSqlite::saveToDb(const Tile &t)
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(&m_db, true);
    e = QPREPARE(q, "INSERT INTO Tiles(LevelX, LevelY, PosX, PosY, Data) VALUES(?,?,?,?,?);"); ree;
    
    const TileInfo &ti = t.tileInfo();
    q.bindValue(0, ti.level().x() );
    q.bindValue(1, ti.level().y());
    q.bindValue(2, ti.pos().x() );
    q.bindValue(3, ti.pos().y());

    QByteArray compressedBlob = t.serializedData(Tile::COMPRESSED_BYTES);

    q.bindValue(4, compressedBlob);
    e = QEXEC_NOARG(q);
    return e;
}


void TileStoreSqlite::clear()
{
    //NOT IMPLEMENTED! Do we want to be able to remove tiles from document?!
}

bool TileStoreSqlite::start()
{
    return m_db.transaction();
}

bool TileStoreSqlite::end()
{
    return m_db.commit();
}

QString TileStoreSqlite::filePath() const
{
    return m_db.databaseName();
}

_PMI_END
