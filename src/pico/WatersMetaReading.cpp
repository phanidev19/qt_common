
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996) // xmemory warning C4996: 'std::_Uninitialized_copy0': Function call with
                              // parameters that may be unsafe - this call relies on the caller to check
                              // that the passed values are correct.
#endif

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <fstream>
#include <codecvt>
#include "PicoUtil.h"
#include "WatersMetaReading.h"
#include "Reader.h"

#include <PmiQtCommonConstants.h>

#include <QFile>
#include <QString>

#define kCalibrateLevelNumber 9

using namespace pico;
using namespace std;

Err WatersMetaReading::readFromWatersPath(const std::wstring &path)
{
    PicoUtil utl;
    if (!utl.dirExists(path)) {
        std::wcerr << "Error, can't find raw directry = " << path.c_str() << std::endl;
        return kError;
    }

    m_path = utl.stripPathChar(path);
    wstring filename = m_path;

    filename.append(L"\\" + g_extern_inf_filename);
    QFile file(QString::fromStdWString(filename));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return kErrorFileIO;
    }
    int currentFunction = -1;

    // extract meta data from binary file
    std::vector<FunctionRec> functionRecords;

    Reader rdr;
    int ms_func_cnt = 0;
    bool valid_FUNCTNS = rdr.getWatersFunctionInfo(path, functionRecords, &ms_func_cnt);

#ifdef TEST_MISSING__FUNCTNS_INF
    valid_FUNCTNS = false; //For testing
