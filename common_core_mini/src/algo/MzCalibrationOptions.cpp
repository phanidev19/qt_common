#include "MzCalibrationOptions.h"

#include <QList>
#include <iostream>

_PMI_BEGIN

MzCalibrationOptions::MzCalibrationOptions()
{
    clear();
}

void MzCalibrationOptions::clear()
{
    lock_mass_list.clear();
    lock_mass_ppm_tolerance = "100";
    lock_mass_moving_average_width = 20;
    lock_mass_moving_median_width = 20;
    lock_mass_debug_output = 1;
    lock_mass_use_ppm_for_one_point = 0;
    lock_mass_desired_time_tick_count = -1; //if -1, it will choose all time points.
    lock_mass_scan_filter.clear();
}

QString MzCalibrationOptions::ppmTolerance() const
{
    return QString(lock_mass_ppm_tolerance.c_str());
}

QList<double> MzCalibrationOptions::lockMassList() const
{
    QList<double> massList;

    QString lockMassStr = QString(lock_mass_list.c_str());
    if (lockMassStr.contains("/")) {
        QStringList list = lockMassStr.split("/", QString::KeepEmptyParts);  //"123.123 / abc" "/ empty"
        lockMassStr = list.front();
    }
    QStringList massListStr = lockMassStr.split(",", QString::SkipEmptyParts);
    foreach(QString str, massListStr) {
        bool ok = false;
        double mass = str.toDouble(&ok);
        if (!ok || mass <= 0) {
            std::cerr << "mass of " << str.toStdString() << " not proper; ignored." << std::endl;
            massList.clear();
            break;
        }
        massList.push_back(mass);
    }
    return massList;
}


void MzCalibrationOptions::setOptions(const std::string & xlock_mass_list, const std::string & xppmTolerance)
{
    lock_mass_list = xlock_mass_list;
    lock_mass_ppm_tolerance = xppmTolerance;
}

_PMI_END
