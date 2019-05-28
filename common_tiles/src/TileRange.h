#ifndef TILERANGE_H
#define TILERANGE_H

#include "pmi_core_defs.h"
#include "qglobal.h"
#include "pmi_common_tiles_export.h"

#include "common_errors.h"

#include <QSize>
#include <QDebug>

//TODO: beautify!

class QSqlDatabase;
class QJsonObject;

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT TileRange {
public:
    TileRange()
        :   m_minX(0), 
            m_stepX(0),
            m_maxX(0),
            m_minY(0),
            m_stepY(0),
            m_maxY(0),
            m_minIntensity(0.0),
            m_maxIntensity(0.0),
            m_size(0,0)
        {}

    // verifies ranges
    bool isValid() const;
    

    // inits X range, size is computed according input
    void initXRange(qreal minX, qreal maxX, qreal stepX);
    // inits Y range, size is computed according input
    void initYRange(int minY, int maxY, int stepY);

    void initXRangeBySize(qreal minX, qreal maxX, int sizeX);
    void initYRangeBySize(int minY, int maxY, int sizeY);

    static Err createTable(QSqlDatabase * db);
    static Err dropTable(QSqlDatabase * db);
    static Err saveRange(const TileRange &range, const QSqlDatabase * db);
    static Err loadRange(const QSqlDatabase * db, TileRange * range);

    int tileCountX() const;
    int tileCountY() const;

    bool operator==(const TileRange& rhs) const;

    int levelWidth(const QPoint &level) const;
    int levelHeigth(const QPoint &level) const;

    //TODO: refactor to levelStepX(int level);
    qreal levelStepX(const QPoint &level) const;
    //TODO: refactor to levelStepX(int level);
    qreal levelStepY(const QPoint &level) const;

    QSize levelSize(const QPoint &level) const;

    // uses min, max, step to compute height
    void initHeight();

    bool isNull() const;

    void setMinX(qreal val) { m_minX = val; }
    qreal minX() const { return m_minX; }

    qreal stepX() const { return m_stepX; }
    void setStepX(qreal val) { m_stepX = val; }

    qreal maxX() const { return m_maxX; }
    void setMaxX(qreal val) { m_maxX = val; }

    int minY() const { return m_minY; }
    void setMinY(int val) { m_minY = val; }

    int stepY() const { return m_stepY; }
    void setStepY(int val) { m_stepY = val; }

    int maxY() const { return m_maxY; }
    void setMaxY(int val) { m_maxY = val; }

    QSize size() const { return m_size; }
    void setSize(QSize val) { m_size = val; }

    qreal minIntensity() const { return m_minIntensity; }
    void setMinIntensity(qreal intensity) { m_minIntensity = intensity; }

    qreal maxIntensity() const { return m_maxIntensity; }
    void setMaxIntensity(qreal intensity) { m_maxIntensity = intensity; }

    bool loadFromFile(const QString &filePath);

    void write(QJsonObject &json) const;
    void read(const QJsonObject &json);

    static double computeStep(double min, double max, int size);

    //! \brief Provides the range expressed in rectangular region
    //! The border values are inclusive, QRectF::right is still valid in the range and QRectF::bottom
    //! The unit here is mz, scan index
    QRectF area() const;

private:
    int downScale(int level, int value) const;

    int computeSize(qreal max, qreal min, qreal step) const;

private:
    // TODO: d-pointer
    qreal m_minX;
    qreal m_stepX;
    qreal m_maxX;
    int m_minY;
    int m_stepY;
    int m_maxY;

    qreal m_minIntensity;
    qreal m_maxIntensity;

    QSize m_size;
};

PMI_COMMON_TILES_EXPORT QDebug operator<<(QDebug debug, const TileRange &ti);

_PMI_END

#endif