#endif
    if (valid_FUNCTNS) {
        int cal_cnt = 0;
        for (unsigned int func_index = 0; func_index < functionRecords.size(); func_index++){ // if valid functions found, insert them in internal data structure
            FunctionInfo function;
            function.functionNumber = func_index + 1; // the function index is base 0, functions are base 1
            function.functionTypeRaw = functionRecords[func_index].type; // assign type and mode. this should be same as SeeMS's ms level
            function.functionModeRaw = functionRecords[func_index].mode;

            function.accurate = functionRecords[func_index].accurate; // if any function is marked as accurate (typically the last function), disable calibration
            if (function.accurate)
                m_isAccurate = true;
            function.function_type = functionRecords[func_index].function_type; // copy function params (e.g., type, mode, etc.) into member info
            function.ion_mode = functionRecords[func_index].ion_mode;
            function.data_type = functionRecords[func_index].data_type;
            function.continuum = functionRecords[func_index].continuum;

            function.MSLevel = _getMSLevelForFunction(function.function_type, function.data_type, function.functionTypeRaw, function.functionModeRaw); // verify that calibration function is marked as MS9
            //if (function.MSLevel == 1 && (function.data_type == 13 || (ms_func_cnt>1 && func_index == ms_func_cnt - 1 && function.data_type == 12))){
            //    function.MSLevel = 9;
            //}  
            if (function.MSLevel == 1 && functionRecords[func_index].cal_function){
                function.MSLevel = 9;
            }  
            if (function.MSLevel == 9) {
                cal_cnt++;
            }
            m_functionNumberToFunctionInfo.insert({ function.functionNumber, function });
        }
        if (cal_cnt == 0){
            cout << "caution: no lock mass function found" << endl;
        } else if (cal_cnt > 1){
            cout << "caution: more than one lock mass function found (count=" << cal_cnt << ")" << endl;
        }

    } else {
        bool valid2 = rdr.getWatersFunctionInfoOnlyUsingExternInf(filename, functionRecords);
        if (!valid2){
            cerr << "Error, can't access both _functns.inf or extern.inf files -- exiting" << endl;
            return kErrorFileIO;
        }

        while (!file.atEnd()) {
            const QString line = file.readLine();
            if (line.contains(kFunction_Parameters)) {
                FunctionInfo function;

                // parse line for function number and function name
                QStringList functionData = line.split('-');

                if (functionData.size() == 3) {
                    // remove extra spaces left over from split
                    functionData[1] = functionData[1].mid(1, functionData[1].size() - 2);
                    functionData[2] = functionData[2].mid(1);

                    function.functionNumber = functionData[1].mid(functionData[1].lastIndexOf(" ")).toInt(); // extract integer from "Function #"
                    function.functionName = functionData[2]; // e.g. "TOF PARENT FUNCTION"
                    int func_index = function.functionNumber - 1;
                    if (func_index < 0 || func_index >= (int)functionRecords.size()) {
                        cout << "Warning, could not read functionData. func_index,functionRecords.size()=" << func_index << "," << functionRecords.size() << endl;
                        cout << "Assumed to be MS level of 1" << endl;
                        function.functionTypeRaw = -1;
                        function.functionModeRaw = -1;
                        function.MSLevel = 1;

                        function.accurate = false;
                        function.function_type = -1;
                        function.ion_mode = -1;
                        function.data_type = -1;
                        function.continuum = false;
                        function.centroid_from_observation = UndeterminedData;

                    } else {
                        function.functionTypeRaw = functionRecords[func_index].type;
                        function.functionModeRaw = functionRecords[func_index].mode;

                        function.accurate = functionRecords[func_index].accurate;
                        if (function.accurate)
                            m_isAccurate = true;
                        function.function_type = functionRecords[func_index].function_type;
                        function.ion_mode = functionRecords[func_index].ion_mode;
                        function.data_type = functionRecords[func_index].data_type;
                        function.continuum = functionRecords[func_index].continuum;

                        if (valid_FUNCTNS) {
                            function.MSLevel = _getMSLevelForFunction(function.function_type, function.data_type, function.functionTypeRaw, function.functionModeRaw);
                        } else {
                            //Note: getWatersFunctionInfoOnlyUsingExternInf returns ms level into functionType
/*                            if (function.functionTypeRaw == 9) {
                                //Note: to minimize change, we'll let _tempMSLevelPatchForCalibration below patch this instead here.
                                function.MSLevel = 1;
                            } else {
                                function.MSLevel = function.functionTypeRaw;
                            }*/
                        }
                    }
                    m_functionNumberToFunctionInfo.insert({ function.functionNumber, function });
                    currentFunction = function.functionNumber;
                }
                continue;
            }

            if (currentFunction != -1 && !line.isEmpty()) {
                const QStringList lineData = line.split('\t', QString::SkipEmptyParts);

                if (lineData.size() == 1)
                    m_functionNumberToFunctionInfo.at(currentFunction)
                        .m_fieldNameToFieldValue.insert(lineData[0], "");

                if (lineData.size() == 2)
                    m_functionNumberToFunctionInfo.at(currentFunction)
                        .m_fieldNameToFieldValue.insert(lineData[0], lineData[1]);

                m_functionNumberToFunctionInfo.at(currentFunction)
                    .trackMetaFieldsOrder.push(lineData[0]);
            }
        }
    } 

    //Perfome MSE check
    _mseCheck();

    //NOTE(Yong): Good that you uncommented this: outfile.open("function_parameters.txt", std::ios::app)
    //When we deploy, we cannot write debug out like in relative path because:
    //1) When we install to C:\Programs Files\etc... we cannot create/write file without admin privilege. So the ofstream would be invalid and likely cause error if written into.
    //2) We only write debug information that customers can see; keep it clean.

    //Do temporary patch
    _tempMSLevelPatchForCalibration();

    //Insert UV tracees, which usually do not exists in the _extern.inf file.
    for (int func_index = 0; func_index < (int)functionRecords.size(); func_index++) {
        int functionNumber = func_index + 1;
        if (m_functionNumberToFunctionInfo.find(functionNumber) == m_functionNumberToFunctionInfo.end()) {
            FunctionInfo function;
            function.functionNumber = functionNumber;
            function.functionName = "FUNCTION_NAME_UNKNOWN";
            function.functionTypeRaw = functionRecords[func_index].type;
            function.functionModeRaw = functionRecords[func_index].mode;

            function.accurate = false;
            function.function_type = -1;
            function.ion_mode = -1;
            function.data_type = -1;
            function.continuum = false;
            function.centroid_from_observation = UndeterminedData;

            function.accurate = functionRecords[func_index].accurate;
            function.function_type = functionRecords[func_index].function_type;
            function.ion_mode = functionRecords[func_index].ion_mode;
            function.data_type = functionRecords[func_index].data_type;
            function.continuum = functionRecords[func_index].continuum;

            function.MSLevel = _getMSLevelForFunction(function.function_type, function.data_type, function.functionTypeRaw, function.functionModeRaw);
            m_functionNumberToFunctionInfo.insert({ function.functionNumber, function });
        }
    }

    return kNoErr;
}

