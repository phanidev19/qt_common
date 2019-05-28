#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <pmi_common_core_mini_export.h>

class PMI_COMMON_CORE_MINI_EXPORT Calibration
{
public:
    Calibration(double a, double b, double c, double ppmOffset) {
        this->m_aCoefficient = a;
        this->m_bCoefficient = b;
        this->m_cCoefficient = c;
        this->m_ppmOffset = ppmOffset;
    }

    double calibrate(double mz) {
        // Apply Coefficients
        // ax^2 + bx + c
        //double mz = (mz * (this->m_aCoefficient * this->m_aCoefficient)) + (mz * this->m_bCoefficient) + this->m_cCoefficient;
        //int val = as::value("do_recal").toInt();

        // Apply ppm offset (lockmass normally)
        //if (val > 0) {
            mz += mz * this->m_ppmOffset / 1000000;
        //}

        return mz;
    }

private:
    double m_aCoefficient;
    double m_bCoefficient;
    double m_cCoefficient;
    double m_ppmOffset;
};

#endif // CALIBRATION_H
