#ifndef WATERS_CALIBRATION_H
#define WATERS_CALIBRATION_H

#include <stdlib.h> 
#include <vector>
#include <map>
#include "WatersMetaReading.h"
#include "PicoUtil.h"

class Sqlite;

_PICO_BEGIN

class WatersCalibration
{
public:
    enum CoefficentsType {
        CoefficentsType_T0,
        CoefficentsType_T1,
        CoefficentsType_None
    };

    struct Coefficents {
        CoefficentsType type;
        std::vector<double> coeffients;
        Coefficents() {
            type = CoefficentsType_None; coeffients = std::vector<double>();
        }
        Coefficents(CoefficentsType type1, std::vector<double> &coeff1) {
            type = type1; coeffients = coeff1;
        }
        Err readFunctionCoefficientsFromSQLite(Sqlite* db, int function_number, bool cal_mod);
        // parse calibration line
        bool parseCalibrationLine(const std::string & text);
    };

    explicit WatersCalibration();

    // destructor:
    ~WatersCalibration();

    // read calibration info from given raw directory path
    Err readFromPath(const std::wstring & raw_dir);

    // get cal function coefficients for a given function
    Coefficents coefficients(int functionNumber);

    // get cal modification coefficients for a given function
    Coefficents calibrationModificationCoefficients(int functionNumber);

    // append calibration information to sqlite database
    bool addToSqlite(bool isAccurate, Sqlite * db);

    // read calibration information from sqlite database
    bool readFromSqlite(Sqlite * db);

    // check coefficients
    void checkCoefficients(std::vector<double> &coeffients);

    // get max scan id
    std::vector<int> getMaxScanId();

private:
    int m_max_function; // max_function
    std::map<int, Coefficents> m_functionCalibration;  //Cal Function
    std::map<int, Coefficents> m_functionCalibrationModification; //Cal Modification
    std::vector<std::string> m_originalLines;
    std::vector<int> m_max_scan_id; //helper function, map scan to function

    // read calibration information from sqlite database
    bool readCoefficientsFromSqlite(Sqlite * db);

    // read calibration modification from sqlite database
    bool readCalibrationModificationFromSqlite(Sqlite * db);

};

_PICO_END

#endif