inline bool contains(const std::unordered_map<int, WatersMetaReading::FunctionInfo> & obj, int key)
{
    auto it = obj.find(key);
    if (it == obj.end())
        return false;
    return true;
}

inline int mslevel(const std::unordered_map<int, WatersMetaReading::FunctionInfo> & obj, int key)
{
    auto it = obj.find(key);
    if (it == obj.end())
        return -1;
    return it->second.MSLevel;
}


//Perfome MSE check on extracted functions
void WatersMetaReading::_mseCheck()
{
    //Criteria #1
    //raw file contains at least 2 or more functions labeled as “TOF PARENT FUNCTIONS”
    //Criteria #2
    //contains two or more "TOF MS FUNCTION" <<-- newly found .inf file had two of these instead of "TOF PARENT FUNCTIONS"
    //Criteria #3
    //can contain only one mslevel 1.
    int tofParentFunctionCnt = 0;
    int tofMSFunctionCnt = 0;
    int msLevelOneCnt = 0;
    for (std::unordered_map<int, FunctionInfo>::const_iterator it = m_functionNumberToFunctionInfo.begin(); it != m_functionNumberToFunctionInfo.end(); ++it) {
        if (it->second.functionName.compare(kTOF_PARENT_FUNCTION) == 0) {
            tofParentFunctionCnt++;
        }
        if (it->second.functionName.compare(kTOF_MS_FUNCTION) == 0) {
            tofMSFunctionCnt++;
        }
        if (it->second.MSLevel == 1) {
            msLevelOneCnt++;
        }
    }

    //Criteria #2
    if (tofMSFunctionCnt >= 2 && msLevelOneCnt >= 2) {
        cout << "Note: mse with tofMSFunctionCnt,msLevelOneCnt" << tofMSFunctionCnt << "," <<  msLevelOneCnt << endl;
        m_isMSE = true;
        m_functionNumberToFunctionInfo.at(2).MSLevel = 2;
        return;
    }

    //Generic assumption that we can only have one mslevel of 1.
    //If there are two mslevel 1, then it must be mse or calibrant as the second.
    //We'll assume calibrant if function 2 says "REFERENCE"
    if (msLevelOneCnt == 2) {
        if ((mslevel(m_functionNumberToFunctionInfo, 1) == 1) &&
            (mslevel(m_functionNumberToFunctionInfo, 2) == 1) ) 
        {
            if (m_functionNumberToFunctionInfo.at(2).functionName.compare(kREFERENCE) != 0) {
                m_isMSE = true;
                m_functionNumberToFunctionInfo.at(2).MSLevel = 2;
                return;
            }
        }
    }

    //Generic assumption that we can only have one mslevel of 1.
    //If there are three (or more) mslevel 1, then it must be mse with calibrant as the third.
    if (msLevelOneCnt >= 3) {
        if ((mslevel(m_functionNumberToFunctionInfo, 1)==1) &&
            (mslevel(m_functionNumberToFunctionInfo, 2)==1) &&
            (mslevel(m_functionNumberToFunctionInfo, 3)==1) ) 
        {
            m_isMSE = true;
            m_functionNumberToFunctionInfo.at(2).MSLevel = 2;
            //function 3 will be assigned to 9 by _tempMSLevelPatchForCalibration()
            return;
        }
    }

    //Does not meet criteria #1
    if (tofParentFunctionCnt < 2)
        return;

    //Criteria #2
    //the first of these functions (typically Function 1, but may not always be) contains a [PARENT MS SURVEY] section, which includes *BOTH* of the following line entries:
    //a) Parent Survey High CE (v)   xxx
    //b) Parent Survey Low CE (v)    yyy
    //c) xxx >= yyy*1.5
    if (m_functionNumberToFunctionInfo.find(1) != m_functionNumberToFunctionInfo.end()) {
        //check for [PARENT MS SURVEY] in Function 1
        const QHash<QString, QString> functionFields = m_functionNumberToFunctionInfo.at(1).m_fieldNameToFieldValue;
        if (functionFields.find(k_PARENT_MS_SURVEY_) != functionFields.end()) {
            //check for Parent Survey High CE and Parent Survey Low
            if (functionFields.find(kParent_Survey_High_CE__V_) != functionFields.end() && functionFields.find(kParent_Survey_Low_CE__V_) != functionFields.end()) {
                float parentSurveyHigh = functionFields.value(kParent_Survey_High_CE__V_).toFloat();
                float parentSurveyLow = functionFields.value(kParent_Survey_Low_CE__V_).toFloat();

                if (parentSurveyHigh >= parentSurveyLow*1.5) {
                    //Criteria #3
                    //ALL subsequent “TOF PARENT FUNCTIONS” functions (other than the first) also contain a [PARENT MS SURVEY] section, which includes one of the following entries:
                    //1) Ramp High Energy from   uuu to zzz
                    //  1a) zzz is larger than uuu 
                    //  1b) BOTH uuu and zzz are larger than yyy
                    //2) Parent Survey High CE & Parent Survey Low CE

                    for (std::unordered_map<int, FunctionInfo>::const_iterator it = m_functionNumberToFunctionInfo.begin(); it != m_functionNumberToFunctionInfo.end(); ++it) {
                        //only look at functions after function 1
                        int funcNum = it->first;
                        if (funcNum > 1 && it->second.functionName.compare(kTOF_PARENT_FUNCTION) == 0) {
                            QHash<QString, QString> funcFields = m_functionNumberToFunctionInfo.at(funcNum).m_fieldNameToFieldValue;
                            //check for [PARENT MS SURVEY]
                            if (funcFields.find(k_PARENT_MS_SURVEY_) != funcFields.end()) {
                                //check for Ramp High Energy
                                if (funcFields.find(kRamp_High_Energy_from) != funcFields.end()) {
                                    QString rampHighEnergyStr = funcFields.value(kRamp_High_Energy_from);
                                    rampHighEnergyStr.remove(" ");
                                    rampHighEnergyStr = pmi::replaceOnce(rampHighEnergyStr, "to", "-");
                                    const QStringList rampHighEnergyValues = rampHighEnergyStr.split('-');
                                    float rampHighEnergyLow = rampHighEnergyValues[0].toFloat();
                                    float rampHighEnergyHigh = rampHighEnergyValues[1].toFloat();

                                    if ((rampHighEnergyHigh > rampHighEnergyLow) && (rampHighEnergyLow > parentSurveyLow) && (rampHighEnergyHigh > parentSurveyLow)) {
                                        m_isMSE = true;
                                        continue;
                                    }
                                } 
                                
                                //check for Parent Survey High CE and Parent Survey Low
                                else if (funcFields.find(kParent_Survey_High_CE__V_) != funcFields.end() && funcFields.find(kParent_Survey_Low_CE__V_) != funcFields.end()) {
                                    if (funcFields.value(kParent_Survey_High_CE__V_).toFloat() >= (funcFields.value(kParent_Survey_Low_CE__V_).toFloat() * 1.5)) {
                                        m_isMSE = true;
                                        continue;
                                    }
                                }

                                else {
                                    m_isMSE = false;
                                    break;
                                }
                            } 
                        } 
                    }

                    if (m_isMSE)
                        m_functionNumberToFunctionInfo.at(2).MSLevel = 2;
                }
            } 
        } 
    } 
}

