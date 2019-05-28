#ifndef __MZ_CALIBRATIONOPTIONS_H__
#define __MZ_CALIBRATIONOPTIONS_H__

#include <pmi_core_defs.h>
#include <pmi_common_core_mini_export.h>
#include <QString>

_PMI_BEGIN

struct PMI_COMMON_CORE_MINI_EXPORT MzCalibrationOptions
{
    std::string lock_mass_list;
    std::string lock_mass_ppm_tolerance;
    std::string lock_mass_scan_filter; ///used to isolate calibrate scans; back up just in case we can't figure it out
    int lock_mass_moving_average_width;
    int lock_mass_moving_median_width;
    int lock_mass_debug_output;
    int lock_mass_use_ppm_for_one_point;
    int lock_mass_desired_time_tick_count;

    MzCalibrationOptions();

    void clear();

    QString ppmTolerance() const;

    QList<double> lockMassList() const;

    void setOptions(const std::string & lock_mass_list, const std::string & ppmTolerance);
};

_PMI_END

#endif
