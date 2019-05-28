#include "RandomBilinearTileIterator.h"
#include "RandomTileIterator.h"
#include "TileManager.h"

_PMI_BEGIN


RandomBilinearTileIterator::RandomBilinearTileIterator(TileManager * manager) 
    : m_randomIterator(new RandomTileIterator(manager)), 
    m_lastValue(0.0)
{

}

RandomBilinearTileIterator::~RandomBilinearTileIterator()
{
}


void RandomBilinearTileIterator::moveTo(const QPoint &level, qreal x, qreal y)
{
    int ix = floor(x);
    int iy = floor(y);

    qreal dx = x - ix;
    qreal dy = y - iy;

    double values[4] = { 0.0 };
    double weight[4] = { 0.0 };

    // https://en.wikipedia.org/wiki/Bilinear_interpolation#Unit_Square
    m_randomIterator->moveTo(level, ix, iy); // (0,0)
    values[0] = m_randomIterator->value();
    weight[0] = (1.0 - dx) * (1.0 - dy);

    m_randomIterator->moveTo(level, ix + 1, iy); // (1, 0)
    values[1] = m_randomIterator->value();
    weight[1] = dx * (1.0 - dy);

    m_randomIterator->moveTo(level, ix, iy + 1); // (0, 1)
    values[2] = m_randomIterator->value();
    weight[2] = (1.0 - dx) * dy;

    m_randomIterator->moveTo(level, ix + 1, iy + 1); // (1, 1)
    values[3] = m_randomIterator->value();
    weight[3] = dx * dy;

    m_lastValue = 0.0;
    for (int i = 0; i < 4; ++i) {
        m_lastValue += values[i] * weight[i];
    }
}

double RandomBilinearTileIterator::value() const
{
    return m_lastValue;
}

_PMI_END