//This patch does not do any changes now.  It's here to find potential errors.
//If the last one is also MS1, we will assume it's a calibration function and adjust
void WatersMetaReading::_tempMSLevelPatchForCalibration()
{
    //There's sometimes a MS level 1 scan as the last function.  Or if there are UV functions, it won't be last but instead right before all the UVs.
    if (m_functionNumberToFunctionInfo.size() <= 1)
        return;

    //Note(old): This might be MSE data.  So, let's not mess with this.-
    //Note(new): We have intact data with two functions, with second function being calibration
    //P:\PMI_Share_Data\2015Intact\Intact Antibody\IntactAntibody1.raw
    //So, we cannot just ignore something with just two.  
    //if (m_functionNumberToFunctionInfo.size() == 2)
    //    return;

    int last_function_with_ms1 = -1;

    //The pattern we expect and fix:
    //function: 1,2,3,4,5,6
    //mslevel : 1,2,2,2,2,1
    //mslevel : 1,2,2,2,2,9  <-- fixed.

    //mslevel : 1,2,2,2,1,0
    //mslevel : 1,2,2,2,9,0  <-- fixed.

    //Get function numbers
    vector<int> functionNumbers;
    for (auto kv : m_functionNumberToFunctionInfo) {
        functionNumbers.push_back(kv.first);
    }
    std::sort(functionNumbers.begin(), functionNumbers.end());

    for (int i = (int)functionNumbers.size() - 1; i >= 1; i--) {
        int func_val = functionNumbers.at(i);
        FunctionInfo info = m_functionNumberToFunctionInfo.at(func_val);
        if (info.MSLevel == kCalibrateLevelNumber) {
            break; //already found, no need to find another.
        }
        if (info.MSLevel == 0) {
            continue; //ignore UV channels
        }
        if (info.MSLevel != 1) {
            break;  //we should find ms level 1 before anything else (other than uv)
        }

        //Note: "2014 0909 PM RS 2.raw" contains "TOF PARENT FUNCTION" as calibrant
        if (info.MSLevel == 1 && (info.functionName == kREFERENCE || info.functionName == kTOF_PARENT_FUNCTION || info.functionName == kTOF_MS_FUNCTION)) {
            std::cerr << "Found calibrant at function= " << func_val << endl;
            std::cout << "Found calibrant at function= " << func_val << endl;
            last_function_with_ms1 = func_val;
            break;
        }
    }

    if (last_function_with_ms1 > 0) {
        if (m_functionNumberToFunctionInfo[last_function_with_ms1].MSLevel != kCalibrateLevelNumber) {
            std::cerr << "<status>warning</status>We should not need to re-assign ms level to CalibrateLevelNumber. last_function_with_ms1, MSLevel=" << last_function_with_ms1 << "," << m_functionNumberToFunctionInfo[last_function_with_ms1].MSLevel << std::endl;
        }
        //m_functionNumberToFunctionInfo[last_function_with_ms1].MSLevel = kCalibrateLevelNumber;
    }
}


