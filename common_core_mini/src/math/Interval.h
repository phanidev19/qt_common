/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Lukas Tvrdy ltvrdy@milosolutions.com
 */

#ifndef PMI_INTERVAL_H
#define PMI_INTERVAL_H

#include "pmi_core_defs.h"

#include "pmi_common_core_mini_export.h"

#include <QDebug>

_PMI_BEGIN

/**
* @brief Generic class for interval that has start and end 
*        
* The design of the class is inspired by QPoint/QPointF
* 
* A interval is specified by a start coordinate and an end coordinate which
* can be accessed using the start() and end() functions. The isNull()
* function returns \c true if both start and end are set to 0. The
* coordinates can be set (or altered) using the setStart() and setEnd()
* functions, or alternatively the rstart() and rend() functions which
* return references to the coordinates (allowing direct
* manipulation).
*  
*/
template <class T>
class Interval
{

public:
    Q_DECL_CONSTEXPR Interval()
        : m_start(T())
        , m_end(T())
    {
    }

    Q_DECL_CONSTEXPR Interval(T start, T end)
        : m_start(start)
        , m_end(end)
    {
        Q_ASSERT(m_start <= m_end);
    }

    Q_DECL_CONSTEXPR bool isNull() const { return m_start == T() && m_end == T(); }

    Q_DECL_CONSTEXPR T start() const { return m_start; }

    Q_DECL_CONSTEXPR T end() const { return m_end; }

    Q_DECL_RELAXED_CONSTEXPR void setStart(T start) { m_start = start; }

    Q_DECL_RELAXED_CONSTEXPR void setEnd(T end) { m_end = end; }

    /*
    * @brief Returns reference to the start item of the interval
    */
    Q_DECL_RELAXED_CONSTEXPR T &rstart() { return m_start; }

    /*
    * @brief Returns reference to the end item of the interval
    */
    Q_DECL_RELAXED_CONSTEXPR T &rend() { return m_end; }

    friend Q_DECL_CONSTEXPR bool operator==(const Interval &i1, const Interval &i2)
    {
        return i1.m_start == i2.m_start && i1.m_end == i2.m_end;
    }

    friend Q_DECL_CONSTEXPR bool operator!=(const Interval &i, const Interval &i2)
    {
        return i.m_start != i2.m_start || i.m_end != i2.m_end;
    }

    /*
     * @brief Returns true if the given interval \a other is inside the interval otherwise returns
     * false
     *
     * It's assumed for now that the interval is closed, that the start and end belongs to the
     * interval
     */
    bool Q_DECL_CONSTEXPR contains(const Interval<T> &other) const
    {
        T s2 = other.start();
        T s1 = start();

        T e2 = other.end();
        T e1 = end();

        if (s2 < s1 || e2 > e1) {
            return false;
        }

        return true;
    }

    /*
     * @brief Returns true if the given coordinate \a item is inside the interval otherwise returns
     * false
     *
     * It's assumed for now that the interval is closed, i.e. the start and end values belong to the
     * interval.
     *
     * E.g. Interval<int> i(5,7), 5 will return true, 4 will return false
     */
    bool Q_DECL_CONSTEXPR contains(const T &item) const
    {
        if (item < start() || item > end()) {
            return false;
        }

        return true;
    }

    Interval<T> intersected(const Interval &other) const {
        Interval<T> result;
        result.rstart() = std::max(other.start(), start());
        result.rend() = std::min(other.end(), end());
        return result;
    }

private:
    T m_start;
    T m_end;
};

template <class T>
QDebug operator<<(QDebug dbg, const Interval<T> &i)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "Interval" << '(';
    dbg << i.start() << "," << i.end();
    dbg << ')';
    return dbg;
}

_PMI_END

#endif // PMI_INTERVAL_H