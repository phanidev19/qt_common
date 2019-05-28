/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef TILE_H
#define TILE_H

#include <QPoint>
#include <QVector>
#include <QHash>
#include <QDebug>


#include "pmi_core_defs.h"
#include "pmi_common_tiles_export.h"


_PMI_BEGIN

const QPoint NULL_LEVEL = QPoint(-1,-1);

class PMI_COMMON_TILES_EXPORT TileInfo {
public:
        TileInfo():m_level(NULL_LEVEL) {}; // null object
        explicit TileInfo(const QPoint &level, const QPoint &pos):m_level(level), m_pos(pos) {}
        
        QPoint level() const { return m_level; }
        QPoint pos() const { return m_pos; }

        bool isValid() const { return m_level != NULL_LEVEL; }

private:
    QPoint m_level;
    QPoint m_pos;
};

inline bool operator==(const TileInfo & lhs, const TileInfo & rhs)
{
    return (lhs.level() == rhs.level()) && 
           (lhs.pos() == rhs.pos());
}

QDebug operator<<(QDebug debug, const TileInfo &ti);


class PMI_COMMON_TILES_EXPORT Tile {

public: 
    enum SerializeFormat {RAW_BYTES, COMPRESSED_BYTES};
    
    Tile();
    
    Tile(const QPoint& level, const QPoint &pos, const QVector<double> &data);
    Tile(const TileInfo &ti, const QVector<double> &data);
    ~Tile();

    static const int WIDTH;
    static const int HEIGHT;

    // it's 0.0 for now
    static const double DEFAULT_TILE_VALUE;

    TileInfo tileInfo() const { return m_tileInfo;  }
    QVector<double> data() const { return m_data; }
    QByteArray serializedData(SerializeFormat format = Tile::RAW_BYTES) const; 

    static QVector<double> deserializedData(const QByteArray &input, SerializeFormat format = Tile::RAW_BYTES);

    bool operator==(const Tile &rhs) const;
    inline bool operator!=(const Tile &rhs) const;

    // x and y has to be valid coordinates within Tile and Tile should not be null!
    double value(int x, int y) const;

    bool isNull() const {
        return (m_tileInfo.level() == NULL_LEVEL) && m_tileInfo.pos().isNull() && m_data.isEmpty();
    }

    //! Helper function useful for debugging tile content
    //! @return Tile that has values distracted from Tile t
    Tile difference(const Tile &t) const;

    //! Helper function useful for debugging tile content
    //! @return Vector of positions of values in tile which are different in Tile t
    QVector<QPoint> differentValues(const Tile &t) const;


private:
    TileInfo m_tileInfo;
    QVector<double> m_data;
};


PMI_COMMON_TILES_EXPORT bool saveDataToFile(const Tile& tile, const QString & filename);


inline uint qHash(const TileInfo &ti, uint seed)
{
    QtPrivate::QHashCombine hash;
    seed = hash(ti.level().x(), seed);
    seed = hash(ti.level().y(), seed);
    seed = hash(ti.pos().x(), seed);
    seed = hash(ti.pos().y(), seed);
    return seed;
}

_PMI_END

#endif
