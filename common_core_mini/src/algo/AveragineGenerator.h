/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 * Author: Lukas Tvrdy ltvrdy@milosolutions.com
 */

#ifndef AVERAGINE_GENERATOR_H
#define AVERAGINE_GENERATOR_H

#include "pmi_common_core_mini_export.h"

#include <common_math_types.h>
#include <pmi_core_defs.h>

#include <QScopedPointer>

_PMI_BEGIN

/*!
 * @brief Using Averagine isotope distribution, provide API to generate Isotope signal
 * with desired attributes of mass and charge.
 */
class PMI_COMMON_CORE_MINI_EXPORT AveragineGenerator
{

public:
    /*
     * @brief Creates the generator, initializes the mass value to @a mass
     * Utilizes @see AveragineIsotopeTable, initialization is thus non-trivial
     */
    explicit AveragineGenerator(double mass);
    ~AveragineGenerator();

    /*!
     * @brief Set the desired mass of the signal generated by @a generateSignal
     */
    void setMass(double mass);

    /*!
     * @brief Current mass used by generator
     * @see setMass
     */
    double mass() const;

    /*!
     * @brief Generates signal based on mass set in @see setMass and @a charge parameter
     */
    point2dList generateSignal(int charge) const;

    /*!
    * @brief For given charge and current mass set in @a setMass, determines maxMz and monoisotope offset
    */
    void getMaxMzAndMonoisotopeOffset(int charge, double *maxMz, int *monoisotopeOffset);

    /*!
     * @brief For given @a mass and @a charge computes monoisotope mz
     */
    static double computeMonoisotopeMz(double mass, int charge);

private:
    Q_DISABLE_COPY(AveragineGenerator)
    class Private;
    const QScopedPointer<Private> d;
};

_PMI_END

#endif // AVERAGINE_GENERATOR_H