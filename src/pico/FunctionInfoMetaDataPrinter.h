#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <fstream>

struct FunctionRec
{
    uint8_t type = -1; // (deprecated) for compatability 
    uint8_t mode = -1; // (deprecated) for compatability // 71=MSn, F1=calib, 24=UVtrace
    bool accurate = false;
    bool cal_function = false;
    uint8_t function_type = -1;
    uint8_t ion_mode = -1;
    uint8_t data_type = -1;
    bool continuum = false;
    float scan_time = -1;
    float survey_intescan_time = -1;
    float interscan_time = -1;
    float survey_start_time = -1;
    float survey_end_time = -1;
    float mass_window_pm = -1;
    float survey_start_mass = -1;
    float survey_end_mass = -1;

    std::wstring filename = L"";
    int function = -1;
    int calibration = -1;

    FunctionRec(uint8_t type1, uint8_t mode1, bool accurate1, bool cal_function1, uint8_t function_type1, uint8_t ion_mode1, uint8_t data_type1, bool continuum1, float scan_time1, float survey_intescan_time1, float interscan_time1, float survey_start_time1, float survey_end_time1, float mass_window_pm1, float survey_start_mass1, float survey_end_mass1){
        type = type1; mode = mode1; function_type = function_type1; ion_mode = ion_mode1; data_type = data_type1; continuum = continuum1; accurate = accurate1; cal_function = cal_function1;
        scan_time = scan_time1; survey_intescan_time = survey_intescan_time1;  interscan_time = interscan_time1; survey_start_time = survey_start_time1; survey_end_time = survey_end_time1; mass_window_pm = mass_window_pm1; survey_start_mass = survey_start_mass1; survey_end_mass = survey_end_mass1;
    }
};

class FunctionInfoMetaDataPrinter
{
    std::vector<FunctionRec> functionInfoMetaData;

public:
    FunctionInfoMetaDataPrinter();
    void pushFunctionInfoMetaData(const FunctionRec &rec); 
    void pushCalibrationInfo(int function, bool info);
    bool writeToFile(const std::wstring &filename);
    ~FunctionInfoMetaDataPrinter();
};


bool calibrationFunctionFlag(int functionModeRaw);

