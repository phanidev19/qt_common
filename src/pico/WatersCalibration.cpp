#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include "Sqlite.h"
#include "Centroid.h"
#include "PicoUtil.h"
#include "WatersCalibration.h"

using namespace pico;
using namespace std;

_PICO_BEGIN

Err WatersCalibration::Coefficents::readFunctionCoefficientsFromSQLite(Sqlite * db, int function_number, bool cal_mod) {
    std::string key, value;
    if (cal_mod)
        key = "Cal Modification ";
    else
        key = "Cal Function ";
    key += std::to_string(function_number);
    if (!db->getKeyValuePropertyFromFilesInfo(key, &value))
        return kErrorFileIO;
    if (value.size() == 0)
        return kError;
    if (!parseCalibrationLine(value))
        return kErrorFileIO;
    return kNoErr;
}

bool WatersCalibration::Coefficents::parseCalibrationLine(const std::string & text) {
    coeffients.clear();
    PicoUtil utl;
    std::vector<std::string> vstr = utl.split(text, ",", true);

    for (int ii = (int)vstr.size() - 1; ii >= 0; ii--){
        std::string sti = vstr[ii];
        if (sti.size() > 1){
            if (sti[0] == 'T'){
                if (sti[1] == '0')
                    type = CoefficentsType_T0;
                else if (sti[1] == '1')
                    type = CoefficentsType_T1;
                else
                    type = CoefficentsType_None;
            }
            vstr.erase(vstr.begin() + ii); break;
        }
    }

    if (vstr.size() > 6){
        std::cout << "too many calib coefficients" << std::endl; return false;
    }

    for (unsigned int ii = 0; ii < vstr.size(); ii++)
        coeffients.push_back(atof(vstr[ii].c_str()));
    return true;
}

// constructor
WatersCalibration::WatersCalibration(){
    m_originalLines.clear();
    m_max_function = 0;
    m_functionCalibration.clear();
    m_functionCalibrationModification.clear();
}

// destructor:
WatersCalibration::~WatersCalibration(){
}

// read calibration info from given raw directory path
Err WatersCalibration::readFromPath(const std::wstring & raw_dir){
    m_originalLines.clear();
    PicoUtil utl;
    if (!utl.dirExists(raw_dir)) {
        std::wcerr << "Error, can't find raw directry = " << raw_dir.c_str() << std::endl;
        return kError;
    }

    std::wstring path = utl.stripPathChar(raw_dir);
    wstring filename = path;

    filename.append(L"\\_HEADER.TXT");

    std::string line;
    std::ifstream file(filename);

    if (!file)
        return kErrorFileIO;

    std::wstring hdr_filename = raw_dir; hdr_filename.append(L"_HEADER.TXT");
    int hdr_file_size = (int)utl.getFileSize(hdr_filename);
    if (hdr_file_size<0){
        std::wcerr << "can't find header file = " << hdr_filename << "\"" << endl; return kError;
    }
    if (hdr_file_size==0){
        std::wcerr << "empty header file = " << hdr_filename << "\"" << endl; return kError;
    }
    std::cout << "header file contains " << hdr_file_size << " bytes" << endl;

    FILE* hdrFile = NULL;
    errno_t err = _wfopen_s(&hdrFile, hdr_filename.c_str(), L"rbS");
    if (err != 0){
        wcerr << "can't open header file = " << hdr_filename << "\"" << endl; return kError;
    }

    unsigned char * hdr_buf = (unsigned char *)malloc(hdr_file_size*sizeof(unsigned char)); 
    if (hdr_buf==NULL){
        std::cout << "mz buf alloc failed" << endl; fclose_safe(hdrFile); return kError;
    } 

    size_t hdr_count = fread(hdr_buf, 1, hdr_file_size, hdrFile); 
    if (hdr_count<(size_t)hdr_file_size){
        std::cout << "can't read from header file = " << hdr_filename.c_str() << endl; 
    }
    fclose_safe(hdrFile);
    char* ptc = (char*)hdr_buf;

    while (ptc - (char*)hdr_buf < (int)hdr_count){
        char cc = *ptc++; string line;
        while ((cc != '\n') && (ptc - (char*)hdr_buf < (int)hdr_count)) {
            line += cc;
            cc = *ptc++;
        }

        std::size_t found = line.find("$$ Cal Function ");
        if (found != std::string::npos){
            if (line[line.length() - 1] == '\r')
                line = line.substr(0, line.length() - 1);
            std::cout << "line[" << m_originalLines.size() << "] = " << line << endl;
            m_originalLines.push_back(line);
            int f_id = atoi((char*)line.c_str() + 16);
            if (f_id > m_max_function)
                m_max_function = f_id;
            std::size_t found = line.find(": ");
            line = line.substr(found + 2, line.length());
            Coefficents coeff;
            coeff.parseCalibrationLine(line);
            m_functionCalibration.insert(std::pair<int, Coefficents>(f_id, coeff));


        } else {
            found = line.find("$$ Cal Modification ");
            if (found != std::string::npos){
                if (line[line.length() - 1] == '\r')
                    line = line.substr(0, line.length() - 1);
                std::cout << "line[" << m_originalLines.size() << "] = " << line << endl;
                m_originalLines.push_back(line);
                int f_id = atoi((char*)line.c_str() + 20);
                if (f_id > m_max_function)
                    m_max_function = f_id;
                std::size_t found = line.find(": ");
                line = line.substr(found + 2, line.length());
                Coefficents coeff;
                coeff.parseCalibrationLine(line);
                checkCoefficients(coeff.coeffients);
                m_functionCalibrationModification.insert(std::pair<int, Coefficents>(f_id, coeff));
            }
        }
    }
    free_safe(hdr_buf);
    return kNoErr;
}

