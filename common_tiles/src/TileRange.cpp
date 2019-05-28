#include "TileRange.h"
#include "QtSqlUtils.h"
#include <QSqlDatabase>

#include <QJsonObject>

#include "Tile.h"

_PMI_BEGIN

bool TileRange::isValid() const
{
    // size and step has to match
    bool validXrange = m_size.width() == computeSize(m_maxX, m_minX, m_stepX);
    bool validYrange = m_size.height() == computeSize(m_maxY, m_minY, m_stepY);
    return (validXrange && validYrange);
}


void TileRange::initXRange(qreal minX, qreal maxX, qreal stepX)
{
    Q_ASSERT(maxX > minX);
    Q_ASSERT(stepX != 0.0);
    setMinX(minX);
    setMaxX(maxX);
    setStepX(stepX);

    int size = computeSize(m_maxX, m_minX, m_stepX);
    m_size.setWidth(size);
}


void TileRange::initYRange(int minY, int maxY, int stepY)
{
    Q_ASSERT(stepY != 0);
    
    setMinY(minY);
    setMaxY(maxY);
    setStepY(stepY);

    int size = computeSize(m_maxY, m_minY, m_stepY);
    m_size.setHeight(size);
}


void TileRange::initXRangeBySize(qreal minX, qreal maxX, int sizeX)
{
    Q_ASSERT(minX < maxX);
    Q_ASSERT(sizeX > 0);
    
    setMinX(minX);
    setMaxX(maxX);
    m_size.setWidth(sizeX);

    setStepX(TileRange::computeStep(minX, maxX, sizeX));
}


void TileRange::initYRangeBySize(int minY, int maxY, int sizeY)
{
    Q_ASSERT(minY < maxY);
    Q_ASSERT(sizeY > 0);
    
    setMinY(minY);
    setMaxY(maxY);
    m_size.setHeight(sizeY);

    (sizeY == 1) ? setStepY(0) : setStepY((maxY - minY) / (sizeY - 1));
}

Err TileRange::createTable(QSqlDatabase * db)
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(db, true);

    e = QEXEC_CMD(q, "CREATE TABLE IF NOT EXISTS TilesRange (\
                             MinX REAL, \
                             MaxX REAL, \
                             StepX REAL, \
                             SizeX INT, \
                             MinY INT, \
                             MaxY INT, \
                             StepY INT, \
                             SizeY INT, \
                             MinIntensity REAL, \
                             MaxIntensity REAL\
                             )"); ree;
    return e;
}

Err TileRange::dropTable(QSqlDatabase * db)
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(db, true);
    e = QEXEC_CMD(q, "DROP TABLE IF EXISTS TilesRange;"); ree;
    return e;
}

Err TileRange::saveRange(const TileRange &range,const QSqlDatabase * db)
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(db, true);
    e = QPREPARE(q, "INSERT INTO TilesRange(MinX, MaxX, StepX, SizeX, MinY, MaxY, StepY, SizeY, MinIntensity, MaxIntensity) \
        VALUES(:MinX, :MaxX, :StepX, :SizeX, :MinY, :MaxY, :StepY, :SizeY, :MinIntensity, :MaxIntensity);"); ree;

    q.bindValue(":MinX", range.minX());
    q.bindValue(":MaxX", range.maxX());
    q.bindValue(":StepX", range.stepX());
    q.bindValue(":SizeX", range.size().width());
    
    q.bindValue(":MinY", range.minY());
    q.bindValue(":MaxY", range.maxY());
    q.bindValue(":StepY", range.stepY());
    q.bindValue(":SizeY", range.size().height());

    q.bindValue(":MinIntensity", range.minIntensity());
    q.bindValue(":MaxIntensity", range.maxIntensity());

    e = QEXEC_NOARG(q);
    return e;
}

Err TileRange::loadRange(const QSqlDatabase * db, TileRange * range)
{
    QSqlQuery q = makeQuery(db, true);
    Err e = QPREPARE(q, "SELECT MinX, MaxX, StepX, SizeX, MinY, MaxY, StepY, SizeY, MinIntensity, MaxIntensity FROM TilesRange LIMIT 1;");

    if (!q.exec()) {
        qDebug() << "Error getting TileRange from TilesRange" << q.lastError();
        return e;
    }

    q.first();
    
    range->setMinX(q.value(0).toDouble());
    range->setMaxX(q.value(1).toDouble());
    range->setStepX(q.value(2).toDouble());
    int width = q.value(3).toInt();

    range->setMinY(q.value(4).toInt());
    range->setMaxY(q.value(5).toInt());
    range->setStepY(q.value(6).toInt());
    int height = q.value(7).toInt();
    range->setSize(QSize(width, height)); // TODO: this is redundant information, can be computed or reused as checksum

    range->setMinIntensity(q.value(8).toDouble());
    range->setMaxIntensity(q.value(9).toDouble());

    Q_ASSERT(range->isValid());

    return e;
}


