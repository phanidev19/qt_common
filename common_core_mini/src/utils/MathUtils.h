/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Dmitry Baryshev <dbaryshev@milosolutions.com>
 */

#ifndef MATHUTILS_H
#define MATHUTILS_H

#include <pmi_common_core_mini_export.h>
#include <pmi_core_defs.h>

#include <QVector2D>

_PMI_BEGIN

class PMI_COMMON_CORE_MINI_EXPORT MathUtils
{
public:
    /*!
     *  Calculate a dot product of @arg v1 and @arg v2
     *
     *  MSVC 2015 32-bit and 64-bit versions produce different results of the following
     *  construction:
     *  @code
     *  QVector2D v1(8.10837e-09, -1);
     *  QVector2D v2(4.45843e-07, -1);
     *
     *  // QVector2D::dotProduct() returns float
     *  const double dotValue = QVector2D::dotProduct(v1,v2);
     *  @endcode
     *
     *  with MSVC 32-bit dotValue == "1.00000000000000355271"
     *  with MSVC 64-bit dotValue == "1.00000000000000000000"
     *
     *  1.00000000000000355271 - 1.00000000000000000000 > std::numeric_limits<double>::epsilon()
     *  on x86, so the values are different.
     *
     *  Most probably Qt for Windows is compiled with /fp:fast, that's why it produces inaccurate
     *  results in 32-bit mode. As a temporary workaround, we calculate dot products by ourselves.
     *
     *  For some reason, if the function is not inline, it still produces inaccurate results.
     *  Inline helps the compiler to convert the whole function call into a direct calculation which
     *  works fine and accurately. Yes, it's still not guaranteed that the compiler inlines it,
     *  so there is a small chance that the issue raises again with a different MSVC version
     *  or flags.
     */
    static Q_ALWAYS_INLINE float dotProduct(const QVector2D &v1, const QVector2D &v2)
    {
        return v1.x()*v2.x() + v1.y()*v2.y();
    }

    /*!
    * \brief lerp linear interpolation between a and b with parameter alpha
    * \param a any real value
    * \param b any real value
    * \param alpha between 0 and 1, inclusive. alpha of 0 will return value a and
    * alpha of 1 returns value b. 
    *
    * \note lerp is a contraction of "linear interpolation" https://en.wikipedia.org/wiki/Linear_interpolation
    */
    static inline double lerp(double a, double b, double alpha)
    {
        Q_ASSERT(alpha >= 0.0 && alpha <= 1.0);
        return (1 - alpha) * a + alpha * b;
    }

};

_PMI_END

#endif // MATHUTILS_H