int WatersMetaReading::_getMSLevelForFunction(int function_type, int data_type, int functionTypeRaw, int functionModeRaw) const
{
    int msLevel;

/*
    //Error checking -- old code
    if (functionType < 0 || functionType > 33) {
        std::cerr << "[WatersMetaReading::_getMSLevelForFunction] Function type is outside of expected range. MS level cannot be computed." << std::endl;
        return -1;
    }
*/

    //switch (functionType) {
    switch (function_type) {
    case FunctionType_Daughters:
    //case FunctionType_MSMS:
    case FunctionType_MS2:
    case FunctionType_TOF_Daughter:
    case FunctionType_Auto_Daughters:
        msLevel = 2;
        break;

    case FunctionType_SIR:
        msLevel = 1; 
        break;

    case FunctionType_MRM:
    case FunctionType_AutoSpec_MRM:
    case FunctionType_AutoSpec_Q_MRM_Quad:
    case FunctionType_AutoSpec_MIKES_Scan:
        msLevel = 2;
        break;


    case FunctionType_Neutral_Loss:
        msLevel = 2;
        break;

    case FunctionType_Neutral_Gain:
        msLevel = 2;
        break;

    case FunctionType_Parents:
    case FunctionType_Scan:
    case FunctionType_Q1F:
    case FunctionType_TOF:
    case FunctionType_TOF_MS:
    case FunctionType_TOF_Survey:
    case FunctionType_TOF_Parent:
    case FunctionType_MALDI_TOF:
        msLevel = 1; 
        break;

    //these functions are not mass spectra
    case FunctionType_Diode_Array:
        msLevel = 0;
        break;

    case FunctionType_Off:
    case FunctionType_Voltage_Scan:
    case FunctionType_Magnetic_Scan:
    case FunctionType_Voltage_SIR:
    case FunctionType_Magnetic_SIR:
        msLevel = 0;
        break;

    /* TODO: figure out what these function types translate to
    FunctionType_Delay
    FunctionType_Concatenated
    FunctionType_TOF_PSD
    FunctionType_AutoSpec_B_E_Scan
    FunctionType_AutoSpec_B2_E_Scan
    FunctionType_AutoSpec_CNL_Scan
    FunctionType_AutoSpec_MIKES_Scan
    FunctionType_AutoSpec_NRMS_Scan
    */

    default:
        msLevel = -1;
    }

    switch (functionModeRaw) {
    case FunctionMode_MSN:
        //do nothing
        break;

    case FunctionMode_Calibrate:
        if (msLevel == 1)
            msLevel = 9;
        else
            std::cerr << "[WatersMetaReading::_getMSLevelForFunction] Function mode == 0xF1 but msLevel != 1" << std::endl;
        break;

    case FuncitonMode_UV_Trace:
        msLevel = 0;
        break;

    default:
        std::cerr << "Warning, [WatersMetaReading::_getMSLevelForFunction] Unexpected function mode 0x" << std::hex << functionModeRaw << std::dec << std::endl;
    }

    if (isCalibrationFunction(functionModeRaw))
        msLevel = 9;

    return msLevel;
}

