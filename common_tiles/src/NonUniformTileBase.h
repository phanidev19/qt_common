/*
* Copyright (C) 2016-2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NON_UNIFORM_TILE_BASE_H
#define NON_UNIFORM_TILE_BASE_H

#include "pmi_core_defs.h"

#include <QVector>
#include "common_math_types.h"
#include "pmi_common_tiles_export.h"

_PMI_BEGIN

template <class T>
class NonUniformTileBase
{

public:
    void setPosition(const QPoint &pos) { m_pos = pos; }
    QPoint position() const { return m_pos; }

    void setData(const QVector<T> &data) { m_data = data; }
    QVector<T> data() const { return m_data; }

    // return the complete mz content in the tile for given tileY
    // tileY value is computed from scan index and converted to local coordinate of the tile
    // @see NonUniformTileRange::tileOffset
    const T &value(int tileOffset) const
    {
        Q_ASSERT_X(!isNull(), Q_FUNC_INFO, "NonUniformTile is null, value is requested");
        if (m_data.size() == 0) {
            return defaultTileValue();
        }

        Q_ASSERT_X(hasValue(tileOffset), Q_FUNC_INFO, "outside range");
        return m_data.at(tileOffset);
    }


    void setValue(int tileOffset, const T &t) {
        Q_ASSERT_X(hasValue(tileOffset), Q_FUNC_INFO, "outside range");
        m_data[tileOffset] = t;
    }

    bool hasValue(int tileOffset) const {
        return (tileOffset >= 0 && tileOffset < m_data.size());
    }

    // todo: some move semantics could be used here, swap(), so far we copy data here
    void append(const T &data) { m_data.append(data); }

    bool isNull() const { return m_pos.isNull() && m_data.isEmpty(); }

    bool isEmpty() const { return m_data.isEmpty(); }

    static const T &defaultTileValue()
    {
        static const T t = T();
        return t;
    }

    bool operator==(const NonUniformTileBase &rhs) const
    {
        return m_data == rhs.m_data && m_pos == rhs.m_pos;
    }

    void reserve(int size) { m_data.reserve(size); }

    int pointCount() const {
        int sum = 0;
        for (const T& item: qAsConst(m_data)) {
            sum += item.size();
        }
        return sum;
    }

private:
    QPoint m_pos;
    QVector<T> m_data;
};

_PMI_END

#endif // NON_UNIFORM_TILE_H