// get cal function coefficients for a given function
WatersCalibration::Coefficents WatersCalibration::coefficients(int functionNumber){
    Coefficents coef; coef.coeffients = std::vector<double>();
    coef.type = CoefficentsType_None;
    if (functionNumber <= 0 || functionNumber > m_max_function)
        return coef;

    map<int, Coefficents>::iterator itr = m_functionCalibration.find(functionNumber);
    if (itr != m_functionCalibration.end())
        return itr->second;   
    return coef;
}

// get cal modification coefficients for a given function
WatersCalibration::Coefficents WatersCalibration::calibrationModificationCoefficients(int functionNumber){
    Coefficents coef; coef.coeffients = std::vector<double>();
    coef.type = CoefficentsType_None;
    if (functionNumber <= 0 || functionNumber > m_max_function)
        return coef;
    
    map<int, Coefficents>::iterator itr = m_functionCalibrationModification.find(functionNumber);
    if (itr != m_functionCalibrationModification.end())
        return itr->second;
    return coef;
}

// append calibration information to sqlite database
bool WatersCalibration::addToSqlite(bool isAccurate, Sqlite* db){
    if (!db->addKeyValueToFilesInfo(1, "TypeWaters", "True")){
        cout << "can't add is waters to FilesInfo in sqlite -- " << string(sqlite3_errmsg(db->getDatabase())) << endl; return false;
    }
    if (!db->addKeyValueToFilesInfo(1, "CalAccurate", isAccurate ? "True" : "False")){
        cout << "can't add cal accurate to FilesInfo in sqlite -- " << string(sqlite3_errmsg(db->getDatabase())) << endl; return false;
    }
    for (unsigned int ii = 0; ii < m_originalLines.size(); ii++){
        std::string line = m_originalLines[ii];
        std::size_t found = line.find("$$ Cal Function ");
        if (found != std::string::npos){
            line = line.substr(found + 16, line.length());
            int f_id = atoi((char*)line.c_str());
            std::string key = "Cal Function " + to_string(f_id);
            std::size_t found = line.find(": ");
            std::string val = line.substr(found + 2, line.length());
            if (!db->addKeyValueToFilesInfo(1, key.c_str(), val.c_str())){
                cout << "can't add cal function to FilesInfo in sqlite -- " << string(sqlite3_errmsg(db->getDatabase())) << endl; return false;
            }
        } else {
            found = line.find("$$ Cal Modification ");
            if (found != std::string::npos){
                line = line.substr(found + 20, line.length());
                int f_id = atoi((char*)line.c_str());
                std::string key = "Cal Modification " + to_string(f_id);
                std::size_t found = line.find(": ");
                line = line.substr(found + 2, line.length());
                std::string val = line.substr(found + 2, line.length());
                if (!db->addKeyValueToFilesInfo(1, key.c_str(), val.c_str())){
                    cout << "can't add cal modification to FilesInfo in sqlite -- " << string(sqlite3_errmsg(db->getDatabase())) << endl; return false;
                }
            }
        }

    }
    return true;
}


// read calibration information from sqlite database
bool WatersCalibration::readCoefficientsFromSqlite(Sqlite * db){
    if (!db->getMaxScanIdFunctionFromSqlite(&m_max_scan_id)){
        return false;
    }
    int function_number = 1;
    Coefficents coef; Err err;
    while ((err = coef.readFunctionCoefficientsFromSQLite(db, function_number, false)) == kNoErr){
        m_functionCalibration.insert(std::pair<int, Coefficents>(function_number++, coef));
    }
    if (function_number>1)
        m_max_function = function_number - 1;
    if (err == kErrorFileIO)
        return false;
    return true;
}

// read calibration modification from sqlite database
bool WatersCalibration::readCalibrationModificationFromSqlite(Sqlite * db){
    
    Coefficents coef;
    //if (m_max_function == 0) {
    //    cout << "zero max calibration function" << endl; return false;
    //}
    if (m_max_function == 0)
        m_max_function = 99; // place holder
    for (int function_number = 1; function_number < m_max_function + 1; function_number++){
        Err err = coef.readFunctionCoefficientsFromSQLite(db, function_number, true);
        if (err == kErrorFileIO)
            return false;
        if (err == kNoErr)
            m_functionCalibrationModification.insert(std::pair<int, Coefficents>(function_number++, coef));
    }
    if (m_max_function == 0 || m_max_function == 99){
        int max = 0;
        std::map<int, Coefficents>::iterator itr = m_functionCalibrationModification.begin();
        for (unsigned int ii = 0; ii < m_functionCalibrationModification.size(); ii++){
            int func = itr->first; itr++;
            if (func > max)
                max = func;
        }
        if (max > 0)
            m_max_function = max;
    }
    return true;
}

// read calibration information from sqlite database
bool WatersCalibration::readFromSqlite(Sqlite * db){
    if (!readCoefficientsFromSqlite(db))
        return false;
    return readCalibrationModificationFromSqlite(db);
}

// check coefficients
void WatersCalibration::checkCoefficients(std::vector<double> &coeffients){
    if (coeffients.size() > 0){
        if (coeffients.size() == 1){
            if (coeffients[0] = 0)
                coeffients.clear();
        } else {
            bool all_zero = true;
            for (unsigned int ii = 0; ii < coeffients.size(); ii++){
                if (ii == 1) continue;
                if (coeffients[ii] != 0){
                    all_zero = false; break;
                }
            }
            if (all_zero && (coeffients[1] == 1 || coeffients[1] == 0))
                coeffients.clear();
        }
    }
}

// get max scan id
std::vector<int> WatersCalibration::getMaxScanId(){
    return m_max_scan_id;
}

_PICO_END