int WatersMetaReading::getMSLevelForFunction(int func_val) const
{
    if (m_functionNumberToFunctionInfo.find(func_val) != m_functionNumberToFunctionInfo.end()) 
        return m_functionNumberToFunctionInfo.at(func_val).MSLevel;
    
    //Note: 0 is expected for UV traces. If we are not sure what the value is, we'll still return -1.
    return -1;
}

// set function type
bool WatersMetaReading::setFunctionObservationCentroidMode(unsigned int func_val, CentroidMode centroid_mode)
{
    std::unordered_map<int, FunctionInfo>::iterator itr = m_functionNumberToFunctionInfo.find(func_val);
    if (itr == m_functionNumberToFunctionInfo.end()){
        cout << "function value out of range" << endl; return false;
    } else {
        itr->second.centroid_from_observation = centroid_mode;
    }
    return true;
}

// get function type
CentroidMode WatersMetaReading::centroidModeFromObservation(int func_val) const
{
    return m_functionNumberToFunctionInfo.at(func_val).centroid_from_observation;
}


static string getWatersMSE_check_Dorons_version(const wstring &meta_filename)
{
    Reader reader;
    std::vector<wstring> metadata = reader.readMetadata(meta_filename);
    if (metadata.size()==0){
        return "NA_no_file";
    }
    std::vector<std::vector<wstring>> info = reader.extractMetadata(metadata);
    if (metadata.size()==0){
        wcerr << "empty metadata file = " << meta_filename << endl; return "NA_metadata_empty";
    }
    std::vector<wstring> functions, uv_traces;
    //if (info.size()!=functions.size()+1)
    //    info.resize(functions.size()+1);
    bool isMSE1 = reader.checkMSE(info);
    if (isMSE1)
        return "Y";
    return "N";
}

