#ifndef TILE_DOCUMENT_H
#define TILE_DOCUMENT_H

#include "pmi_core_defs.h"
#include "pmi_common_ms_export.h"

#include <QScopedPointer>
#include <QObject>

#include "common_errors.h"

class QSqlDatabase;

_PMI_BEGIN

struct MetaInfoEntry {
    MetaInfoEntry(const QString &k, const QString &v):key(k), value(v) {}

    QString key;
    QString value;
};

class TileRange;

class TileDocumentPrivate;
class PMI_COMMON_MS_EXPORT TileDocument : public QObject {
    Q_OBJECT
public:    
    explicit TileDocument(QObject * parent = nullptr);
    ~TileDocument();

    void setTileRange(const TileRange &range);
    TileRange tileRange() const;
    
    // !\brief Imports the MS file and creates heatmap document in @param outputFileName
    //
    // @note: If you call this function in different thread than main, open the MSReader in main thread!
    void import(const QString &inputFileName, const QString &outputFileName, bool buildRipmaps);
    
    void generateRipmaps(const QString &tilesDocumentFilePath);


signals:
    void progressChanged(const QPoint &level, int totalTileCount, int processedTiles, qint64 milisecondsElapsed);

private:
    Err saveMetaInfo(QSqlDatabase *db, const QVector<MetaInfoEntry> &metaInfo);
    Err loadMetaInfo(QSqlDatabase *db, MetaInfoEntry *entry);

public:
    // meta tags
    static const QLatin1String TAG_MS_FILE_PATH;
    static const QLatin1String TAG_GIT_VERSION;

private:
    const QScopedPointer<TileDocumentPrivate> d; // d-pointer

    friend class TileDocumentTest;
};

_PMI_END

#endif // TILE_DOCUMENT_H