int TileRange::levelWidth(const QPoint &level) const
{
    return downScale(level.x(), size().width());
}

int TileRange::levelHeigth(const QPoint &level) const
{
    return downScale(level.y(), size().height());
}


qreal TileRange::levelStepX(const QPoint &level) const
{
    return m_stepX * pow(2, level.x() - 1);
}


qreal TileRange::levelStepY(const QPoint &level) const
{
    return m_stepY * pow(2, level.y() - 1);
}

QSize TileRange::levelSize(const QPoint &level) const
{
    return QSize(
        downScale(level.x(), size().width()),
        downScale(level.y(), size().height())
                );
}

void TileRange::initHeight()
{
    int height = ((maxY() - minY()) / stepY()) + 1;
    size().setHeight(height);
}

bool TileRange::isNull() const
{
    const bool xIntervalIsNull = 
            qFuzzyCompare(m_minX, m_maxX) &&
            qFuzzyCompare(m_maxX, m_stepX) &&
            qFuzzyCompare(m_stepX, 0.0);

    const bool yIntervalIsNull = (m_minY == m_maxY == m_stepY == 0);

    return xIntervalIsNull && yIntervalIsNull && m_size.isNull();
}

bool TileRange::loadFromFile(const QString &filePath)
{
    QFile inFile(filePath);
    if (!inFile.open(QIODevice::ReadOnly)) {
        qWarning("Couldn't open preset file.");
        return false;
    }

    QByteArray jsonData = inFile.readAll();

    QJsonDocument loadDoc(QJsonDocument::fromJson(jsonData));
    read(loadDoc.object());

    return true;
}

void TileRange::read(const QJsonObject &json)
{
    setMinX(json["MinX"].toDouble());
    setMaxX(json["MaxX"].toDouble());
    setStepX(json["StepX"].toDouble());
    int width = json["SizeX"].toInt();

    setMinY(json["MinY"].toInt());
    setMaxY(json["MaxY"].toInt());
    setStepY(json["StepY"].toInt());
    int height = json["SizeY"].toInt();

    setSize(QSize(width, height));

    setMinIntensity(json["MinIntensity"].toDouble());
    setMaxIntensity(json["MaxIntensity"].toDouble());
}

double TileRange::computeStep(double min, double max, int size)
{
    return (size == 1) ? 0.0 : (max - min) / (size - 1);
}

QRectF TileRange::area() const
{
    QRectF result;
    result.setLeft(minX());
    result.setRight(maxX());
    
    result.setTop(minY());
    result.setBottom(maxY());
    
    return result;
}

void pmi::TileRange::write(QJsonObject &json) const
{
    json["MinX"] = minX();
    json["MaxX"] = maxX();
    json["StepX"] = stepX();
    json["SizeX"] = size().width();

    json["MinY"] = minY();
    json["MaxY"] = maxY();
    json["StepY"] = stepY();
    json["SizeY"] = size().height();

    json["MinIntensity"] = minIntensity();
    json["MaxIntensity"] = maxIntensity();
}

int TileRange::downScale(int level, int value) const
{
    return qCeil(value / pow(2, level - 1));
}


int TileRange::computeSize(qreal max, qreal min, qreal step) const
{
    return qCeil((max - min) / step) + 1;
}

int TileRange::tileCountX() const
{
    return qCeil(qreal(size().width()) / Tile::WIDTH);
}

int TileRange::tileCountY() const
{
    return qCeil(qreal(size().height()) / Tile::HEIGHT);
}

bool TileRange::operator==(const TileRange& rhs) const
{
    return minX() == rhs.minX() &&
        stepX() == rhs.stepX() &&
        maxX() == rhs.maxX() &&
        minY() == rhs.minY() &&
        stepY() == rhs.stepY() &&
        maxY() == rhs.maxY() &&
        size() == rhs.size();
}

QDebug operator<<(QDebug debug, const TileRange &ti)
{
    debug << "TileRange (" << ti.size() << ti.minX() << ti.maxX() << ti.stepX();
    debug << ti.minY() << ti.maxY() << ti.stepY() << ")";
    return debug.space();
}


_PMI_END