bool WatersMetaReading::writeMetaToDB_forTesting(const std::wstring &filePath, const std::wstring &outSqliteFile)
{
    sqlite3 *db = NULL;

    /*
    * Open 'Waters Extern Info' SQLite database
    */
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    if (sqlite3_open(conv.to_bytes(outSqliteFile).c_str(), &db) != SQLITE_OK) {
        std::cout << "Can't open sqlite database:\n" << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    string isMSE_doron = getWatersMSE_check_Dorons_version(m_path + L"\\" + g_extern_inf_filename);

    /*
    * 'Files' Table
    */
    sqlite3_stmt *stmt = NULL; 
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    //Create Table
    if (sqlite3_prepare_v2(db, "CREATE TABLE IF NOT EXISTS Files(Id INTEGER PRIMARY KEY, Path TEXT, isMSE TEXT, isMSE_D);", -1, &stmt, NULL) != SQLITE_OK) {
        std::cout << "Can't create 'Files' table in sqlite:\n" << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cout << "SQLite Error:\n" << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    if (stmt)
        sqlite3_finalize(stmt);

    //Insert directory path
    if (sqlite3_prepare_v2(db, "INSERT INTO Files (Path, isMSE, isMSE_D) Values(?, ?, ?);", -1, &stmt, NULL) != SQLITE_OK) {
        std::cout << "Error in preparing INSERT statement.\n" << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, conv.to_bytes(filePath).c_str(), static_cast<int>(filePath.size()), 0);
    if (m_isMSE)
        sqlite3_bind_text(stmt, 2, "Y", static_cast<int>(strlen("Y")), SQLITE_TRANSIENT);
    else
        sqlite3_bind_text(stmt, 2, "N", static_cast<int>(strlen("N")), SQLITE_TRANSIENT);

    sqlite3_bind_text(stmt, 3, isMSE_doron.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cout << "SQLite Error:\n" << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    if (stmt)
        sqlite3_finalize(stmt);

    //Used to link Files & Info tables
    int fileId = (int) sqlite3_last_insert_rowid(db);

    /*
    * 'Info' Table
    */

    //Create Table
    if (sqlite3_prepare_v2(db, "CREATE TABLE IF NOT EXISTS Info(FilesId INT, FunctionNumber INT, Field TEXT, Value TEXT);", -1, &stmt, NULL) != SQLITE_OK) {
        std::cout << "Can't create 'Info' table in sqlite:\n" << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cout << "SQLite Error:\n" << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    if (stmt)
        sqlite3_finalize(stmt);

    if (sqlite3_prepare_v2(db, "INSERT INTO Info(FilesId, FunctionNumber, Field, Value) Values(?, ?, ?, ?);", -1, &stmt, NULL) != SQLITE_OK) {
        std::cout << "Error in preparing INSERT statement.\n" << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    for (std::unordered_map<int, FunctionInfo>::const_iterator it = m_functionNumberToFunctionInfo.begin(); it != m_functionNumberToFunctionInfo.end(); ++it) {
        int funcNum = it->first;
        const QString funcName = it->second.functionName;
        int funcType = it->second.functionTypeRaw;
        int funcMode = it->second.functionModeRaw;
        int msLevel = it->second.MSLevel;

        //Insert 'Funcition Name'
        sqlite3_bind_int(stmt, 1, fileId);
        sqlite3_bind_int(stmt, 2, funcNum);
        sqlite3_bind_text(stmt, 3, "Function Name", static_cast<int>(strlen("Function Name")), 0);
        sqlite3_bind_text(stmt, 4, funcName.toStdString().c_str(), funcName.size(), 0);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "SQLite Error:\n" << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        sqlite3_reset(stmt);

        //Insert 'Function Type'
        sqlite3_bind_int(stmt, 1, fileId);
        sqlite3_bind_int(stmt, 2, funcNum);
        sqlite3_bind_text(stmt, 3, "Function Type", static_cast<int>(strlen("Function Type")), 0);
        sqlite3_bind_int(stmt, 4, funcType);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "SQLite Error:\n" << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        sqlite3_reset(stmt);

        //Insert 'Funcition Mode'
        sqlite3_bind_int(stmt, 1, fileId);
        sqlite3_bind_int(stmt, 2, funcNum);
        sqlite3_bind_text(stmt, 3, "Function Mode", static_cast<int>(strlen("Function Mode")), 0);
        sqlite3_bind_int(stmt, 4, funcMode);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "SQLite Error:\n" << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        sqlite3_reset(stmt);

        //Insert 'MS Level'
        sqlite3_bind_int(stmt, 1, fileId);
        sqlite3_bind_int(stmt, 2, funcNum);
        sqlite3_bind_text(stmt, 3, "MS Level", static_cast<int>(strlen("MS Level")), 0);
        sqlite3_bind_int(stmt, 4, msLevel);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "SQLite Error:\n" << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        sqlite3_reset(stmt);


        //Insert all other fields in the order in which they appear in _extern.inf
        std::queue<QString> q = it->second.trackMetaFieldsOrder;
        while (!q.empty()) {
            const QString field = q.front();
            if (it->second.m_fieldNameToFieldValue.find(field) != it->second.m_fieldNameToFieldValue.end()) {
                const QString value = it->second.m_fieldNameToFieldValue.value(field);

                sqlite3_bind_int(stmt, 1, fileId);
                sqlite3_bind_int(stmt, 2, funcNum);
                sqlite3_bind_text(stmt, 3, field.toStdString().c_str(), field.size(), 0);
                sqlite3_bind_text(stmt, 4, value.toStdString().c_str(), value.size(), 0);

                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    std::cout << "SQLite Error:\n" << sqlite3_errmsg(db) << std::endl;
                    return false;
                }
                sqlite3_reset(stmt);
            }
            q.pop();
        }
    }

    if (stmt)
        sqlite3_finalize(stmt);

    sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);

    /*
    * Close SQLite database
    */
    sqlite3_close(db);

    return true;
}

class WatersMetaReadingTest {
public:
    WatersMetaReadingTest() {
        WatersMetaReading obj;
        obj.readFromWatersPath(L"E:\\R-BSA digest-mse-072415.raw");
    }
};

//static WatersMetaReadingTest watersMetaReadingTest_obj;

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
