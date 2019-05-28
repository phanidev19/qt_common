#include "TileDocument.h"

#include <QSqlDatabase>

#include "TileRange.h"

#include "MSReader.h"
#include "db\ScanInfoDao.h"
#include "TileStoreSqlite.h"
#include "TileBuilder.h"

#include "MetaInfoDao.h"

#include <config-pmi_qt_common.h>

_PMI_BEGIN

const QLatin1String TileDocument::TAG_MS_FILE_PATH = QLatin1String("MSFilePath");
const QLatin1String TileDocument::TAG_GIT_VERSION = QLatin1String("GitVersion");

class TileDocumentPrivate {
public:
    TileRange tileRange;
};


TileDocument::TileDocument(QObject * parent) 
    :QObject(parent), 
     d(new TileDocumentPrivate())
{
}


TileDocument::~TileDocument()
{
}


void TileDocument::setTileRange(const TileRange &range)
{
    d->tileRange = range;
}

TileRange TileDocument::tileRange() const
{
    return d->tileRange;
}

void TileDocument::import(const QString &inputFileName, const QString &outputFileName, bool buildRipmaps)
{
    // TODO: refactor TileDocument into proper document which manages db connection
    TileStoreSqlite tileStore(outputFileName);
    if (tileStore.createTable() != kNoErr) {
        qWarning() << "Failed to create DB Scheme for the document";
        return;
    }

    QSqlDatabase &db = tileStore.db();
    
    // Save
    if (TileRange::createTable(&db) != kNoErr) {
        qWarning() << "Failed to create DB Scheme for the document";
        return;
    }

    if (TileRange::saveRange(d->tileRange, &db) != kNoErr) {
        qWarning() << "Failed to save TileRange info";
        return;
    }

    MSReader *reader = MSReader::Instance();
    if (!reader->isOpen() || (reader->getFilename() != inputFileName)) {
        Err e = reader->openFile(inputFileName);
        if (e != kNoErr) {
            qWarning() << "Failed to input MS file, error:" << e;
            return;
        }
    }

    QList<msreader::ScanInfoWrapper> lockmassList;
    reader->getScanInfoListAtLevel(1, &lockmassList);

    if (ScanInfoDao::createTable(&db) != kNoErr) {
        qWarning() << "Failed to create DB Scheme for the document";
        return;
    }

    if (ScanInfoDao::import(lockmassList, &db) != kNoErr)
    {
        qWarning() << "Failed to import scan time information to ScanInfo table";
        return;
    }


    TileBuilder ir(d->tileRange);

    QObject::connect(&ir, &TileBuilder::progressChanged, this, &TileDocument::progressChanged);

    ir.buildLevel1Tiles(&tileStore, reader);
    reader->closeFile();
    
    if (buildRipmaps) {
        ir.buildRipTilePyramid(QPoint(8, 8), &tileStore);
    } else {
        ir.buildTilePyramid(8, &tileStore);
    }

    // store meta information
    const MetaInfoEntry msFilePath = { TAG_MS_FILE_PATH, inputFileName };
    const MetaInfoEntry version = { TAG_GIT_VERSION, PMI_QT_COMMON_VERSION_STRING " " PMI_QT_COMMON_GIT_VERSION };

    saveMetaInfo(&tileStore.db(), { msFilePath, version });

}

void TileDocument::generateRipmaps(const QString &tilesDocumentFilePath)
{
    TileStoreSqlite tileStore(tilesDocumentFilePath);

    TileBuilder ir(d->tileRange);

    QObject::connect(&ir, &TileBuilder::progressChanged, this, &TileDocument::progressChanged);

    ir.buildRipTilePyramid(QPoint(8,8), &tileStore);
}

Err TileDocument::saveMetaInfo(QSqlDatabase *db, const QVector<MetaInfoEntry> &metaInfo)
{
    MetaInfoDao dao(db);
    Err e = dao.createTable(); ree;

    for (const MetaInfoEntry &item : metaInfo) {
        e = dao.insertOrUpdate(item.key, item.value); ree;
    }
    
    return e;
}

Err TileDocument::loadMetaInfo(QSqlDatabase *db, MetaInfoEntry *entry)
{
    MetaInfoDao dao(db);
    Err e = dao.select(entry->key, &entry->value); ree;
    return e;
}


_PMI_END
