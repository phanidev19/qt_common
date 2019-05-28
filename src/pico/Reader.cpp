// ********************************************************************************************
// Reader.cpp : Direct Reader
//
//      @Author: D.Kletter    (c) Copyright 2014, PMI Inc.
// 
// This module reads in a raw MS file and produces an uncompressed byspec2 file.
// The algorithm uses a proprietary run length hop coding that leverages the sparse data nature
// of the underlying data.
//
// The algorithm can be applied to an entire byspec file or one spectra at a time:
// call PicoLocalDecompress::decompress(string input_db_filename, string out_compressed_db_filename) to 
// decompress the entire file:
//
//      @Arguments:  <input_db_filename>  the full path to the input (compressed) byspec2 file to decompress.
//                   <out_uncompressed_db_filename>  full path to the uncompressed output byspec2, where
//                    the result is written
//
// Alternatively, call:
// PicoLocalDecompress::decompressSpectra(unsigned char** compressed, unsigned int* compressed_length, 
//         double** restored_mz, float** restored_intensity, unsigned int* length)
// to decompress one scanline at a time. In order to support high performance, the input file is opened only
// once in the beginning, and an sqlite pointer is passed for subsequent spectra, thereby avoiding the extra
// overhead associated with any subsequent closing and re-opeining the input file for each scan. The caller
// is responsible for ::freeing the restored mz and intensity memory after they have been been written or copied.
// as well as the compressed input buffer.
//
//      @Arguments:  <**compressed> pointer to the address of a compressed blob of a single spectra from the input file
//                   <*compressed_length> the length of the compressed spectra blob (in bytes) 
//                   <**restored_mz> the resulting uncompressed mz blob of the entire scan, allocated by PicoDeompress 
//                   <**restored_intensity> the resulting uncompressed intensity blob of the entire scan, allocated by PicoDeompress 
//                   <*length> the number of elements in the resulting uncompressed mz and/or intensity blobs 
//
//      @Returns: true if successful, false otherwise
//
// *******************************************************************************************

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996) // xmemory warning C4996: 'std::_Uninitialized_copy0': Function call with
                              // parameters that may be unsafe - this call relies on the caller to check
                              // that the passed values are correct.
#endif

#include <ios>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <map>
#include <vector>
#include <string>
#include <codecvt>
//#include <assert.h>
#include <intrin.h>
#include <boost/algorithm/string.hpp>
#include "Sqlite.h"
#include "Reader.h"
#include "Centroid.h"
#include "PicoUtil.h"
#include "PicoLocalCompress.h"
#include "WatersMetaReading.h"
//#include "WatersCalibration.h"
#include "PicoLocalDecompress.h" //<=== debug only

#include <PmiQtCommonConstants.h>

#define DEFAULT_MS_TYPE 2
#define DYNAMIC_CALIBRATION_MS_ID 9
#define BUF_LEN 4800000L
#define MS_TYPE6_CNT 100
#define MIN_MS_TYPE6_DELTA 0x612 //0x200
#define MIN_MS_TYPE6_DELTA_DBL 0.3 
#define MIN_PRECURSOR_MZ 10.0  //we are trying to catch 0.1, 0.3, 1e-30, 1e+25
#define MAX_PRECURSOR_MZ 10000.0
#define MAX_FUNC 256
const unsigned int IDX_BUFLEN = 128;
const unsigned long long R_BUFLEN = 8ULL*1024ULL*1024ULL;  // 8MB read, must be multiple of 16
const unsigned long long W_BUFLEN = 8ULL*1024ULL*1024ULL;  // 8MB write, must be multiple of 16
const unsigned long long WI_BUFLEN = 4ULL*1024ULL*1024ULL;  // 4MB write, must be multiple of 16
const unsigned int MIN_CENTROID_LEN = 256;
const double MIN_DELTA = 5E-4;

const std::string READER_VERSION = "1.0.1";

std::wstring g_extern_inf_filename = L"_extern.inf";

bool g_output_function_info = false;

FunctionInfoMetaDataPrinter functionInfoMetaDataPrinter;

using namespace std;

_PICO_BEGIN

const char* FunctionTypeString[] = {
    "MS",
    "SIR",
    "DLY",
    "CAT",
    "OFF",
    "PAR",
    "MSMS",
    "NL",
    "NG",
    "MRM",
    "Q1F",
    "MS2",
    "DAD",
    "TOF",
    "PSD",
    "TOFS",
    "TOFD",
    "MTOF",
    "TOFMS",
    "TOFP",
    "ASPEC_VSCAN",
    "ASPEC_MAGNET",
    "ASPEC_VSIR",
    "ASPEC_MAGNET_SIR",
    "QUAD_AUTO_DAU",
    "ASPEC_BE",
    "ASPEC_B2E",
    "ASPEC_CNL",
    "ASPEC_MIKES",
    "ASPEC_MRM",
    "ASPEC_NRMS",
    "ASPEC_MRMQ"
};

const char* IonModeString[] = {
    "EI+",
    "EI-",
    "CI+",
    "CI-",
    "FB+",
    "FB-",
    "TS+",
    "TS-",
    "ES+",
    "ES-",
    "AI+",
    "AI-",
    "LD+",
    "LD-",
    "FI+",
    "FI-"
};

const char* DataTypeString[] = {
    "COMPRESSED",
    "STANDARD",
    "SIR_MRM",
    "SCAN_CONTINUUM",
    "MCA",
    "MCASD",
    "MCB",
    "MCBSD",
    "MOLWEIGHT",
    "HIAC_CALIBRATED",
    "SFPREC",
    "EN_UNCAL",
    "EN_CAL",
    "EN_CAL_ACC"
};


inline bool tic_sort_by_retention_time (TicRec &ci, TicRec &cj) 
{ 
    if (ci.retention_time == cj.retention_time) return (ci.tic_value > cj.tic_value); 
    return (ci.retention_time < cj.retention_time); 
}

inline bool sort_by_count1 (CntRecord &ci, CntRecord &cj) 
{ 
    if (ci.count == cj.count) return (ci.value < cj.value); 
    return (ci.count > cj.count); 
}

inline bool sort_by_value1 (CntRecord &ci, CntRecord &cj) 
{ 
    //if (ci.count == cj.count) return (ci.value < cj.value); 
    return (ci.value < cj.value); 
}

#define PRINT_DB_ERR(db,cmd) {std::cerr << "db error: " << sqlite3_errmsg(db) << std::endl << __FILE__ << ":" << __LINE__ << std::endl << "cmd:" << std::endl << cmd << std::endl;} 

static void
begin_transaction(sqlite3 * db) {
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
}

static void
end_transaction(sqlite3 * db) {
    sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
}

//This function will update ParentScanNumber and ParentNativeId using RetentionTime
//This function defines parent as the most level-1 scan; this will override existing ones, if any.
//This function assumes ScanNumber and NativeId are already populated.
//NOTE: So far, the above definition holds for Thermo and other instruments we've worked with,
//      but we may need to adjust this definition.
static void
UpdateParentScanNumberAndParentNativeId(sqlite3 *byspecDB)
{
    const string cmd_select_Spectra = "SELECT MSLevel, Id as SpectraId, NativeId, ParentNativeId, ScanNumber FROM Spectra ORDER BY RetentionTime";
    const string cmd_update = "UPDATE Spectra SET ParentScanNumber=?, ParentNativeId=? WHERE Id = ?";
    const string cmd_rettime_index = "CREATE INDEX IF NOT EXISTS idx_SpectraRetentionTime ON Spectra(RetentionTime)";

    sqlite3_exec(byspecDB, cmd_rettime_index.c_str(), NULL, NULL, NULL);

    sqlite3_stmt *selectStmt = NULL, *updateStmt = NULL;
    if (sqlite3_prepare_v2(byspecDB, cmd_select_Spectra.c_str(), -1, &selectStmt, NULL) != SQLITE_OK) {
        PRINT_DB_ERR(byspecDB, cmd_select_Spectra);
        return;
    }
    if (sqlite3_prepare_v2(byspecDB, cmd_update.c_str(), -1, &updateStmt, NULL) != SQLITE_OK) {
        PRINT_DB_ERR(byspecDB, cmd_update);
        return;
    }
    int scanNumber = -1;
    //int parentScanNumber = -1;
    vector<int> level_last_scanNumber;
    vector<string> level_last_nativeId;
    //note: this is for keeping last used scan at different levels; 100 is more than sufficient to account for scan levels
    level_last_scanNumber.resize(100, -1);
    level_last_nativeId.resize(100);
    begin_transaction(byspecDB);
    while (sqlite3_step(selectStmt) == SQLITE_ROW) {
        int level = sqlite3_column_int(selectStmt, 0);
        int spectraId = -1;
        spectraId = sqlite3_column_int(selectStmt, 1);
        string nativeIdStr, parentNativeIdStr;
        if (sqlite3_column_text(selectStmt, 2)){
            char * ptx = (char*)sqlite3_column_text(selectStmt, 2);
            if (ptx)
                nativeIdStr = ptx;
        }
        if (sqlite3_column_text(selectStmt, 3)){
            char * ptx = (char*)sqlite3_column_text(selectStmt, 3);
            if (ptx)
                parentNativeIdStr = ptx;
        }
        scanNumber = sqlite3_column_int(selectStmt, 4);

        if (int(level_last_scanNumber.size()) <= level) {
            level_last_scanNumber.resize(level + 1, -1);
            level_last_nativeId.resize(level + 1);
        }
        sqlite3_bind_int(updateStmt, 3, spectraId);
        if (level > 1 && level_last_scanNumber[level - 1] >= 0) {
            sqlite3_bind_int(updateStmt, 1, level_last_scanNumber[level - 1]);
        }
        else {
            sqlite3_bind_null(updateStmt, 1);
        }

        if (level > 1 && level_last_nativeId[level - 1].size() > 0) {
            sqlite3_bind_text(updateStmt, 2, level_last_nativeId[level - 1].c_str(), -1, SQLITE_TRANSIENT);
        }
        else {
            if (parentNativeIdStr.size() > 0) {
                sqlite3_bind_text(updateStmt, 2, parentNativeIdStr.c_str(), -1, SQLITE_TRANSIENT);
            }
            else {
                sqlite3_bind_null(updateStmt, 2);
            }
        }

        if (sqlite3_step(updateStmt) != SQLITE_DONE) {
            PRINT_DB_ERR(byspecDB, cmd_update);
            return;
        }
        sqlite3_reset(updateStmt);
        level_last_scanNumber[level] = scanNumber;
        level_last_nativeId[level] = nativeIdStr;
        /*
        if (spectraId < 10) {
        std::cout << "level = " << level << " spectraId = " << spectraId << " scanNumber =" << scanNumber << endl;
        }
        */
    }
    //error:
    if (selectStmt) sqlite3_finalize(selectStmt);
    if (updateStmt) sqlite3_finalize(updateStmt);
    end_transaction(byspecDB);
}

//Note(2016-10-03): Wilfred needs this info to properly run Byonic and Supernovo.
//https://proteinmetrics.atlassian.net/browse/PMI-1453
bool updateSpectraCommentFieldForSupernovo_Patch(sqlite3 * db)
{
    Sqlite sqlite_db(db);

    //Get filename only
    //C:\data_input\Mass_Spectra\cona_tmt0saxpdetd.raw
    /*
    SELECT Id, Filename
    FROM Files
    */

    //Get filename only: cona_tmt0saxpdetd

    //Each Spectra.Comment = filename + "." + NativeId
    //E.g. "cona_tmt0saxpdetd.function=1 process=0 scan=2"

    std::wstring fileRootname;
    if (!sqlite_db.getFileRootFromFilesTableInPicoByspec(&fileRootname)) {
        return false;
    }

    // get number of Ids
    std::vector<int> spectraIdList;
    if (!sqlite_db.getIdsFromPicoByspecSpectraTable(&spectraIdList)) {
        return false;
    }

    sqlite3_exec(sqlite_db.getDatabase(), "BEGIN TRANSACTION", NULL, NULL, NULL);

    bool success = true;
    for (unsigned int i = 0; i < spectraIdList.size(); i++) {
        int currentRowId = spectraIdList[i];

        // get native id from picobyspec
        std::wstring nativeId;
        if (!sqlite_db.getNativeIdFromPicoByspecSpectraTableForId(currentRowId, &nativeId)) {
            success = false;
            break;
        }

        // create desired comment string
        std::wstring newCommentString = fileRootname + L"." + nativeId;

        // update byspec with comment string
        if (!sqlite_db.updateSpectraCommentAtId(currentRowId, newCommentString)) {
            success = false;
            break;
        }
    }

    sqlite3_exec(sqlite_db.getDatabase(), "END TRANSACTION", NULL, NULL, NULL);
    return success;
}

Reader::Reader()
{
    top_k = -1;
    uncert_val = 0.01; end_uncert_val = uncert_val;
    merge_radius = 4.0*uncert_val;
    uncert_type = ConstantUncertainty;
    m_identifyFunctionMsTypeExpectNoZeros = true; // assume any file type -- turn off for non-intact files
    disable_profile_centroiding = false;
    verbose = 0;
}

// constructor
Reader::Reader(int verbose1)
{
    top_k = -1;
    uncert_val = 0.01; end_uncert_val = uncert_val;
    merge_radius = 4.0*uncert_val;
    uncert_type = ConstantUncertainty;
    m_identifyFunctionMsTypeExpectNoZeros = true; // assume any file type -- turn off for non-intact files
    disable_profile_centroiding = false;
    verbose = verbose1;
}

Reader::~Reader()
{
}

// reader file
bool Reader::readRawFile(const wstring &input_db_filename, const wstring &out_compressed_db_filename,
                         ReaderType type, CentroidProcessing centroid_type, ReadMode read_mode,
                         bool adjust_intensity, bool sort_mz, int argc, wchar_t** argv)
{
    if (type==Type_ReadBruker1){ 
        cerr << "unsupported file compression type" << endl;
        return false; //compressType0(input_db_filename, out_compressed_db_filename);

    } else if (type==Type_ReadWaters1){
        if (read_mode == Read_All){
            return readRawFileType1ToByspec(input_db_filename, out_compressed_db_filename, centroid_type, adjust_intensity, sort_mz, argc, argv);
        } else if (read_mode==Read_Chromatograms){
            return readChromatogramsFromRawFileType1ToByspec(input_db_filename, out_compressed_db_filename, centroid_type);  
        } else {
            cerr << "unsupported read_mode" << endl;
            return false;
        }
    } else {
        cerr << "unsupported file compression type" << endl;
        return false;
    }
}

// read raw Waters file to byspec
bool Reader::readRawFileType1ToByspec(const wstring &raw_dir_, const wstring &out_db_filename, CentroidProcessing centroid_type, bool adjust_intensity, bool sort_mz, int argc, wchar_t** argv)
{
    Err e = kNoErr;
    PicoUtil* utl = new PicoUtil();
    wstring raw_dir(raw_dir_);
    if (!utl->dirExists(raw_dir)){
        wcerr << "can't find raw directry = " << raw_dir << endl;
        delete utl;
        return false;
    }
    wstring rdir = utl->stripPathChar(raw_dir); raw_dir = rdir.append(L"\\");
    std::vector<wstring> list = utl->dirList(raw_dir);
    if (list.size()==0){
        wcerr << "empty raw directry = " << raw_dir << endl;
        delete utl;
        return false;
    }
    WatersMetaReading watersMeta;
    e = watersMeta.readFromWatersPath(raw_dir);
    if (e != kNoErr) {
        wcerr << "Error reading meta information of raw directry = " << raw_dir << endl;
        delete utl;
        return false;
    }

    wstring meta_filename;
    std::vector<wstring> functions, uv_traces;
    for (unsigned int ii=0; ii<list.size(); ii++){
        wstring name = utl->stripPathChar(list[ii]);
        wstring prefix = name.substr(0,6), suffix = name.substr(name.size()-4,name.size());
        if (::pico::iequals(prefix, L"_FUNC0") && ::pico::iequals(suffix, L".DAT"))
            functions.push_back(name);
        if (::pico::iequals(name, g_extern_inf_filename))
            meta_filename = raw_dir + name;
    }
    delete utl;
    std::vector<wstring> metadata; std::vector<std::vector<wstring>> info; wstring isMSEStr = L"0";
    if (meta_filename.size()>0){
        metadata = readMetadata(meta_filename);
        if (metadata.size() == 0){
            return false;
        }
        info = extractMetadata(metadata);
        if (metadata.size() == 0){
            wcerr << "empty metadata file = " << meta_filename << endl; return false;
        }
        if (info.size() != functions.size() + 1)
            info.resize(functions.size() + 1);
        bool isMSE1 = checkMSE(info);
        bool isMSE2 = watersMeta.isMSE();

        if (isMSE2) {
            isMSEStr = L"1";
        }
        if (isMSE1 != isMSE2) {
            cerr << "Note: mse check differs: isMSE1,isMSE2=" << isMSE1 << "," << isMSE2 << endl;
            isMSEStr += L"x";
        }
    }

    int FilesId = 1;
    bool profile_mode = true;  // true for MS1, false for centroid or MS2
    wstring MS_Type;
    if (info.size() > 0){
        for (unsigned int kk = 0; kk < info[0].size(); kk++){
            wstring entry = info[0][kk];
            if (entry.compare(L"Type") == 0){
                if (kk < info[0].size() - 1){
                    wstring next = info[0][kk + 1];
                    if (next.size() > 2){
                        if (next.substr(0, 2).compare(L"MS") == 0){
                            MS_Type = next.substr(0, 3); //kk++; //<======= refine
                        }
                    }
                }
            } else if (entry.compare(L"ProfileMode") == 0){
                if (kk < info[0].size() - 1){
                    wstring next = info[0][kk + 1];
                    /*    if (next.size()>=7){
                            if (next.substr(0,7).compare("Profile")==0)
                            profileMode = true;
                            }*/
                    if (next.size() >= 8){
                        if (next.substr(0, 8).compare(L"Centroid") == 0)
                            profile_mode = false;
                    }
                }
            }
        }
    }

    wstring out_filename = out_db_filename;
    if (out_filename.size()==0){
        wcerr << "empty output database = " << out_filename << endl;
        return false;
    }
/*    if (out_filename.size()>=8){
        if (out_filename.substr(out_filename.size()-8, out_filename.size()).compare(".byspec2") == 0){
            out_filename.append(".byspec2");
        }
    }*/

    std::vector<std::vector<string>> calib_metadata; std::vector<std::vector<string>> calib_coeff; std::vector<string> calib_modf;
    if (!readCalibrationDataFromMetadataFile(raw_dir, &calib_metadata, &calib_coeff, &calib_modf)){
        std::cout << "can't read calib coeff from meta file" << endl; return false; 
    }

    Sqlite* sql_out = new Sqlite(out_filename);
    if(!sql_out->openDatabase()){
        wcerr << "can't open output sqlite database = " << out_filename << endl;
        delete sql_out; return false;
    } 
    if(!sql_out->setMode()){
        wcerr << "can't set output sqlite mode = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if(!sql_out->createNewByspecFile()){
        wcerr << "can't create output sqlite mode = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if(!sql_out->writeFilesTableToSqlite(raw_dir, list, FilesId)){
        wcerr << "can't write Files table in sqlite file = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if (!sql_out->addKeyValueToFilesInfo(1, L"MSE", isMSEStr.c_str())){
        wcerr << "can't add mse tag to FilesInfo table in sqlite file = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }
    if (argc > 0){
        for (int ii=0; ii<argc; ii++){
            wostringstream stri; stri << "argv_" << ii;
            if (!sql_out->addKeyValueToFilesInfo(1, stri.str().c_str(), argv[ii])){
                wcerr << "can't add mse tag to FilesInfo table in sqlite file = " << out_filename << endl;
                sql_out->closeDatabase(); delete sql_out; return false;
            }
        }
    }
    sqlite3* db_out = sql_out->getDatabase();
    if (centroid_type!=Centroid_All_Uncompressed && centroid_type!=Centroid_All)
        sql_out->addPeaksMS1CentroidedTableToByspecFile(); 

/*    bool no_calibration = (calib_coeff.size()==0);
    if (!no_calibration){
        if (!sql_out->addCalibrationTableToByspecFile()){
            return false; 
        }
    }

    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    if (!no_calibration){
        sqlite3_stmt *stmt; bool success1 = false; int rc1;
        for (unsigned int ii=0; ii<calib_metadata.size(); ii++){
            string metatext = calib_metadata[ii][0];
            for (unsigned int jj=1; jj<calib_metadata[ii].size(); jj++)
                metatext.append(calib_metadata[ii][jj]);
            ostringstream cmd; cmd << "INSERT into Calibration(Id,MzCoeffs,MetaText) values ('" << (ii+1) << "','" << calib_coeff[ii][calib_coeff[ii].size()-1] << "','" << metatext << "')"; 
            if ((rc1=sqlite3_prepare_v2(db_out, cmd.str().c_str(), -1, &stmt, NULL)) == SQLITE_OK){
                rc1 = sqlite3_step(stmt);
                if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                    success1 = true;
                } 
            }
            sqlite3_finalize(stmt);  
            if (!success1){
                if (verbose==1) std::cout << "rc1=" << rc1 << ": can't write calibration data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                sql_out->closeDatabase(); delete sql_out; return false;
            }
        }
    }*/

    PicoLocalCompress* plc = new PicoLocalCompress(true);
    unsigned long long total_original_size = 0, total_compressed_size = 0; 
    int scan_number = 1; std::vector<float> ms1_retention_time;
    std::vector<std::vector<TicRec>> tic; 
    auto start_tm = chrono::system_clock::now();

    int idx_size = identifyIdxSize(raw_dir, functions);
    if (idx_size<0){
        std::cout << "can't determine index size" << endl; delete plc; return false;
    }

    bool ms_type_6 = false;
    int ms_type = identifyFunctionMsType(raw_dir, idx_size);
    if (ms_type==1){
        ms_type_6 = true; std::cout << "identified ms type = ms_type_6" << endl;
    }
    int first_MS1_function = -1; std::vector<string> labels; std::vector<unsigned int> remove; 
    for (unsigned int function=0; function<functions.size(); function++){
        int type = identifyFunctionType(function + 1, watersMeta);
        if (type==0){
            uv_traces.push_back(functions[function]); remove.push_back(function);
        } else {
            if (first_MS1_function<0){
                if (type==1)
                    first_MS1_function = function;
            } else {
                if (type==1)
                    type = DEFAULT_MS_TYPE;
            }
        }
        string labeli = type==0?"UVtrace":type==3?"REF":"MS"+std::to_string((long long)type); labels.push_back(labeli);
        if (verbose==1) std::cout << "  -- function=" << (function+1) << " of " << functions.size() << ": identified type = " << labeli << endl;
    }
    for (int ii=(int)remove.size()-1; ii>=0; ii--)
        functions.erase(functions.begin() + remove[ii]);

    WatersCalibration wtrs_cal;
    Err er = wtrs_cal.readFromPath(raw_dir);
    if (er != kNoErr) {
        wcerr << "can't read calibration data" << raw_dir << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }

    if (!wtrs_cal.addToSqlite(watersMeta.isAccurate(), sql_out)){
        wcerr << "can't write calibration data to sqlite" << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }

    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    bool sts_override = false, check_override = true;
    for (unsigned int function = 1; function<functions.size() + 1; function++){
        wstring name_prefix(L"_FUNC");
        wstring name = appendZeroPaddedNumberForWatersFile(name_prefix, function);

        wstring name1 = name + L".DAT";
        wstring filename = raw_dir + name1;
        std::cout << "processing function=" << function << ":" << endl;

        int MSLevel = watersMeta.getMSLevelForFunction(function);
        /*
        string labeli = labels[function-1];
        if (labeli[0]=='M')
            MSLevel = labeli[2]-'0';
        else 
            if (labeli[0]=='R')
                MSLevel = DYNAMIC_CALIBRATION_MS_ID;
        */

        std::vector<unsigned int> v3c_tab;
        bool f_type_v3c = false;
        // this reverts change made in pmi-1352
        if (function == functions.size() && MSLevel == 9){ //ms_type_6 &&
        //if (ms_type_6 && function == functions.size() && MSLevel==9){
            int vai = identifyFunctionType_V3C(raw_dir, function); 
            if (vai == 1)
                f_type_v3c = true; v3c_tab = buildV3CLookupTable(25);
        }

        double IsolationWindowLowerOffset = -1.0, IsolationWindowUpperOffset = -1.0;
        if (function==1){ // <=========== extract only the first MS1 tic for now
            tic.resize(function);
        }

        wstring idx_name = name + L".IDX";
        wstring idx_filename = raw_dir + idx_name;
        int idx_file_size = (int)getFileSize(idx_filename);
        if (idx_file_size<0){
            wcerr << "can't open idx file = " << idx_filename << "\"" << endl; delete plc; return false;
        }
        if (idx_file_size==0){
            wcerr << "empty idx file = " << idx_filename << "\"" << endl; delete plc; return false;
        }
        if (verbose==1) std::cout << "idx file contains " << idx_file_size << " bytes" << endl;

        FILE* idxFile = NULL;
        errno_t err = _wfopen_s(&idxFile, idx_filename.c_str(), L"rbS");
        if (err != 0){
            std::cout << "can't open idx file = " << idx_filename.c_str() << "\"" << endl; delete plc; return false;
        }

        unsigned char * idx_buf = (unsigned char *)malloc(idx_file_size*sizeof(unsigned char)); 
        if (idx_buf==NULL){
            std::cout << "mz buf alloc failed" << endl; fclose_safe(idxFile); delete plc; return false;
        } 

        size_t idx_count = fread(idx_buf, 1, idx_file_size, idxFile); 
        if (idx_count<(size_t)idx_file_size){
            std::cout << "can't read from idx file = " << idx_filename.c_str() << endl;
        }
        fclose_safe(idxFile);

        unsigned char* sts_buf = 0, *pts = 0; bool has_sts = true; int sts_file_size = 0;
        unsigned short sts_len = 0;
        if (function > 1 || (function == 1 && MSLevel>1)){ // allow precursor mz extraction even without MS1
            wstring sts_name = name + L".STS";
            wstring sts_filename = raw_dir + sts_name;
            sts_file_size = (int)getFileSize(sts_filename);
            if (sts_file_size<0){
                std::cout << "can't open sts file = " << sts_filename.c_str() << "\"" << endl; has_sts = false; //free_safe(idx_buf); break;
            }
            if (sts_file_size==0){
                std::cout << "empty sts file = " << sts_filename.c_str() << "\"" << endl; has_sts = false; //free_safe(idx_buf); break;
            }
            FILE* stsFile = NULL; 
            if (has_sts){
                if (verbose==1) std::cout << "sts file contains " << sts_file_size << " bytes" << endl;
                errno_t err = _wfopen_s(&stsFile, sts_filename.c_str(), L"rbS");
                if (err != 0){
                    std::cout << "can't open sts file = " << sts_filename.c_str() << "\"" << endl; has_sts = false; //free_safe(idx_buf); break;
                }
            }

            if (has_sts){
                sts_buf = (unsigned char *)malloc(sts_file_size*sizeof(unsigned char)); 
                if (sts_buf==NULL){
                    std::cout << "mz buf alloc failed" << endl; has_sts = false; free_safe(idx_buf); fclose_safe(stsFile); break;
                } 

                size_t sts_count = fread(sts_buf, 1, sts_file_size, stsFile); 
                if (sts_count<(size_t)sts_file_size){
                    std::cout << "can't read from sts file = " << sts_filename.c_str() << endl;
                }
                fclose_safe(stsFile);
                pts = (unsigned char*)sts_buf;
                unsigned short sts_offset = *(unsigned short*)pts;  
                sts_len = *(unsigned short*)(pts + 4);
                if (sts_offset < sts_file_size){
                    pts += sts_offset;
                    if (check_override){
                        sts_override = checkStsOverride(pts, static_cast<int>(sts_count) - sts_offset, sts_file_size - sts_offset, ms_type_6); check_override = false;
                    }
                    pts += (sts_override || !ms_type_6) ? 0x2a : 0x26;
                }
            }
        }

        long long remain_count = getFileSize(filename);//, file_size = remain_count/8;
        if (verbose==1) std::cout << "infile contains " << remain_count << " bytes" << endl;
        if (remain_count<0){
            std::cout << "can't find input file = \"" << filename.c_str() << "\"" << endl; free_safe(sts_buf); free_safe(idx_buf); delete plc; return false;
        }
        if (remain_count==0){
            std::cout << "empty input file = \"" << filename.c_str() << "\"" << endl; free_safe(sts_buf); free_safe(idx_buf); delete plc; return false;
        }

        FILE* inFile = NULL;
        err = _wfopen_s(&inFile, filename.c_str(), L"rbS");
        if (err != 0){
            std::cout << "can't open file" << endl; free_safe(sts_buf); return false;
        }

        std::vector<double> calib_coefi; int calib_type = 0; //int start = 0;       
        //bool no_calibration = (calib_coeff.size() == 0) || (function == 1 && watersMeta.isAccurate()); //<=== skip calib for accurate data
        bool no_calibration = (calib_coeff.size() == 0) || (watersMeta.getMSLevelForFunction(function)<9 && watersMeta.isAccurate()); //<=== skip calib for accurate data
        if(g_output_function_info) {
            functionInfoMetaDataPrinter.pushCalibrationInfo(function, !no_calibration);
            continue;
        }
        if (!no_calibration){
            string str;
            if (function<calib_coeff.size())
                str = calib_coeff[function-1][calib_coeff[function-1].size()-1];
            else
                str = calib_coeff[calib_coeff.size()-1][calib_coeff[calib_coeff.size()-1].size()-1];
            if (!parseCalibrationString(str, &calib_type, calib_coefi)){
                std::cout << "can't parse calib data = " << str << "\"" << endl; return false;
            }

            if (calib_coefi.size() < 6) calib_coefi.resize(6);
            bool all_zero = true;
            for (unsigned int ii=0; ii<calib_coefi.size(); ii++){
                if (calib_coefi[ii]!=0){
                    all_zero = false; break;
                }
            }
            if (all_zero)
                calib_coefi[0] = 1.0;
            if (calib_type == 0)
                calib_coefi[1] = -calib_coefi[1];
        }
        WatersCalibration::Coefficents cal_mod_coef = wtrs_cal.calibrationModificationCoefficients(function);

        bool sts_len_flag = false; //<=== for check
        const unsigned short fact_tab[7] = { 1, 4, 16, 64, 256, 1024, 4096 };
        bool cal_offset = false, cal_offset_once = true;
        unsigned long long original_size = 0, compressed_size = 0; 
        int max_scan = (int)(idx_file_size/idx_size), precursor_scan = 0;
        unsigned char* ptx = (unsigned char*)idx_buf;
        for (int scan=0; scan<max_scan; scan++){
            auto start = chrono::system_clock::now();

            int parent_scan_number = 0, parent_function = 1; ptx+=4;
            unsigned int bb = *ptx++;
            unsigned int bb1 = *ptx++;
            unsigned int bb2 = (*ptx++ & 0x3f); // cf. PMI-1417, afam method uses flags in the high order bits that need to be masked out
            unsigned int mz_len = (bb2<<16) | (bb1<<8) | bb; 
           
            unsigned char flags = *ptx; ptx+=5;
            bool is_centroid_data = (flags==0x08)?true:false;

            float RetentionTime = *(float *)ptx; ptx+=4; 
            RetentionTime *= 60.0F; 
            if (function==1)
                ms1_retention_time.push_back(RetentionTime);
            else {
                while (precursor_scan<(int)ms1_retention_time.size() && ms1_retention_time[precursor_scan]<RetentionTime)
                    precursor_scan++;
                parent_scan_number = precursor_scan;
            }
            float tic_value = *(float *)ptx; 
            if (idx_size==22)
                ptx+=6;
            else
                ptx+=14; //<=========== for df 
            if (function==1)  // <=========== extract only the first MS1 tic for now
                tic[function-1].push_back(TicRec(RetentionTime/60.0F, tic_value)); //<----- tic in minutes

            float precursor_mz = -1.0F;
            if (has_sts && (pts-(unsigned char*)sts_buf)<sts_file_size){ //function>1 && // allow precursor mz extraction even without MS1
                precursor_mz = *(float*)pts; pts += sts_len;
                unsigned short lenx = sts_override ? 0x7a : (ms_type_6 ? 0x76 : 0x95); // check sts offset
                if (lenx != sts_len){
                    if (sts_len_flag) {
                        cout << "sts offset check mismatch for function=" << function << ", scan=" << scan << ": (new=0x" << sts_len << ", was=0x" << lenx << ")" << endl;
                        sts_len_flag = false; // output once upon first mismatching scan
                    }
                }
            }

            if (cal_offset_once && !ms_type_6 && function-1<labels.size()){
                if (labels[function-1].compare("MS9")){
                    cal_offset = checkCalOffset(inFile, mz_len); cal_offset_once = false;
                }
            }
            
            size_t size = (size_t)(ms_type_6 ? (f_type_v3c ? 12 : 6) : (f_type_v3c ? 12 : cal_offset ? 12 : 8))*mz_len*sizeof(unsigned char);
            unsigned char * buf = (unsigned char *)malloc(size);
            if (buf==NULL){
                std::cout << "read buf alloc failed" << endl;
                sql_out->closeDatabase(); delete sql_out; return false;
            }

            if (scan == 0){
                CentroidMode centroid_mode = ProfileData;
                std::cout << "auto centroid detection: ";
                if (identifyFunctionMode(function, max_scan, mz_len, f_type_v3c, ms_type_6, cal_offset, raw_dir, v3c_tab, &fact_tab[0], idx_buf, idx_size, inFile, &centroid_mode)){
                    bool centroid_detect = (centroid_mode == CentroidData);
                    if (centroid_detect != is_centroid_data){
                        std::cout << "caution: ";
                    }
                    std::cout << "F" << function << "=" << (centroid_detect ? "Centroid" : "Profile") << ", while Metadata=" << (is_centroid_data ? "Centroid" : "Profile") << endl;
                    is_centroid_data = centroid_detect; // override
                    watersMeta.setFunctionObservationCentroidMode(function, centroid_mode);
                } else {
                    std::cout << "auto centroid could not determine F" << function << " -- using Metadata = " << (is_centroid_data ? "Centroid" : "Profile") << endl;
                }
				rewind(inFile);
            }

            size_t count = fread(buf, 1, size, inFile);
            if (count<size){
                std::cout << "caution: early finish while reading data from file = \"" << filename.c_str() << "\"" << endl;  //<==============
                free_safe(buf); break; // sql_out->closeDatabase(); delete sql_out; return false;
            }
            remain_count -= count; 
/*
            if (scan == 1532){ //1488 || scan == 4175){ //<============== debug
                std::vector<unsigned int> comb_data2; comb_data2.reserve(2 * mz_len);
                unsigned int* pti = (unsigned int *)buf;
                for (unsigned int ii = 0; ii<mz_len; ii++){
                    unsigned int raw_intens = *pti++; comb_data2.push_back(raw_intens);
                    unsigned int decoded_intens = decodeIntensityType1(raw_intens); comb_data2.push_back(decoded_intens);
                    unsigned int mz = *pti++; comb_data2.push_back(mz);
                }
                ofstream myfile;
                myfile.open("h:/xx/raw_scan1532.csv");
                for (unsigned int ii = 0; ii < comb_data2.size()/3; ii++){
                    myfile << hex << std::setprecision(15) << "'" << comb_data2[3 * ii] << "," << "'" << comb_data2[3 * ii + 1] << "," << "'" << comb_data2[3 * ii + 2] << endl;
                }
                myfile.close(); 
            } //<============== debug
*/
            unsigned long long intensity_sum = 0;
            std::vector<unsigned int> comb_data; comb_data.reserve(2*mz_len); 
            if (f_type_v3c){
                unsigned int* pti = (unsigned int *)buf;
                for (unsigned int ii = 0; ii<mz_len; ii++){
                    unsigned int raw_intens = *pti++;
                    double dbl_intensity = decodeIntensityType1_V3C(raw_intens, v3c_tab, true);
                    unsigned int decoded_intens = (unsigned int)(dbl_intensity + .5); intensity_sum += decoded_intens; comb_data.push_back(decoded_intens);
                    unsigned int mz = *pti++;
                    unsigned int mz2 = recodeMzType1_6_V3C(mz); comb_data.push_back(mz2);
                    pti++;
                }
            } else {
                if (ms_type_6){
                    unsigned char* ptc = (unsigned char*)buf;
                    for (unsigned int ii = 0; ii < mz_len; ii++){
                        unsigned char* ptci = ptc + 6 * ii, *ptcm = ptci + 2;
                        unsigned int decoded_intens = *(unsigned short*)(ptci), mz = *(unsigned int*)(ptcm);
                        int scl = mz & 0xF, fact = fact_tab[scl];
                        unsigned int intensity = decoded_intens*fact; intensity_sum += intensity; comb_data.push_back(intensity); //combi.push_back(decoded_intens); ptc+=2;                 
                        unsigned int mz2 = recodeMzType1_6(mz & 0xFFFFFFF0); //double mzi = decodeMzType1_6(mz & 0xFFFFFFF0);
                        comb_data.push_back(mz2); //combmz.push_back(mz); //double mzi = decodeMzType1_6(mz); ptc+=4; 
                    }

                } else {
                    unsigned int* pti = (unsigned int *)buf; 
                    for (unsigned int ii=0; ii<mz_len; ii++){
                        unsigned int raw_intens = *pti++, decoded_intens = decodeIntensityType1(raw_intens); intensity_sum += decoded_intens; comb_data.push_back(decoded_intens);
                        unsigned int mz = *pti++; comb_data.push_back(mz); //double mzi = decodeMzType1(mz); 
                        if (cal_offset) pti++;
                    }
                }
            }
            free_safe(buf);

            // =================================Centroiding============================================== //
            bool i_first = true; int centroid_size = 0;
            if (!disable_profile_centroiding){
                if ((centroid_type != Centroid_MS1Profile && centroid_type != Centroid_MS1Profile_Uncompressed) || // do centroiding only on MS1 in MS1Profile modes, or on every MSx otherwise
                    ((centroid_type == Centroid_MS1Profile || centroid_type == Centroid_MS1Profile_Uncompressed) && profile_mode && MSLevel == 1)){
                    // --------------- centroiding code --------------------
                    double * mz_buf = NULL; float * intens_buf = NULL;
                    if (is_centroid_data){
                        mz_buf = (double *)malloc(mz_len*sizeof(double)); // allocate buffers
                        if (mz_buf == NULL){
                            std::cout << "mz buf alloc failed" << endl; free_safe(idx_buf); fclose_safe(inFile); delete plc; return false;
                        }
                        intens_buf = (float *)malloc(mz_len*sizeof(float));
                        if (intens_buf == NULL){
                            std::cout << "intens buf alloc failed" << endl; sql_out->closeDatabase(); delete sql_out; free_safe(mz_buf); fclose_safe(inFile); free_safe(idx_buf); delete plc; return false;
                        }
                        centroid_size = mz_len;

                        for (unsigned int ii = 0; ii < mz_len; ii++){
                            unsigned int decoded_intens = comb_data[2 * ii], mz = comb_data[2 * ii + 1];
                            double mzi = decodeMzType1(mz);
                            if (mzi<32.0 || mzi >= 32768.0){
                                if (i_first){
                                    std::cout << "non valid mzi value=" << mzi << " at ii=" << ii << endl; i_first = false;
                                }
                                break; // return false;
                            }
                            intens_buf[ii] = (float)decoded_intens; intensity_sum += decoded_intens;
                            if (no_calibration){
                                mz_buf[ii] = mzi;
                            } else {  // calibration
                                double oval;
                                if (calib_coefi[1] < 0){ //ms_type_6){ // for SQD
                                    //calib_coefi[1] = -calib_coefi[1];
                                    double val2 = mzi*mzi;
                                    oval = calib_coefi[0] - calib_coefi[1] * mzi + calib_coefi[2] * val2 + calib_coefi[3] * val2*mzi + calib_coefi[4] * val2*val2;// +calib_coefi[5] * val2*val2*mzi;
                                } else {
                                    double vsq = sqrt(mzi), mz4 = mzi*mzi;
                                    oval = calib_coefi[0] + calib_coefi[1] * vsq + calib_coefi[2] * mzi + calib_coefi[3] * vsq*mzi + calib_coefi[4] * mz4 + calib_coefi[5] * mz4*vsq;
                                    oval *= oval;
                                }
                                mz_buf[ii] = oval;
                            }
                        }
                    } else { // data is not centroided
                        std::vector<std::pair<double, double>> input_data; input_data.reserve(mz_len);
                        point2dList in_data;
                        in_data.reserve(mz_len);//, data_gauss_smoothed_centroided_top; 
                        for (unsigned int ii = 0; ii < mz_len; ii++){
                            unsigned int decoded_intens = comb_data[2 * ii], mz = comb_data[2 * ii + 1]; double mzi = 0;
                            //    if (ms_type_6){
                            //        mzi = decodeMzType1_6(mz);
                            //    } else {
                            mzi = decodeMzType1(mz);
                            //    }
                            if (mzi<32.0 || mzi >= 32768.0){
                                if (i_first){
                                    std::cout << "non valid mzi value=" << mzi << " at ii=" << ii << endl; i_first = false;
                                }
                                break; // return false;
                            }

                            if (no_calibration){
                                in_data.push_back(point2d(mzi, (double)decoded_intens)); input_data.push_back(pair<double, double>(mzi, (double)decoded_intens));
                            } else {  // calibration
                                double oval;
                                if (calib_coefi[1] < 0){ //ms_type_6){ // for SQD
                                    //calib_coefi[1] = -calib_coefi[1];
                                    double val2 = mzi*mzi;
                                    oval = calib_coefi[0] - calib_coefi[1] * mzi + calib_coefi[2] * val2 + calib_coefi[3] * val2*mzi + calib_coefi[4] * val2*val2;// +calib_coefi[5] * val2*val2*mzi;
                                } else {
                                    double vsq = sqrt(mzi), mz4 = mzi*mzi;
                                    oval = calib_coefi[0] + calib_coefi[1] * vsq + calib_coefi[2] * mzi + calib_coefi[3] * vsq*mzi + calib_coefi[4] * mz4 + calib_coefi[5] * mz4*vsq;
                                    oval *= oval;
                                } if (cal_mod_coef.type != WatersCalibration::CoefficentsType_None && cal_mod_coef.coeffients.size()>0){ // cal_mod
                                    mzi = oval;
                                    if (cal_mod_coef.type == WatersCalibration::CoefficentsType_T0){ // for SQD
                                        oval = 0; double val2 = 1;
                                        for (unsigned int ii = 0; ii < cal_mod_coef.coeffients.size(); ii++){
                                            oval += cal_mod_coef.coeffients[ii] * val2;
                                            val2 *= mzi;
                                        }
                                        //oval = calib_coefi[0] + calib_coefi[1] * mzi + calib_coefi[2] * val2 + calib_coefi[3] * val2*mzi + calib_coefi[4] * val2*val2;// +calib_coefi[5] * val2*val2*mzi;
                                    } else {
                                        oval = 0; double vsq = sqrt(mzi), val2 = 1;
                                        for (unsigned int ii = 0; ii < cal_mod_coef.coeffients.size(); ii++){
                                            oval += cal_mod_coef.coeffients[ii] * val2;
                                            val2 *= vsq;
                                        }
                                        //oval = calib_coefi[0] + calib_coefi[1] * vsq + calib_coefi[2] * mzi + calib_coefi[3] * vsq*mzi + calib_coefi[4] * mz4 + calib_coefi[5] * mz4*vsq;
                                        oval *= oval;
                                    }
                                }
                                in_data.push_back(point2d(oval, (double)decoded_intens)); input_data.push_back(pair<double, double>(oval, (double)decoded_intens));
                            }
                        }
/*
                        if (function == 1 && scan == 0){
                            ofstream myfile; //<============ debug
                            myfile.open("f:/waters_raw/raw_scan1_mod.csv");
                            for (unsigned int ii = 0; ii < input_data.size(); ii++){
                                std::pair<double, double> pairi = input_data[ii];
                                myfile << std::setprecision(15) << pairi.first << "," << pairi.second << endl;
                            }
                            myfile.close(); //<============ end debug
                        }
*/
                        std::vector<std::pair<double, double>> centroided_out;
                        if (i_first){
                            Centroid *cnt = new Centroid();
                            if (!cnt->smooth_and_centroid(input_data, uncert_val, end_uncert_val, uncert_type, top_k, merge_radius, adjust_intensity, sort_mz, centroided_out)){
                                centroided_out.clear(); // return false;
                            }
                            delete cnt;
                        }

                        in_data.clear();
                        in_data.swap(std::vector<point2d>(in_data));
                        input_data.clear();
                        input_data.swap(std::vector<std::pair<double, double>>(input_data));
                        centroid_size = static_cast<int>(centroided_out.size());
                        mz_buf = (double *)malloc(centroid_size*sizeof(double)); // allocate buffers
                        if (mz_buf == NULL){
                            std::cout << "mz buf alloc failed" << endl; free_safe(idx_buf); fclose_safe(inFile); delete plc; return false;
                        }
                        intens_buf = (float *)malloc(centroid_size*sizeof(float));
                        if (intens_buf == NULL){
                            std::cout << "intens buf alloc failed" << endl; sql_out->closeDatabase(); delete sql_out; free_safe(mz_buf); fclose_safe(inFile); free_safe(idx_buf); delete plc; return false;
                        }
                        double* mzi = mz_buf; float* intensi = intens_buf;
                        for (int ii = 0; ii < centroid_size; ii++){
                            double yy = centroided_out[ii].second; //point2d p2d = centroided_out[ii];
                            *mzi++ = centroided_out[ii].first; *intensi++ = (float)yy; intensity_sum += (int)yy;
                        }
                        centroided_out.clear();
                        centroided_out.swap(std::vector<std::pair<double, double>>(centroided_out));
                    }
                    unsigned char* compressed_mz = (unsigned char*)mz_buf, *compressed_intensity = (unsigned char*)intens_buf;
                    unsigned int compressed_mz_length = 0, compressed_intensity_length = 0; unsigned long long intensity_sum2 = 0;
                    if (centroid_type == Centroid_All || centroid_type == Centroid_MS1Profile || centroid_type == Centroid_MSn || centroid_type == Test_Profile_Compression || (centroid_type == Centroid_MSn_Uncompressed && profile_mode && MSLevel == 1)){
                        auto start = chrono::system_clock::now(); //-V561 (remove PVS warning)
                        if (!plc->compressRawTypeCentroided1(mz_buf, intens_buf, centroid_size, &compressed_mz, &compressed_mz_length, &compressed_intensity, &compressed_intensity_length, &intensity_sum2, false)){ //bool peak_sorting)
                            sql_out->closeDatabase(); delete sql_out; free_safe(mz_buf); free_safe(intens_buf); fclose_safe(inFile); return false;
                        }

                        //auto duration = chrono::system_clock::now() - start;
                        //auto decompress_millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
                        free_safe(mz_buf); free_safe(intens_buf);
                    }
                    sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                    ostringstream cmd1;
                    if (centroid_type == Centroid_All_Uncompressed || (centroid_type == Centroid_MSn_Uncompressed && MSLevel > 1) || (centroid_type == Centroid_MSn && MSLevel == 1 && is_centroid_data)){ // data goes to Peaks // ==================================Centroid data insertion======================================== //
                        cmd1 << "INSERT into Peaks(PeaksMz,PeaksIntensity,PeaksCount,IntensitySum,CompressionInfoId) values (?,?,'" << centroid_size << "','" << intensity_sum << "','" << WATERS_CENTROID_NO_COMPRESSION_ID << "')"; //uncompressed centroid into peaks 
                    } else if (centroid_type == Centroid_All || (centroid_type == Centroid_MSn && MSLevel > 1) || (centroid_type == Centroid_MSn && MSLevel == 1 && is_centroid_data)){
                        cmd1 << "INSERT into Peaks(PeaksMz,PeaksIntensity,PeaksCount,IntensitySum,CompressionInfoId) values (?,?,'" << centroid_size << "','" << intensity_sum << "','" << WATERS_READER_CENTROIDED_INFO_ID << "')"; //compressed centroid into peaks
                    } else if (centroid_type == Test_Profile_Compression || ((centroid_type == Centroid_MS1Profile || centroid_type == Centroid_MSn) && profile_mode && MSLevel == 1)) { // centroided data goes to Peaks_MS1Centroided // =========================================================================================== //
                        cmd1 << "INSERT into Peaks_MS1Centroided(PeaksMz,PeaksIntensity,PeaksCount,IntensitySum,CompressionInfoId) values (?,?,'" << centroid_size << "','" << intensity_sum << "','" << WATERS_READER_CENTROIDED_INFO_ID << "')"; //compressed centroid into peaks_ms1
                    } else {
                        cmd1 << "INSERT into Peaks_MS1Centroided(PeaksMz,PeaksIntensity,PeaksCount,IntensitySum,CompressionInfoId) values (?,?,'" << centroid_size << "','" << intensity_sum << "','" << WATERS_CENTROID_NO_COMPRESSION_ID << "')"; //uncompressed centroid into peaks_ms1
                    }
                    if ((rc1 = sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                        if (centroid_type == Centroid_All_Uncompressed || centroid_type == Centroid_MS1Profile_Uncompressed || (centroid_type == Centroid_MSn_Uncompressed && MSLevel > 1)){
                            sqlite3_bind_blob(stmt1, 1, compressed_mz, centroid_size * 8, SQLITE_STATIC);
                            sqlite3_bind_blob(stmt1, 2, compressed_intensity, centroid_size * 4, SQLITE_STATIC);
                        } else {
                            sqlite3_bind_blob(stmt1, 1, compressed_mz, compressed_mz_length, SQLITE_STATIC);
                            sqlite3_bind_blob(stmt1, 2, compressed_intensity, compressed_intensity_length, SQLITE_STATIC);
                        }
                        rc1 = sqlite3_step(stmt1);
                        if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                            success1 = true;
                        }
                    }
                    sqlite3_finalize(stmt1); free_safe(compressed_mz); free_safe(compressed_intensity);
                    if (!success1){
                        if (verbose == 1) std::cout << "rc1=" << rc1 << ": can't write compressed peaks data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl;
                        sql_out->closeDatabase(); delete sql_out; free_safe(idx_buf); fclose_safe(inFile); delete plc; return false;
                    }
                    if (centroid_type == Centroid_All || centroid_type == Centroid_All_Uncompressed){
                        total_compressed_size += centroid_size * 12; total_original_size += mz_len * 12;
                    }
                    if (verbose == 1) std::cout << "F" << function << ", scn=" << scan << ", cnt=" << scan_number << endl;

                }
            }

            // ====================================Profile compression============================================= //
            if (profile_mode && !is_centroid_data && (centroid_type==Test_Profile_Compression || centroid_type==Centroid_MS1Profile || centroid_type==Centroid_MS1Profile_Uncompressed || //&& MSLevel<=2) || 
                ((centroid_type==Centroid_MSn || centroid_type==Centroid_MSn_Uncompressed) && MSLevel==1))){ // ms1 only for modes 4,5 

                unsigned char* compressed_mz1 = (unsigned char*)malloc(180+8*mz_len*sizeof(char)); //52+128
                if (compressed_mz1 == NULL){
                    std::cout << "compressed m/z buf allocation failed" << endl; sql_out->closeDatabase(); delete sql_out; free_safe(idx_buf); fclose_safe(inFile); delete plc; fclose_safe(inFile); return false;
                }

                unsigned int compressed_mz_length1 = 0; 
                auto start = chrono::system_clock::now();
                if (!plc->compressRawType1_N(comb_data, mz_len, compressed_mz1, &compressed_mz_length1, &intensity_sum, calib_coefi, false)){ //ms_type_6)){ //<===================
                    sql_out->closeDatabase(); delete sql_out; return false;
                }
                auto duration = chrono::system_clock::now() - start;
                auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
                comb_data.clear();

                int combined_length = compressed_mz_length1;
                compressed_size += combined_length; original_size += mz_len*12; // 8 bytes mz + 4 bytes intensity 
                total_compressed_size += combined_length; total_original_size += mz_len*12; 

                sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                ostringstream cmd1; 
                
                cmd1 << "INSERT into Peaks(PeaksMz,PeaksCount,IntensitySum,CompressionInfoId) values (?,'"<< mz_len << "','" << intensity_sum << "','" << WATERS_READER_INFO_ID << "')"; 
                if ((rc1=sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                    sqlite3_bind_blob(stmt1, 1, compressed_mz1, compressed_mz_length1, SQLITE_STATIC); 
                    rc1 = sqlite3_step(stmt1);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt1); 
                if (!success1){
                    if (verbose==1) std::cout << "rc1=" << rc1 << ": can't write peaks data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sql_out->closeDatabase(); delete sql_out; free_safe(idx_buf); free_safe(compressed_mz1); delete plc; fclose_safe(inFile); return false;
                }

                free_safe(compressed_mz1);  
                if (verbose==1) std::cout << dec << "  F" << function << ", scn=" << scan << ", cnt=" << scan_number << ", compress=" << combined_length << ", orig=" << mz_len*12 << " bytes (cr=" << setprecision(5) << (float)mz_len*12/combined_length << setprecision(0) << ") [" << millis << "]" << endl;                                     
            }

            ostringstream NativeId; NativeId << "function="<< function << " process=0 scan=" << (scan+1);
            ostringstream ParentNativeId; ParentNativeId << "function=" << parent_function << " process=0 scan=" << parent_scan_number;
            ostringstream MetaText; 
            if (centroid_type==Centroid_MS1Profile || centroid_type==Centroid_MS1Profile_Uncompressed || centroid_type==Test_Profile_Compression || ((centroid_type==Centroid_MSn_Uncompressed || centroid_type==Centroid_MSn) && MSLevel==1)) 
                MetaText << "<RetentionTimeUnit>second</RetentionTimeUnit><PeakMode>profile spectrum</PeakMode>";
            else
                MetaText << "<RetentionTimeUnit>second</RetentionTimeUnit><PeakMode>centroid spectrum</PeakMode>";
            if (MSLevel==DYNAMIC_CALIBRATION_MS_ID)
                MetaText << "<Calibration>dynamic</Calibration>";
            sqlite3_stmt *stmt2; bool success2 = false; int rc2;
            ostringstream cmd2; 
            if (MSLevel==1 || MSLevel==DYNAMIC_CALIBRATION_MS_ID){
                if (IsolationWindowLowerOffset<0 && IsolationWindowUpperOffset<0)
                    cmd2 << "INSERT into Spectra(FilesId,MSLevel,RetentionTime,ScanNumber,NativeId,PeaksId,MetaText) "
                    << "values ('" << FilesId << "','" << MSLevel << "','" << RetentionTime << "','" << scan_number << "','" << NativeId.str() << "','" << scan_number << "','" << MetaText.str() << "');"; 
                else
                    cmd2 << "INSERT into Spectra(FilesId,MSLevel,IsolationWindowLowerOffset,IsolationWindowUpperOffset,RetentionTime,ScanNumber,NativeId,PeaksId,MetaText) "
                    << "values ('" << FilesId << "','" << MSLevel << "','" << IsolationWindowLowerOffset << "','" << IsolationWindowUpperOffset << "','" << RetentionTime << "','" << scan_number << "','" << NativeId.str() << "','" << scan_number << "','" << MetaText.str() << "');"; 
            } else {
                if (precursor_mz<0.0F){
                    if (IsolationWindowLowerOffset<0 && IsolationWindowUpperOffset<0)
                        cmd2 << "INSERT into Spectra(FilesId,MSLevel,RetentionTime,ScanNumber,NativeId,PeaksId,ParentScanNumber,ParentNativeId,MetaText) "
                        << "values ('" << FilesId << "','" << MSLevel << "','" << RetentionTime << "','" << scan_number << "','" << NativeId.str() << "','" << scan_number << "','" << parent_scan_number << "','" << ParentNativeId.str() << "','" << MetaText.str() << "');"; 
                    else
                        cmd2 << "INSERT into Spectra(FilesId,MSLevel,IsolationWindowLowerOffset,IsolationWindowUpperOffset,RetentionTime,ScanNumber,NativeId,PeaksId,ParentScanNumber,ParentNativeId,MetaText) "
                        << "values ('" << FilesId << "','" << MSLevel << "','" << IsolationWindowLowerOffset << "','" << IsolationWindowUpperOffset << "','" << RetentionTime << "','" << scan_number << "','" << NativeId.str() << "','" << scan_number << "','" << parent_scan_number << "','" << ParentNativeId.str() << "','" << MetaText.str() << "');"; 
                } else {
                    if (IsolationWindowLowerOffset<0 && IsolationWindowUpperOffset<0)
                        cmd2 << "INSERT into Spectra(FilesId,MSLevel,ObservedMz,RetentionTime,ScanNumber,NativeId,PeaksId,ParentScanNumber,ParentNativeId,MetaText) "
                        << "values ('" << FilesId << "','" << MSLevel << "','" << precursor_mz << "','" << RetentionTime << "','" << scan_number << "','" << NativeId.str() << "','" << scan_number << "','" << parent_scan_number << "','" << ParentNativeId.str() << "','" << MetaText.str() << "');"; 
                    else
                        cmd2 << "INSERT into Spectra(FilesId,MSLevel,ObservedMz,IsolationWindowLowerOffset,IsolationWindowUpperOffset,RetentionTime,ScanNumber,NativeId,PeaksId,ParentScanNumber,ParentNativeId,MetaText) "
                        << "values ('" << FilesId << "','" << MSLevel << "','" << precursor_mz << "','" << IsolationWindowLowerOffset << "','" << IsolationWindowUpperOffset << "','" << RetentionTime << "','" << scan_number << "','" << NativeId.str() << "','" << scan_number << "','" << parent_scan_number << "','" << ParentNativeId.str() << "','" << MetaText.str() << "');"; 
                }
            }
            if ((rc2=sqlite3_prepare_v2(db_out, cmd2.str().c_str(), -1, &stmt2, NULL)) == SQLITE_OK){
                rc2 = sqlite3_step(stmt2);
                if (rc2 == SQLITE_ROW || rc2 == SQLITE_DONE){
                    success2 = true;
                } 
            }
            sqlite3_finalize(stmt2);
            if (!success2){
                if (verbose==1) std::cout << "rc2=" << rc2 << ": can't write spectra to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                sql_out->closeDatabase(); delete sql_out; free_safe(idx_buf); fclose_safe(inFile); delete plc; return false;
            }

            auto duration = chrono::system_clock::now() - start;
            auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

            if (verbose>1 && scan_number>0){
                if ((scan_number%verbose)==0){
                    std::cout << dec << "numSpectra = " << scan_number << " took " << millis << " clicks, or " << float(millis)/CLOCKS_PER_SEC << " seconds" << endl;
                }
            }

            scan_number++; 
            //if (scan > 3) //<========= test
            //    break;
        }
        //sort(tic[function-1].begin(), tic[function-1].end(), tic_sort_by_retention_time);
        free_safe(idx_buf); 
        if (has_sts && function>1)
            free_safe(sts_buf); 

        if (verbose==1) std::cout << "done function=" << function << " of " << functions.size() << endl;
        //if (function > 0) //<========= test
        //    break;
            fclose_safe(inFile); 
    }
    delete plc;

    if(g_output_function_info) {
        functionInfoMetaDataPrinter.writeToFile(L"function_info.csv");
        exit(1);
    }

    bool type_ms1 = false;
    for (int ff=0; ff<(int)info.size(); ff++){
        std::vector<wstring> info_i = info[ff];
        for (int ii=0; ii<(int)info_i.size(); ii++){
            wstring key = info_i[ii];
            if (key.compare(L"Type")==0 || key.compare(L"ProfileMode")==0 || key.compare(L"StartTime")==0 || key.compare(L"EndTime")==0 || key.compare(L"StartMass")==0 || key.compare(L"EndMass")==0){
                wstring vali = info_i[++ii];
                if (vali.compare(L"MS1")==0){
                    if (!type_ms1)
                        type_ms1 = true;
                    else
                        vali[2]='2';
                }
                sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                wostringstream keyi; keyi << key.c_str(); if (ff>0) keyi << L"_" << ff;
                std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                wostringstream cmd; cmd << "INSERT into Info(Key,Value) values ('" << keyi.str() << "','" << vali.c_str() << "')";
                if ((rc1=sqlite3_prepare_v2(db_out, conv.to_bytes(cmd.str()).c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                    rc1 = sqlite3_step(stmt1);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt1);  
                if (!success1){
                    if (verbose==1) std::cout << "rc1=" << rc1 << ": can't write 'Info' metadata to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sql_out->closeDatabase(); delete sql_out; return false;
                }
            }
        }
    }
    sqlite3_exec(db_out, "COMMIT;", NULL, NULL, NULL);

    std::vector<string> tic_uv_labels;
    for (int ii = 0; ii < (int)tic.size(); ii++){
        stringstream label; label << "TIC" << (ii + 1);
        tic_uv_labels.push_back(label.str());
    }

    int trace_func_size = static_cast<int>(tic.size());
    if (uv_traces.size()>0){
        for (int trace = trace_func_size; trace<trace_func_size + (int)uv_traces.size(); trace++){
            wstring uv_dat_filename = raw_dir + uv_traces[trace - trace_func_size];
            int dat_file_size = (int)getFileSize(uv_dat_filename);
            if (dat_file_size<0){
                std::cout << "can't open uv_trace file = " << uv_dat_filename.c_str() << "\"" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }
            if (dat_file_size == 0){
                std::cout << "empty uv_trace file = " << uv_dat_filename.c_str() << "\"" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }
            if (verbose == 1) std::wcout << L"uv_trace file [" << uv_traces[trace - trace_func_size].substr(5, 3) << L"] contains " << dat_file_size << L" bytes" << endl;

            wstring uv_idx_filename = uv_dat_filename.substr(0, uv_dat_filename.size() - 3).append(L"IDX");
            int idx_file_size = (int)getFileSize(uv_idx_filename);
            if (idx_file_size<0){
                std::cout << "can't open uv_trace file = " << uv_idx_filename.c_str() << "\"" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }
            if (idx_file_size == 0){
                std::cout << "empty uv_trace file = " << uv_idx_filename.c_str() << "\"" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }
            if (verbose == 1) std::wcout << L"uv_idx file [f_id=" << uv_traces[trace - trace_func_size].substr(5, 3) << L"] contains " << idx_file_size << L" bytes" << endl;

            unsigned char * dat_buf = (unsigned char *)malloc(dat_file_size*sizeof(unsigned char));
            if (dat_buf == NULL){
                std::cout << "mz buf alloc failed" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }

            unsigned char * idx_buf = (unsigned char *)malloc(idx_file_size*sizeof(unsigned char));
            if (idx_buf == NULL){
                std::cout << "mz buf alloc failed" << endl; free_safe(dat_buf); sql_out->closeDatabase(); delete sql_out; return false;
            }

            FILE* datFile = NULL;
            errno_t err = _wfopen_s(&datFile, uv_dat_filename.c_str(), L"rbS");
            if (err != 0){
                std::cout << "can't open uv_trace file = " << uv_dat_filename.c_str() << "\"" << endl; free_safe(idx_buf); free_safe(dat_buf); sql_out->closeDatabase(); delete sql_out; return false;
            }

            size_t dat_count = fread(dat_buf, 1, dat_file_size, datFile);
            if (dat_count<(size_t)dat_file_size){
                std::cout << "can't read from tic file = " << uv_dat_filename.c_str() << endl; fclose_safe(datFile); free_safe(idx_buf); free_safe(dat_buf); sql_out->closeDatabase(); delete sql_out; return false;
            }
            fclose_safe(datFile);

            FILE* idxFile = NULL;
            err = _wfopen_s(&idxFile, uv_idx_filename.c_str(), L"rbS");
            if (err != 0){
                std::cout << "can't open uv_trace file = " << uv_idx_filename.c_str() << "\"" << endl; free_safe(idx_buf); free_safe(dat_buf); sql_out->closeDatabase(); delete sql_out; return false;
            }

            size_t idx_count = fread(idx_buf, 1, idx_file_size, idxFile);
            if (idx_count<(size_t)idx_file_size){
                std::cout << "can't read from tic file = " << uv_idx_filename.c_str() << endl; fclose_safe(idxFile); free_safe(idx_buf); free_safe(dat_buf); sql_out->closeDatabase(); delete sql_out; return false;
            }
            fclose_safe(idxFile);

            unsigned char* ptc = (unsigned char*)dat_buf, *ptx = (unsigned char*)idx_buf;
            std::vector< std::vector<TicRec>> uv_data; map<unsigned short, unsigned short> uv_map; std::vector<unsigned short> uv_freqs;
            ptc += 4; ptx += 8;
            int max_scan = (int)(idx_file_size / idx_size), idx_inc = idx_size - 4, dat_size = dat_file_size / max_scan - 1;
            if (dat_file_size != (dat_size + 1)*max_scan){
                dat_size = 5; idx_inc = 18; max_scan = (idx_file_size - 4) / 22;
            }
            for (int ii = 0; ii < max_scan; ii++){
                unsigned int freq = *ptc++;
                freq = freq * 256 + *ptc; ptc += dat_size;
                unsigned short id;
                map<unsigned short, unsigned short>::iterator itr = uv_map.find((unsigned short)freq);
                if (itr == uv_map.end()) {
                    id = (unsigned short)uv_map.size();
                    uv_map.insert(std::pair<unsigned short, unsigned short>(
                        static_cast<unsigned short>(freq),
                        static_cast<unsigned short>(uv_map.size())));
                    uv_freqs.push_back((unsigned short)freq);
                    uv_data.resize(uv_data.size() + 1);
                } else {
                    id = itr->second;
                }

                float tic_value = *(float*)ptx; ptx += 4;
                float retention_time = *(float*)ptx; ptx += idx_inc;
                uv_data[id].push_back(TicRec(retention_time, tic_value)); // in minutes
            }

            free_safe(idx_buf); free_safe(dat_buf);
            int tsize = static_cast<int>(tic.size());
            tic.resize(tic.size() + uv_data.size());
            for (unsigned int jj = 0; jj < uv_data.size(); jj++){
                std::vector<TicRec> dati = uv_data[jj];
                for (unsigned int ii = 0; ii < dati.size(); ii++){
                    tic[tsize + jj].push_back(dati[ii]);
                }
                stringstream label;
                label << "Trace" << jj << "_" << uv_freqs[jj];
                tic_uv_labels.push_back(label.str());
            }
        }
        //sort(tic[tic.size()-1].begin(), tic[tic.size()-1].end(), tic_sort_by_retention_time);

        sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
        for (int trace=0; trace<(int)tic.size(); trace++){
            //float * tm_buf = (float *)malloc(tic[trace].size()*sizeof(float)); 
            double * tm_buf = (double *)malloc(tic[trace].size()*sizeof(double)); //temp hack double instead of float
            if (tm_buf==NULL){
                std::cout << "mz buf alloc failed" << endl; return false;
            }    
            float * tc_buf = (float *)malloc(tic[trace].size()*sizeof(float)); 
            if (tc_buf==NULL){
                std::cout << "intens buf alloc failed" << endl; free_safe(tm_buf); return false;
            }    
            //float* tmi = tm_buf, *tci = tc_buf; 
            double * tmi = tm_buf; float *tci = tc_buf; //temp hack double instead of float

            for (int ii=0; ii<(int)tic[trace].size(); ii++){
                TicRec trec = tic[trace][ii];
                *tmi++ = trec.retention_time; *tci++ = trec.tic_value;
            }

            sqlite3_stmt *stmt1 = NULL; bool success1 = false; int rc1;
            ostringstream cmd1; cmd1 << "INSERT into Chromatogram(FilesId,Identifier,ChromatogramType,DataX,DataY,DataCount,MetaText) values ('" 
                << FilesId << "','" << tic_uv_labels[trace] << "','" << (trace<trace_func_size ? "total ion current chromatogram" : "absorption chromatogram")
                <<"',?,?,'"<< tic[trace].size() <<"','"<<"<RetentionTimeUnit>minute</RetentionTimeUnit>"<<"')"; 
            if ((rc1=sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                //sqlite3_bind_blob(stmt1, 1, tm_buf, tic[trace].size()*4, SQLITE_STATIC); 
                sqlite3_bind_blob(stmt1, 1, tm_buf, static_cast<int>(tic[trace].size()*8), SQLITE_STATIC); //temp hack double instead of float
                sqlite3_bind_blob(stmt1, 2, tc_buf, static_cast<int>(tic[trace].size()*4), SQLITE_STATIC);
                rc1 = sqlite3_step(stmt1);
                if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                    success1 = true;
                } 
            }
            sqlite3_finalize(stmt1); free_safe(tc_buf); free_safe(tm_buf); 
            if (!success1){
                if (verbose==1) std::cout << "rc1=" << rc1 << ": can't write peaks data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                sql_out->closeDatabase(); delete sql_out; return false;
            }        
        }
    }

    int uv_trace = static_cast<int>(functions.size()); //trace_func_size;
    std::vector<wstring> uv_metadata = readUvMetadata(raw_dir);
    for (int ii=0; ii<(int)uv_metadata.size(); ii++){
        wstring key = uv_metadata[ii];
        wstring vali = uv_metadata[++ii];
        sqlite3_stmt *stmt1; bool success1 = false; int rc1;
        wostringstream keyi; keyi << key.c_str(); keyi << "_" << uv_trace;
        wostringstream cmd; cmd << "INSERT into Info(Key,Value) values ('" << keyi.str() << "','" << vali.c_str() << "')";
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        if ((rc1=sqlite3_prepare_v2(db_out, conv.to_bytes(cmd.str()).c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
            rc1 = sqlite3_step(stmt1);
            if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                success1 = true;
            } 
        }
        sqlite3_finalize(stmt1);  
        if (!success1){
            if (verbose==1) std::cout << "rc1=" << rc1 << ": can't write 'Info' metadata to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
            sql_out->closeDatabase(); delete sql_out; return false;
        }
    }

    if (centroid_type==Centroid_MS1Profile || centroid_type==Centroid_MS1Profile_Uncompressed || centroid_type==Centroid_MSn || centroid_type==Centroid_MSn_Uncompressed || centroid_type==Test_Profile_Compression){
        if (!sql_out->addPropertyValueToCompressionInfo(WATERS_READER_INFO_ID, "pico:reader:waters:profile", READER_VERSION.c_str())){ // start with id=3
            wcerr << "can't write CompressionInfo table in sqlite file = " << out_filename << endl;
            sql_out->closeDatabase(); delete sql_out; return false;
        }
    }
    if (centroid_type==Centroid_All || centroid_type==Centroid_MS1Profile || centroid_type==Centroid_MSn || centroid_type==Centroid_MSn_Uncompressed){
        if (!sql_out->addPropertyValueToCompressionInfo(WATERS_READER_CENTROIDED_INFO_ID, "pico:reader:waters:centroid", READER_VERSION.c_str())){ // start with id=3
            wcerr << "can't write CompressionInfo table in sqlite file = " << out_filename << endl;
            sql_out->closeDatabase(); delete sql_out; return false;
        }
    }

    int chrom_base = static_cast<int>(tic_uv_labels.size()) - trace_func_size;
    wstring chroms_filename = raw_dir + L"_CHROMS.INF";
    utl = new PicoUtil();
    if (utl->fileExists(chroms_filename)){
        delete utl;
        std::vector<std::vector<std::pair<wstring, wstring>>> chroms_metadata;
        if (!readChromsMetadata(raw_dir, chroms_metadata)){
            if (verbose==1) std::cout << "can't read chroms metadata" << endl; delete sql_out; return false;
        }
        for (unsigned int ii=0; ii<chroms_metadata.size(); ii++){
            std::vector<std::pair<wstring, wstring>> chroms_i = chroms_metadata[ii];
            wstring fname, chr_filename = raw_dir;
            wstring key = L"Channel_";
            wstring value = L"Absorbance at ";
            wstring label;
            for (unsigned int jj=0; jj<chroms_i.size(); jj++){
                std::pair<wstring, wstring> pair_j = chroms_i[jj];
                if (pair_j.first == L"channel") {
                    key.append(pair_j.second); label.append(L"_" + pair_j.second);
                }
                else if (pair_j.first == L"wavelength") {
                    value.append(pair_j.second);  label.append(L"_" + pair_j.second);
                }
                else if (pair_j.first == L"filename") {
                    fname = pair_j.second; chr_filename.append(fname); chr_filename.append(L".DAT");
                }
            }
            int chro_file_size = (int)getFileSize(chr_filename);
            if (chro_file_size<0){
                std::wcout << "can't open uv_trace file = " << chr_filename << "\"" << endl; return false;
            }
            if (chro_file_size==0){
                std::wcout << "empty uv_trace file = " << chr_filename << "\"" << endl; return false;
            }
            if (verbose==1) std::wcout << "uv_trace file [" << fname.substr(5,3) << "] contains " << chro_file_size << " bytes" << endl;

            FILE* chroFile = NULL;
            errno_t err = _wfopen_s(&chroFile, chr_filename.c_str(), L"rbS");
            if (err != 0){
                std::cout << "can't open uv_trace file = " << chr_filename.c_str() << "\"" << endl; return false;
            }

            unsigned char * chro_buf = (unsigned char *)malloc(chro_file_size*sizeof(unsigned char)); 
            if (chro_buf==NULL){
                std::cout << "mz buf alloc failed" << endl; return false;
            } 

            size_t tic_count = fread(chro_buf, 1, chro_file_size, chroFile); 
            if (tic_count<(size_t)chro_file_size){
                std::cout << "can't read from uv_trace file = " << chr_filename.c_str() << endl; 
            }
            unsigned short* ptc = (unsigned short*)chro_buf;

            unsigned short offset = *ptc; 
            unsigned int size = (chro_file_size - offset)/8;

            double * tm_buf = (double *)malloc(size*sizeof(double)); //temp hack double instead of float
            if (tm_buf==NULL){
                std::cout << "tm buf alloc failed" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }    
            float * tc_buf = (float *)malloc(size*sizeof(float)); 
            if (tc_buf==NULL){
                std::cout << "tc buf alloc failed" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }    

            float* ptd = (float*)(chro_buf+offset);
            for (unsigned int jj=0; jj<size; jj++){
                tm_buf[jj] = (double)*ptd++; tc_buf[jj] = *ptd++; //temp hack double instead of float
            }

            sqlite3_stmt *stmt1 = NULL; bool success1 = false; int rc1;
            wostringstream cmd1; cmd1 << L"INSERT into Chromatogram(FilesId,Identifier,ChromatogramType,DataX,DataY,DataCount,MetaText) values ('" 
                <<FilesId<<L"','"<<L"Trace"<<(chrom_base+ii)<<(label.length()>0?label:L"")<<L"','"<<L"absorption chromatogram"<<L"',?,?,'"<< size <<L"','"<<L"<RetentionTimeUnit>minute</RetentionTimeUnit>"<<L"')"; 
            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
            if ((rc1 = sqlite3_prepare_v2(db_out, conv.to_bytes(cmd1.str()).c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                //sqlite3_bind_blob(stmt1, 1, tm_buf, tic[trace].size()*4, SQLITE_STATIC); 
                sqlite3_bind_blob(stmt1, 1, tm_buf, size*8, SQLITE_STATIC); //temp hack double instead of float
                sqlite3_bind_blob(stmt1, 2, tc_buf, size*4, SQLITE_STATIC); 
                rc1 = sqlite3_step(stmt1);
                if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                    success1 = true;
                } 
            }
            sqlite3_finalize(stmt1); free_safe(tc_buf); free_safe(tm_buf); 
            if (!success1){
                if (verbose==1) std::cout << "rc1=" << rc1 << ": can't write peaks data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                sql_out->closeDatabase(); delete sql_out; return false;
            }        

            PicoLocalCompress *plc = new PicoLocalCompress();
            delete plc; 
            free_safe(chro_buf); fclose_safe(chroFile); 
        }
    } else
        delete utl;

    sqlite3_exec(db_out, "COMMIT;", NULL, NULL, NULL);

    updateSpectraCommentFieldForSupernovo_Patch(db_out);

    sql_out->closeDatabase(); delete sql_out;
    if (verbose==1){
        std::cout << "done! overall compression=" << total_compressed_size << " orig=" << total_original_size << " bytes (cr=" << setprecision(5) << (float)total_original_size/total_compressed_size << setprecision(0) << ")" << endl;
        std::cout << "all done!" << endl;
    }
    return true;
}

// check if raw file is MSE_type, print message to console
bool Reader::checkMseFile(std::wstring &raw_dir)
{
    Err e = kNoErr;
    WatersMetaReading watersMeta;
    e = watersMeta.readFromWatersPath(raw_dir);
    if (e != kNoErr) {
        structured_output(ReaderConstants::kMessage, L"Error reading meta information of raw directry = " + raw_dir);
        structured_output(ReaderConstants::kStatus, ReaderConstants::kError);
        structured_output(ReaderConstants::kIsMSE, L"0");
        return false;
    }

/*
    string meta_filename;
    std::vector<string> functions, uv_traces;
    for (unsigned int ii = 0; ii<list.size(); ii++){
        string name = utl->stripPathChar(list[ii]);
        string prefix = name.substr(0, 6), suffix = name.substr(name.size() - 4, name.size());
        if (prefix.compare("_FUNC0") == 0 && suffix.compare(".DAT") == 0)
            functions.push_back(name);
        if (name.compare("_extern.inf") == 0)
            meta_filename = raw_dir + name;
    }
    delete utl;
    std::vector<string> metadata = readMetadata(meta_filename);
    if (metadata.size() == 0){
        return false;
    }
    std::vector<std::vector<string>> info = extractMetadata(metadata);
    if (metadata.size() == 0){
        cerr << "empty metadata file = " << meta_filename << endl; return false;
    }
    if (info.size() != functions.size() + 1)
        info.resize(functions.size() + 1);
    bool isMSE1 = checkMSE(info);
*/
    bool isMSE2 = watersMeta.isMSE();
/*
    string isMSEStr = "0";
    if (isMSE2) {
        isMSEStr = "1";
    }
    if (isMSE1 != isMSE2) {
        cerr << "Note: mse check differs: isMSE1,isMSE2=" << isMSE1 << "," << isMSE2 << endl;
        isMSEStr += "x";
    }*/
    if (isMSE2){
        structured_output(ReaderConstants::kIsMSE, L"1");
    }
    else {
        structured_output(ReaderConstants::kIsMSE, L"0");
    }
    return true;
}

// read raw Waters file chromatograms to byspec
bool Reader::readChromatogramsFromRawFileType1ToByspec(const wstring &raw_dir_, const wstring &out_db_filename, CentroidProcessing /*centroid_type*/)
{
    Err e = kNoErr;
    PicoUtil* utl = new PicoUtil();
    wstring raw_dir(raw_dir_);
    if (!utl->dirExists(raw_dir)){
        wcerr << "can't find raw directry = " << raw_dir << endl; delete utl; return false;
    }
    wstring rdir = utl->stripPathChar(raw_dir); raw_dir = rdir.append(L"\\");
    std::vector<wstring> list = utl->dirList(raw_dir);
    if (list.size()==0){
        wcerr << "empty raw directry = " << raw_dir << endl; delete utl; return false;
    }
    WatersMetaReading watersMeta;

    e = watersMeta.readFromWatersPath(raw_dir);
    if (e != kNoErr) {
        std::wcout << "Error reading meta information of raw directry = " << raw_dir.c_str() << endl;
        delete utl;
        return false;
    }

    wstring meta_filename;
    std::vector<wstring> functions, uv_traces;
    for (unsigned int ii=0; ii<list.size(); ii++){
        wstring name = utl->stripPathChar(list[ii]);
        wstring prefix = name.substr(0,6), suffix = name.substr(name.size()-4,name.size());
        if (::pico::iequals(prefix, L"_FUNC0") && ::pico::iequals(suffix, L".DAT"))
            functions.push_back(name);
        if (::pico::iequals(name, g_extern_inf_filename))
            meta_filename = raw_dir + name;
    }
    delete utl;
    std::vector<wstring> metadata = readMetadata(meta_filename);
    if (metadata.size() == 0) {
        string ss(g_extern_inf_filename.begin(), g_extern_inf_filename.end());
        cerr << "missing " << ss << endl;
    }
    std::vector<std::vector<wstring>> info = extractMetadata(metadata);
    if (info.size() == 0) {
        wcerr << "empty metadata file = " << meta_filename << endl;
    }
    if (info.size() != functions.size() + 1) {
        info.resize(functions.size() + 1);
    }
    bool isMSE1 = checkMSE(info);
    bool isMSE2 = watersMeta.isMSE();
    wstring isMSEStr = L"0";
    if (isMSE2) {
        isMSEStr = L"1";
    }
    if (isMSE1 != isMSE2) {
        cerr << "Note: mse check differs: isMSE1,isMSE2=" << isMSE1 << "," << isMSE2 << endl;
        isMSEStr += L"x";
    }

    int FilesId = 1;
    bool profile_mode = true;  // true for MS1, false for centroid or MS2
    wstring MS_Type;
    for (unsigned int kk=0; kk<info[0].size(); kk++){
        wstring entry = info[0][kk];
        if (entry.compare(L"Type")==0){
            if (kk<info[0].size()-1){
                wstring next = info[0][kk+1];
                if (next.size()>2){
                    if (next.substr(0,2).compare(L"MS")==0){
                        MS_Type = next.substr(0,3); //kk++; //<======= refine
                    }
                }
            }
        } else if (entry.compare(L"ProfileMode")==0){
            if (kk<info[0].size()-1){
                wstring next = info[0][kk+1];
            /*    if (next.size()>=7){
                    if (next.substr(0,7).compare("Profile")==0)
                        profileMode = true;
                }*/
                if (next.size()>=8){
                    if (next.substr(0,8).compare(L"Centroid")==0)
                        profile_mode = false;
                }
            }
        }
    }

    wstring out_filename = out_db_filename;
    if (out_filename.size()==0){
        wcerr << "empty output database = " << out_filename << endl; return false;
    }
    Sqlite* sql_out = new Sqlite(out_filename);
    if(!sql_out->openDatabase()){
        wcerr << "can't open output sqlite database = " << out_filename << endl;
        delete sql_out; return false;
    }
    if(!sql_out->setMode()){
        wcerr << "can't set output sqlite mode = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }
    if(!sql_out->createNewByspecFile()){
        wcerr << "can't create output sqlite mode = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }
    if(!sql_out->writeFilesTableToSqlite(raw_dir, list, FilesId)){
        wcerr << "can't write Files table in sqlite file = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }
    if (!sql_out->addKeyValueToFilesInfo(1, L"MSE", isMSEStr.c_str())){
        wcerr << L"can't add mse tag to FilesInfo table in sqlite file = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }
    sqlite3* db_out = sql_out->getDatabase();
    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL);

    //int scan_number = 1; 
    std::vector<float> ms1_retention_time; std::vector<std::vector<TicRec>> tic;
    //auto start_tm = chrono::system_clock::now();
    int idx_size = identifyIdxSize(raw_dir, functions);
    if (idx_size<0){
        std::cout << "can't determine index size" << endl; return false;
    }

    bool ms_type_6 = false;
    int ms_type = identifyFunctionMsType(raw_dir, idx_size);
    if (ms_type==1){
        ms_type_6 = true; std::cout << "identified ms type = ms_type_6" << endl;
    }
    int first_MS1_function = -1; std::vector<string> labels; std::vector<unsigned int> remove;
    for (unsigned int function = 0; function<functions.size(); function++){
        int type = identifyFunctionType(function + 1, watersMeta);
        if (type == 0){
            uv_traces.push_back(functions[function]); remove.push_back(function);
        } else {
            if (first_MS1_function<0){
                if (type == 1)
                    first_MS1_function = function;
            } else {
                if (type==1)
                    type = DEFAULT_MS_TYPE;
            }
        }
        string labeli = type == 0 ? "UVtrace" : type == 3 ? "REF" : "MS" + std::to_string((long long)type); labels.push_back(labeli);
        if (verbose == 1) std::cout << "  -- function=" << (function + 1) << " of " << functions.size() << ": identified type = " << labeli << endl;
    }
    for (int ii = (int)remove.size() - 1; ii >= 0; ii--)
        functions.erase(functions.begin() + remove[ii]);

    bool type_ms1 = false;
    for (int ff=0; ff<(int)info.size(); ff++){
        std::vector<wstring> info_i = info[ff];
        for (int ii=0; ii<(int)info_i.size(); ii++){
            wstring key = info_i[ii];
            if (key.compare(L"Type")==0 || key.compare(L"ProfileMode")==0 || key.compare(L"StartTime")==0 || key.compare(L"EndTime")==0 || key.compare(L"StartMass")==0 || key.compare(L"EndMass")==0){
                wstring vali = info_i[++ii];
                if (vali.compare(L"MS1")==0){
                    if (!type_ms1)
                        type_ms1 = true;
                    else
                        vali[2]='2';
                }
                sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                wostringstream keyi; keyi << key.c_str(); if (ff>0) keyi << L"_" << ff;
                std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;

                wostringstream cmd; cmd << "INSERT into Info(Key,Value) values ('" << keyi.str() << "','" << vali.c_str() << "')";
                if ((rc1=sqlite3_prepare_v2(db_out, conv.to_bytes(cmd.str()).c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                    rc1 = sqlite3_step(stmt1);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    }
                }
                sqlite3_finalize(stmt1);
                if (!success1){
                    if (verbose==1) std::cout << "rc1=" << rc1 << ": can't write 'Info' metadata to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl;
                    sql_out->closeDatabase(); delete sql_out; return false;
                }
            }
        }
    }
    sqlite3_exec(db_out, "COMMIT;", NULL, NULL, NULL);

    std::vector<string> tic_uv_labels;
    for (int ii = 0; ii < (int)tic.size(); ii++){
        stringstream label; label << "TIC" << (ii + 1);
        tic_uv_labels.push_back(label.str());
    }

    int trace_func_size = static_cast<int>(tic.size());
    if (uv_traces.size()>0){
        for (int trace = trace_func_size; trace<trace_func_size + (int)uv_traces.size(); trace++){
            wstring uv_dat_filename = raw_dir + uv_traces[trace - trace_func_size];
            int dat_file_size = (int)getFileSize(uv_dat_filename);
            if (dat_file_size<0){
                std::cout << "can't open uv_trace file = " << uv_dat_filename.c_str() << "\"" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }
            if (dat_file_size == 0){
                std::cout << "empty uv_trace file = " << uv_dat_filename.c_str() << "\"" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }
            if (verbose == 1) std::wcout << L"uv_trace file [" << uv_traces[trace - trace_func_size].substr(5, 3) << L"] contains " << dat_file_size << L" bytes" << endl;

            wstring uv_idx_filename = uv_dat_filename.substr(0, uv_dat_filename.size() - 3).append(L"IDX");
            int idx_file_size = (int)getFileSize(uv_idx_filename);
            if (idx_file_size<0){
                std::cout << "can't open uv_trace file = " << uv_idx_filename.c_str() << "\"" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }
            if (idx_file_size == 0){
                std::cout << "empty uv_trace file = " << uv_idx_filename.c_str() << "\"" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }
            if (verbose == 1) std::wcout << L"uv_idx file [f_id=" << uv_traces[trace - trace_func_size].substr(5, 3) << L"] contains " << idx_file_size << L" bytes" << endl;

            unsigned char * dat_buf = (unsigned char *)malloc(dat_file_size*sizeof(unsigned char));
            if (dat_buf == NULL){
                std::cout << "mz buf alloc failed" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }

            unsigned char * idx_buf = (unsigned char *)malloc(idx_file_size*sizeof(unsigned char));
            if (idx_buf == NULL){
                std::cout << "mz buf alloc failed" << endl; free_safe(dat_buf); sql_out->closeDatabase(); delete sql_out; return false;
            }

            FILE* trcFile = NULL;
            errno_t err = _wfopen_s(&trcFile, uv_dat_filename.c_str(), L"rbS");
            if (err != 0){
                std::cout << "can't open uv_trace file = " << uv_dat_filename.c_str() << "\"" << endl; free_safe(idx_buf); free_safe(dat_buf); sql_out->closeDatabase(); delete sql_out; return false;
            }

            size_t dat_count = fread(dat_buf, 1, dat_file_size, trcFile);
            if (dat_count<(size_t)dat_file_size){
                std::cout << "can't read from tic file = " << uv_dat_filename.c_str() << endl; fclose_safe(trcFile); free_safe(idx_buf); free_safe(dat_buf); sql_out->closeDatabase(); delete sql_out; return false;
            }
            fclose_safe(trcFile);

            FILE* idxFile = NULL;
            err = _wfopen_s(&idxFile, uv_idx_filename.c_str(), L"rbS");
            if (err != 0){
                std::cout << "can't open uv_trace file = " << uv_idx_filename.c_str() << "\"" << endl; free_safe(idx_buf); free_safe(dat_buf); sql_out->closeDatabase(); delete sql_out; return false;
            }

            size_t idx_count = fread(idx_buf, 1, idx_file_size, idxFile);
            if (idx_count<(size_t)idx_file_size){
                std::cout << "can't read from tic file = " << uv_idx_filename.c_str() << endl; fclose_safe(idxFile); free_safe(idx_buf); free_safe(dat_buf); sql_out->closeDatabase(); delete sql_out; return false;
            }
            fclose_safe(idxFile);

            unsigned char* ptc = (unsigned char*)dat_buf, *ptx = (unsigned char*)idx_buf;
            std::vector< std::vector<TicRec>> uv_data; map<unsigned short, unsigned short> uv_map; std::vector<unsigned short> uv_freqs;
            ptc += 4; ptx += 8;
            int max_scan = (int)(idx_file_size / idx_size), idx_inc = idx_size - 4, dat_size = dat_file_size / max_scan - 1;
            if (dat_file_size != (dat_size+1)*max_scan){
                dat_size = 5; idx_inc = 18; max_scan = (idx_file_size - 4) / 22;
            }
            for (int ii = 0; ii<max_scan; ii++){
                unsigned int freq = *ptc++;
                freq = freq * 256 + *ptc; ptc += dat_size;
                unsigned short id;
                map<unsigned short, unsigned short>::iterator itr = uv_map.find((unsigned short)freq);
                if (itr == uv_map.end()) {
                    id = static_cast<unsigned short>(uv_map.size());
                    uv_map.insert(std::pair<unsigned short, unsigned short>(
                        static_cast<unsigned short>(freq),
                        static_cast<unsigned short>(uv_map.size())));
                    uv_freqs.push_back(static_cast<unsigned short>(freq));
                    uv_data.resize(uv_data.size() + 1);
                } else {
                    id = itr->second;
                }

                float tic_value = *(float*)ptx; ptx += 4;
                float retention_time = *(float*)ptx; ptx += idx_inc;
                uv_data[id].push_back(TicRec(retention_time, tic_value)); // in minutes
            }

            free_safe(idx_buf); free_safe(dat_buf);
            int tsize = static_cast<int>(tic.size());
            tic.resize(tic.size() + uv_data.size());
            for (unsigned int jj = 0; jj < uv_data.size(); jj++){
                std::vector<TicRec> dati = uv_data[jj];
                for (unsigned int ii = 0; ii < dati.size(); ii++){
                    tic[tsize + jj].push_back(dati[ii]);
                }
                stringstream label;
                label << "Trace" << jj << "_" << uv_freqs[jj];
                tic_uv_labels.push_back(label.str());
            }
        }
        //sort(tic[tic.size()-1].begin(), tic[tic.size()-1].end(), tic_sort_by_retention_time);

        sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL);
        for (int trace = 0; trace<(int)tic.size(); trace++){
            //float * tm_buf = (float *)malloc(tic[trace].size()*sizeof(float)); 
            double * tm_buf = (double *)malloc(tic[trace].size()*sizeof(double)); //temp hack double instead of float
            if (tm_buf == NULL){
                std::cout << "mz buf alloc failed" << endl; sql_out->closeDatabase(); delete sql_out; return false;
            }
            float * tc_buf = (float *)malloc(tic[trace].size()*sizeof(float));
            if (tc_buf == NULL){
                std::cout << "intens buf alloc failed" << endl; sql_out->closeDatabase(); delete sql_out; free_safe(tm_buf); return false;
            }
            //float* tmi = tm_buf, *tci = tc_buf; 
            double * tmi = tm_buf; float *tci = tc_buf; //temp hack double instead of float

            for (int ii = 0; ii<(int)tic[trace].size(); ii++){
                TicRec trec = tic[trace][ii];
                *tmi++ = trec.retention_time; *tci++ = trec.tic_value;
            }

            sqlite3_stmt *stmt1 = NULL; bool success1 = false; int rc1;
            ostringstream cmd1; cmd1 << "INSERT into Chromatogram(FilesId,Identifier,ChromatogramType,DataX,DataY,DataCount,MetaText) values ('"
                << FilesId << "','" << tic_uv_labels[trace] << "','" << (trace<trace_func_size ? "total ion current chromatogram" : "absorption chromatogram")
                << "',?,?,'" << tic[trace].size() << "','" << "<RetentionTimeUnit>minute</RetentionTimeUnit>" << "')";
            if ((rc1 = sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                //sqlite3_bind_blob(stmt1, 1, tm_buf, tic[trace].size()*4, SQLITE_STATIC); 
                sqlite3_bind_blob(stmt1, 1, tm_buf, static_cast<int>(tic[trace].size() * 8), SQLITE_STATIC); //temp hack double instead of float
                sqlite3_bind_blob(stmt1, 2, tc_buf, static_cast<int>(tic[trace].size() * 4), SQLITE_STATIC);
                rc1 = sqlite3_step(stmt1);
                if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                    success1 = true;
                }
            }
            sqlite3_finalize(stmt1); free_safe(tc_buf); free_safe(tm_buf);
            if (!success1){
                if (verbose == 1) std::cout << "rc1=" << rc1 << ": can't write peaks data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl;
                sql_out->closeDatabase(); delete sql_out; return false;
            }
        }
    }
    sqlite3_exec(db_out, "COMMIT;", NULL, NULL, NULL);

    ptrdiff_t uv_trace = functions.size();
    std::vector<wstring> uv_metadata = readUvMetadata(raw_dir);
    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL);
    for (int ii = 0; ii<(int)uv_metadata.size(); ii++){
        wstring key = uv_metadata[ii];
        wstring vali = uv_metadata[++ii];
        sqlite3_stmt *stmt1; bool success1 = false; int rc1;
        wostringstream keyi; keyi << key; keyi << L"_" << uv_trace;
        wostringstream cmd; cmd << L"INSERT into Info(Key,Value) values ('" << keyi.str() << L"','" << vali << L"')";
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        if ((rc1=sqlite3_prepare_v2(db_out, conv.to_bytes(cmd.str()).c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
            rc1 = sqlite3_step(stmt1);
            if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                success1 = true;
            }
        }
        sqlite3_finalize(stmt1);
        if (!success1){
            if (verbose == 1) std::cout << "rc1=" << rc1 << ": can't write 'Info' metadata to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl;
            sql_out->closeDatabase(); delete sql_out; return false;
        }
    }
    if (verbose == 1) std::cout << "done uv_traces" << endl;

    ptrdiff_t chrom_base = tic_uv_labels.size() - trace_func_size;
    wstring chroms_filename = raw_dir + L"_CHROMS.INF";
    utl = new PicoUtil();
    if (utl->fileExists(chroms_filename)){
        delete utl;
        std::vector<std::vector<std::pair<wstring, wstring>>> chroms_metadata;
        if (!readChromsMetadata(raw_dir, chroms_metadata)){
            if (verbose==1) std::cout << "can't read chroms metadata" << endl; delete sql_out; return false;
        }
        for (unsigned int ii=0; ii<chroms_metadata.size(); ii++){
            std::vector<std::pair<wstring, wstring>> chroms_i = chroms_metadata[ii];
            wstring fname = L"", chr_filename = raw_dir;
            wstring key = L"Channel_";
            wstring value = L"Absorbance at ", label = L"";
            for (unsigned int jj = 0; jj<chroms_i.size(); jj++){
                std::pair<wstring, wstring> pair_j = chroms_i[jj];
                if (pair_j.first ==  L"channel"){
                    key.append(pair_j.second); label.append(L"_" + pair_j.second);
                }
                else if (pair_j.first == L"wavelength"){
                    value.append(pair_j.second);  label.append(L"_" + pair_j.second);
                }
                else if (pair_j.first == L"filename"){
                    fname = pair_j.second; chr_filename.append(fname); chr_filename.append(L".DAT");
                }
            }
            int chro_file_size = (int)getFileSize(chr_filename);
            if (chro_file_size<0){
                std::cout << "can't open uv_trace file = " << chr_filename.c_str() << "\"" << endl; return false;
            }
            if (chro_file_size == 0){
                std::cout << "empty uv_trace file = " << chr_filename.c_str() << "\"" << endl; return false;
            }
            if (verbose==1) std::wcout << "uv_trace file [" << fname.substr(5,3) << "] contains " << chro_file_size << " bytes" << endl;

            FILE* chroFile = NULL;
            errno_t err = _wfopen_s(&chroFile, chr_filename.c_str(), L"rbS");
            if (err != 0){
                std::cout << "can't open uv_trace file = " << chr_filename.c_str() << "\"" << endl; return false;
            }

            unsigned char * chro_buf = (unsigned char *)malloc(chro_file_size*sizeof(unsigned char));
            if (chro_buf == NULL){
                std::cout << "mz buf alloc failed" << endl; fclose_safe(chroFile); return false;
            }

            size_t tic_count = fread(chro_buf, 1, chro_file_size, chroFile);
            if (tic_count<(size_t)chro_file_size){
                std::cout << "can't read from uv_trace file = " << chr_filename.c_str() << endl;
            }
            unsigned short* ptc = (unsigned short*)chro_buf;

            unsigned short offset = *ptc;
            unsigned int size = (chro_file_size - offset) / 8;

            double * tm_buf = (double *)malloc(size*sizeof(double)); //temp hack double instead of float
            if (tm_buf == NULL){
                std::cout << "tm buf alloc failed" << endl; sql_out->closeDatabase(); delete sql_out; free_safe(tm_buf); free_safe(chro_buf); ptc = NULL; fclose_safe(chroFile); return false;
            }
            float * tc_buf = (float *)malloc(size*sizeof(float));
            if (tc_buf == NULL){
                std::cout << "tc buf alloc failed" << endl; sql_out->closeDatabase(); delete sql_out; free_safe(tm_buf); free_safe(chro_buf); ptc = NULL; fclose_safe(chroFile); return false;
            }

            float* ptd = (float*)(chro_buf + offset);
            for (unsigned int jj = 0; jj<size; jj++){
                tm_buf[jj] = (double)*ptd++; tc_buf[jj] = *ptd++; //temp hack double instead of float
            }

            sqlite3_stmt *stmt1 = NULL; bool success1 = false; int rc1;
            wostringstream cmd1; cmd1 << L"INSERT into Chromatogram(FilesId,Identifier,ChromatogramType,DataX,DataY,DataCount,MetaText) values ('"
                << FilesId << L"','" << L"Trace" << (chrom_base + ii) << (label.length()>0 ? label : L"") << L"','" << "absorption chromatogram" << L"',?,?,'" << size << L"','" << L"<RetentionTimeUnit>minute</RetentionTimeUnit>" << L"')";
            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
            if ((rc1 = sqlite3_prepare_v2(db_out, conv.to_bytes(cmd1.str()).c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                //sqlite3_bind_blob(stmt1, 1, tm_buf, tic[trace].size()*4, SQLITE_STATIC); 
                sqlite3_bind_blob(stmt1, 1, tm_buf, size * 8, SQLITE_STATIC); //temp hack double instead of float
                sqlite3_bind_blob(stmt1, 2, tc_buf, size * 4, SQLITE_STATIC);
                rc1 = sqlite3_step(stmt1);
                if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                    success1 = true;
                }
            }
            sqlite3_finalize(stmt1); free_safe(tc_buf); free_safe(tm_buf);
            if (!success1){
                if (verbose == 1) std::cout << "rc1=" << rc1 << ": can't write peaks data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl;
                sql_out->closeDatabase(); delete sql_out; free_safe(chro_buf); fclose_safe(chroFile); return false;
            }

            free_safe(chro_buf); fclose_safe(chroFile);
        }
    } else
        delete utl;

    sqlite3_exec(db_out, "COMMIT;", NULL, NULL, NULL);
    sql_out->closeDatabase(); delete sql_out;
    if (verbose == 1){
        std::cout << "all done!" << endl;
    }
    return true;
}


// read chro metadata
bool Reader::readChromsMetadata(const wstring &raw_dir, std::vector<std::vector<std::pair<std::wstring, std::wstring>>> &chroms_metadata)
{
    wstring chr_filename = raw_dir + L"_CHROMS.INF";
    int chro_file_size = (int)getFileSize(chr_filename);
    if (chro_file_size<0){
        std::wcout << "can't open chroms file = " << chr_filename << "\"" << endl; return false;
    }
    if (chro_file_size==0){
        std::wcout << L"empty chroms file = " << chr_filename.c_str() << L"\"" << endl; return false;
    }
    if (verbose==1) std::cout << "chroms file contains " << chro_file_size << " bytes" << endl;

    FILE* chroFile = NULL;
    errno_t err = _wfopen_s(&chroFile, chr_filename.c_str(), L"rbS");
    if (err != 0){
        std::cout << "can't open chroms file = " << chr_filename.c_str() << "\"" << endl; return false;
    }

    unsigned char * chro_buf = (unsigned char *)malloc(chro_file_size*sizeof(unsigned char)); 
    if (chro_buf==NULL){
        std::cout << "mz buf alloc failed" << endl; fclose_safe(chroFile); return false;
    } 

    size_t tic_count = fread(chro_buf, 1, chro_file_size, chroFile); 
    if (tic_count<(size_t)chro_file_size){
        std::cout << "can't read from chroms file = " << chr_filename.c_str() << endl; 
    }
    unsigned short* ptc = (unsigned short*)chro_buf;

    unsigned short offset = *ptc; 
    unsigned short inc = *(unsigned short*)(chro_buf+4);
    unsigned int size = (chro_file_size - offset)/inc;
    for (unsigned int ii=0; ii<size; ii++){
        unsigned char* ptd = chro_buf+offset+ii*inc;
        //int type = *(int *)ptd;
        ptd += 4;
        wstring property;
        for (int jj=0; jj<inc; jj++){
            char cc = *ptd++;
            if (cc==0)
                break;
            property += cc;
        }
        wstring value;
        for (int jj=0; jj<inc; jj++){
            char cc = *ptd++;
            if (cc==0)
                break;
            value += cc;
        }
        wstring chan = L"";
        std::size_t found = findChromatogramChannelName(property, &chan);

        if (found!=std::string::npos){
            if (verbose==1) std::cout << "'uv_trace' found at chan=" << (ii+1) << '\n';
            //Trace0_FLR_ChA_Ex330,Em420_nm
            //E.g. chan = "TUV_Ch", property = "xxxxxxA Ex330,Em420_nm"
            //output chan = "TUV_ChA"
            if (property.length()>found+6)
                chan.append(property.substr(found+6,1)); wstring val;
            std::size_t found2 = property.find(L"nm");
            if (found2 != std::string::npos){
                val = property.substr(found + 8, found2);
                std::replace(val.begin(), val.end(), ' ', '_');
            }
            wstring name_prefix = L"_CHRO";
            wstring name = appendZeroPaddedNumberForWatersFile(name_prefix, ii + 1);
            std::vector<std::pair<wstring, wstring>> chroms_i;
            chroms_i.push_back(std::pair<wstring, wstring>((const wchar_t *)L"channel", L"" + chan));
            chroms_i.push_back(std::pair<wstring, wstring>((const wchar_t *)L"wavelength", val));
            chroms_i.push_back(std::pair<wstring, wstring>((const wchar_t *)L"filename", name));
            chroms_i.push_back(std::pair<wstring, wstring>(property, value));
            chroms_metadata.push_back(chroms_i);
        }
    }
    free_safe(chro_buf);
    fclose_safe(chroFile);
    return true;
}

// read index file
bool Reader::readIndexFile(const wstring &idx_filename)
{
    long long idx_file_size = getFileSize(idx_filename);

    if (idx_file_size<0){
        std::wcout << "can't open idx file = " << idx_filename << "\"" << endl; return false;
    }
    if (idx_file_size==0){
        std::wcout << "empty idx file = " << idx_filename << "\"" << endl; return false;
    }
    if (verbose==1) std::cout << "idx file contains " << idx_file_size << " bytes" << endl;
    long long idx_remain_count = idx_file_size;

    FILE* idxFile = NULL;
    errno_t err = _wfopen_s(&idxFile, idx_filename.c_str(), L"rbS");
    if (err != 0){
        wcout << "can't read idx file = " << idx_filename<< "\"" << endl; return false;
    }

    unsigned char * idx_buf = (unsigned char *)malloc(IDX_BUFLEN*sizeof(unsigned char)); 
    if (idx_buf==NULL){
        std::cout << "mz buf alloc failed" << endl; fclose_safe(idxFile); return false; //fclose_safe(idxFile); 
    } 

//    double t0 = ((double)0x2068b94bba02c718ULL); double tm0 = 10.0255671; // get t0,t1 from file
//    double t1 = ((double)0x20aef84da76a9118ULL); double tm1 = 10.042717;
//    double fact = (tm1-tm0)/(t1-t0), t0_sec = tm0;//*60.0; //min
//    double tk = t0 - tm0/fact;
//    long long vk = (long long&)tk;

    int max_scan = (int)idx_file_size/30; unsigned long long total_offset = 0;
    for (int scan=0; scan<max_scan; scan++){
        if (idx_remain_count<=0){
            std::cout << "zero bytes left in idx file..." << endl; break;
        }
        size_t idx_count = fread(idx_buf, 1, 30, idxFile); 
        if (idx_count<30){
            std::cout << "can't read idx data from file = " << idx_filename.c_str() << endl; free_safe(idx_buf); fclose_safe(idxFile); return false; //fclose_safe(idxFile);
        }
        idx_remain_count -= idx_count; 
        unsigned char* ptx = (unsigned char *)idx_buf;

        unsigned int bb = *ptx++;
        unsigned int bb1 = *ptx++;
        unsigned int bb2 = (*ptx++ & 0x3f);
        unsigned int val1 = (bb<<16) | (bb1<<8) | bb2; // 3 zeros
        if (val1!=0){
            std::cout << "non-zero leading value =" << val1 << ", at scan=" << scan << endl; 
        }
        unsigned int size = *(unsigned int *)ptx; size>>=8; ptx+=4; // num of points //7

        unsigned int tbd0 = *(unsigned int *)ptx; ptx+=4;  // 11
        unsigned char cx = *ptx++; 
        unsigned int timei = *(unsigned int *)ptx; ptx+=4;    //16
        //double time = t0_sec+(timei-t0)*fact*60.0;
        float rt_time = (float&)timei;

        //bb = *ptx++;
        //bb1 = *ptx++;
        //bb2 = *ptx++;
        //unsigned int tbd1 = (bb<<16) | (bb1<<8) | bb2;           
        unsigned short tbd1 = *(unsigned short *)ptx; ptx+=2;  // 18
        unsigned int tbd2 = *(unsigned int *)ptx; ptx+=4;        // 22

        unsigned long long offset = *(unsigned long long *)ptx; ptx+=8; // 30
        if (offset != total_offset){
            std::cout << "offset mismatch =" << offset << ", at scan=" << scan << endl; 
        }
        total_offset += size*8;  // 8 bytes mz+intens

        std::cout<<dec<<"s="<<scan<<hex<<": b0=0x"<<tbd0<<", c="<<(int)cx<<", d=0x"<<tbd1<<", e=0x"<<tbd2<<";  n="<<dec<<size<<", of="<<hex<<offset<<", t="<<dec<<rt_time<<endl;

        scan+=0;
    }

    free_safe(idx_buf); fclose_safe(idxFile); //fclose_safe(idxFile);
    return true;
}



// compress file
bool Reader::compressType1(const wstring &in_filename, int* mz_len11, unsigned char* compressed, unsigned int* compressed_length)
{
    long long total_count = getFileSize(in_filename)/8, remain_count = total_count;
    if (total_count<0){
        wcout << "can't find input file=" << in_filename << endl; return false;
    }

    ifstream infile;
    infile.open (in_filename, ios::in | ios::binary); 
    if (!infile.good()){
        wcout << "can't read input file="  << in_filename << endl; return false;
    }

    //std::vector<unsigned int> mz_data, intens_data;
    unsigned char * buf = new unsigned char[BUF_LEN];
    unsigned int* pti = (unsigned int *)buf;

    std::vector<unsigned int> mz_dif, intensity_index, hcnts; mz_dif.push_back(0);     
    std::vector<unsigned int> hvals, hop_dels, hop_cnts, hop_vals; std::vector<CntRecord> intens_recs, hop_recs;
    map<unsigned int, unsigned int> hmap, hop_map; 

    //
    //while (remain_count>0){
        int size = (int)min((long long)BUF_LEN, remain_count);
        infile.read((char *)buf, size); remain_count -= BUF_LEN;

        mz_dif.clear(); intensity_index.clear();
        unsigned int intens0=*pti++, mz0 = *pti++, prev = mz0, next_mz0 = 0, next_intens = 0; //pdel = 0,
        //unsigned int scale0 = intens0&0xFFFF;
        intens0 >>= 16;
        hmap.clear(); hcnts.clear(); hvals.clear(); hop_map.clear(); hop_cnts.clear(); hop_vals.clear();
        int ii=1;
        for ( ;  ; ii++){
            unsigned int intens = *pti++, mz = *pti++;
            if (pti >= (unsigned int *)buf+size){ // rload buf
                infile.read((char *)buf, size); remain_count -= BUF_LEN;
                pti = (unsigned int *)buf;
            }

            if (ii>200 && mz==mz0){
                next_mz0 = mz; next_intens = intens;
                break;  // end of scan
            }

            unsigned char cc = intens & 0xFF;
            if (cc>0){
                std::cout << "non zero intensity=" << hex << cc << " spare at ii=" << ii << dec << endl; delete[] buf; return false;
            }  intens >>= 8; 
            intensity_index.push_back(intens); // no zero skip

            unsigned int dif = mz - prev; mz_dif.push_back(dif); 
            if (dif>0x01000000)
                ii+=0;
            prev = mz; 

            unsigned int id;
            map<unsigned int, unsigned int>::iterator itr = hmap.find(dif);
            if (itr == hmap.end()) {
                id = static_cast<unsigned int>(hmap.size());
                hmap.insert(std::pair<unsigned int, unsigned int>(
                    dif, static_cast<unsigned int>(hmap.size())));
                hcnts.push_back(1);
                hvals.push_back(dif);
            } else {
                id = itr->second;
                hcnts[id] += 1;
            }

            ii+=0; //intensity
            map<unsigned int, unsigned int>::iterator itrh = hop_map.find(intens);
            if (itrh == hop_map.end()) {
                id = static_cast<unsigned int>(hop_map.size());
                hop_map.insert(std::pair<unsigned int, unsigned int>(
                    intens, static_cast<unsigned int>(hop_map.size())));
                hop_cnts.push_back(1);
                hop_vals.push_back(intens);
            } else {
                id = itrh->second;
                hop_cnts[id] += 1;
            }

            ii+=0; //intensity
        }
        intens_recs.clear(); //unsigned int pvali = 0;
        for (unsigned int ii=0; ii<hcnts.size(); ii++)
            intens_recs.push_back(CntRecord(hcnts[ii], hvals[ii], ii));
/*        sort(intens_recs.begin(), intens_recs.end(), sort_by_value1);
        for (int ii=0; ii<(int)intens_recs.size(); ii++){
            unsigned int vali = intens_recs[ii].value;
            if (vali%8 !=0)
                ii+=0;
            if (vali != pvali+8)
                std::cout << ii << ": " << vali << ", d=" << (vali - pvali-8) << endl;
            pvali = vali;
            if (ii>100)
                ii+=0;
        }*/
        sort(intens_recs.begin(), intens_recs.end(), sort_by_count1);

        int mz_len1 = ii; *mz_len11 = mz_len1;
        map<double, unsigned short> hmp6; 
        for (unsigned int ii=0; ii<intens_recs.size(); ii++)
            hmp6.insert(std::pair<double, unsigned short>(intens_recs[ii].value, static_cast<unsigned short>(hmp6.size())));

        std::vector<unsigned short> indices;
        for (int ii = 1; ii<mz_len1; ii++){
            double dif = mz_dif[ii];
            unsigned short idx = hmp6[dif];
            indices.push_back(idx);
        }

        std::vector<unsigned int> seq7; unsigned short prev7 = 0; //unsigned int previ = 0;
        for (int ii=1; ii<mz_len1-3; ii++){
            unsigned short idx = indices[ii], idx1 = indices[ii+1], idx2 = indices[ii+2];
            if (idx==idx1 && idx1==idx2 && idx!=prev7){
                if (seq7.size()>0) 
                    seq7.push_back(ii); 
                seq7.push_back(idx); prev7=idx; ii+=2;
            }
        }
        seq7.push_back(mz_len1-1); 


        // ----------- compress scan -----------------
        //*compressed = (unsigned char*)malloc(mz_len1*sizeof(double)); 
        //if (compressed == NULL){
        //    std::cout << "compressed m/z buf allocation failed" << endl; return false;
        //}

        // ----- dict ---------
        *(unsigned int*)(compressed+10) = mz_len1;
        *(double*)(compressed+14) = mz0;
        unsigned char* ptr = compressed+22;
        for (unsigned int ii=0; ii<intens_recs.size(); ii++){
            unsigned short vali = (unsigned short)(intens_recs[ii].value*32768); 
            *(unsigned short*)ptr = vali; ptr+=2; 
        }
        unsigned short dict_len = (unsigned short)(ptr - compressed - 22);
        *(unsigned short*)compressed = dict_len;

        // -------- base -------
        std::vector<unsigned int> base; unsigned int cur7 = 0; 
        for (unsigned int ii=0; ii<seq7.size()/2; ii++){
            unsigned int vali = seq7[2*ii], loci = seq7[2*ii+1]; 
            *ptr++ = (unsigned char)vali; // set vali
            unsigned int cnt7 = loci - cur7; 
            *ptr++ = (unsigned char)(cnt7 >> 8); // set loc (16 bits)
            *ptr++ = (unsigned char)(cnt7 & 0xFF);
            for (unsigned int jj=0; jj<cnt7; jj++)
                base.push_back(vali); 
            cur7 = loci;
        }
        unsigned int base_len = static_cast<unsigned int>(ptr - compressed) - dict_len - 22;
        *(unsigned short*)(compressed+2) = (unsigned short)base_len;

        // -------- indices --------
        std::vector<std::vector<unsigned int>> array(intens_recs.size());
        for (unsigned int ii=0; ii<indices.size(); ii++){
            unsigned short idx = (unsigned short)indices[ii], idx1 = (unsigned short)(base[ii]);
            if (idx!=idx1)
                array[idx].push_back(ii);
        }

        // -------- hfr --------
        for (int ii=0; ii<min(256,(int)array.size()); ii++){ // series of skip n write (1-3 byte per data)
            if (array[ii].size()>0){                        
                cur7 = 0; // set val = ii
                for (unsigned int jj=0; jj<array[ii].size(); jj++){
                    unsigned int loci = array[ii][jj], deli = loci - cur7; 
                    if (deli<127) {
                        *ptr++ = (unsigned char)(deli | 0x80); // set loc (8 bits)
                    } else if (deli<16383){
                        *ptr++ = (((deli>>8)&0xFF) | 0x40); // set loc (16 bits)
                        *ptr++ = deli&0xFF;
                    } else {
                        *ptr++ = (unsigned char)(deli >> 16);  // set loc (24 bits)
                        *ptr++ = (deli>>8)&0xFF;  
                        *ptr++ = deli&0xFF; 
                    }
                    cur7 = loci; 
                }
                *ptr++ = 255; 
            } //else
                //ii+=0;
        }
        unsigned int hfr_len = static_cast<unsigned int>(ptr - compressed) - base_len - dict_len - 22;
        *(unsigned int*)(compressed+4) = hfr_len;
                
        // -------- lfr --------
        for (unsigned int ii=256; ii<intens_recs.size(); ii++){ // series of skip n write val (1 byte per data)
            if (array[ii].size()>0){
                for (unsigned int jj=0; jj<array[ii].size(); jj++){
                    unsigned int loci = array[ii][jj];    
                    unsigned int vali = loci>>16;
                    if (jj==array[ii].size()-1)
                        vali |= 0x80;
                    *ptr++ = (unsigned char)vali; // set loc (16 bits)
                    *ptr++ = (loci>>8)&0xFF;                         
                    *ptr++ = loci&0xFF;                         
                }
            } //else
                //ii+=0;
            //ii+=0;
        }
        unsigned int lfr_len = static_cast<unsigned int>(ptr-compressed)-hfr_len-base_len-dict_len-22;
        *(unsigned short*)(compressed+8) = (unsigned short)lfr_len;
                
        *compressed_length = (unsigned int)(ptr - compressed);





        // ================= intensity =====================
        hop_recs.clear();
        for (unsigned int ii=0; ii<hop_cnts.size(); ii++)
            hop_recs.push_back(CntRecord(hop_cnts[ii], hop_vals[ii], ii)); 
        sort(hop_recs.begin(), hop_recs.end(), sort_by_count1);



    //}*/
    infile.close(); 

    //std::vector<double> data;
    //string out_filename = in_filename + "_out.bin";

    

/*
    unsigned int* ptr = (unsigned int *)buf;
    unsigned int intens0=*ptr++, mz0 = *ptr++, prev = mz0, pdel = 0;
    unsigned int scale0 = intens0&0xFFFF; intens0 >>= 16;
    std::cout << dec << setfill('0') << setw(6) << 0 << ": " << hex << setfill('0') << setw(8) << mz0 << "  " << setfill('0') << setw(4) << intens0 << "  " << setfill('0') << setw(4) << scale0 << ";" << endl;
    for (int ii=1; ii<233610*2; ii++){
        unsigned int val = *ptr++, mz = *ptr++;
        unsigned short intens = val>>16, scale = val&0xFFFF;
        unsigned int del = mz - prev;
        //cout << dec << setfill('0') << setw(6) << ii << ": " << hex << setfill('0') << setw(8) << mz << "  " << setfill('0') << setw(4) << intens << "  " << setfill('0') << setw(4) << scale << "; " << setfill(' ') << setw(4) << del << endl;
        if (ii%100==0)
            std::cout << dec << ii << endl;
        prev = mz; 
        if (ii==1) pdel = del;
        else {
            if (del != pdel)
                ii+=0;
        }

        if ((scale&0xFF)>0)
            ii+=0;

        if (ii>200 && ((mz>>16)==0x4e40))
            ii+=0;

        if (mz==mz0)
            ii+=0;

        if (ii>10) //7534)
            ii+=0;
    }
*/

    delete[] buf;
    return true;
}


// read UV traces
bool Reader::readUvTraces(const wstring &filename, int channels, std::vector<std::vector<int>> &uv_trace, int* length)
{
    uv_trace.resize(channels);
    ifstream myfile;
    myfile.open (filename, ios::in | ios::binary); 
    myfile.seekg(0, myfile.end);
    int flength = (int)myfile.tellg(); *length = flength/12;
    myfile.seekg(0, myfile.beg);
    unsigned char * buf = new unsigned char[flength];
    myfile.read((char *)buf, flength);
    myfile.close();
    if (verbose==1) std::cout << "read " << myfile.gcount() << " chars" << endl;

    unsigned char * ptr = buf;
    for (int ii=0; ii<*length; ii++){
        if (ii>258)
            ii+=0;
        if (verbose==1) std::cout << "ii=" << ii;
        for (int jj=0; jj<channels; jj++){            
                unsigned int ptr0 = ptr[0], ptr1 = ptr[1], ptr2 = ptr[2], ptr3 = ptr[3]; 
                std::cout << ", in=0x" << hex << ptr2 << "," << ptr3 << "," << ptr1 << "," << ptr0 << dec;
                unsigned short val = (unsigned short)((ptr1 << 8) | ptr0);
                int intens = (int)((short)val);  
                short frq_scale = (short)(ptr2>>4) - 8;
                int intens_scale = ptr2&0x0F;
                if (intens_scale>1)
                    ii+=0;
                if (intens_scale>0)
                    intens *= intens_scale*4;
                if (intens < 0) 
                    ii+=0;
                    //intens -= 65536;// - intens;
                unsigned int mag = (((unsigned int)ptr[4])<<8) | ptr[5]; ptr += 6;
                unsigned int freq = (unsigned int)(pow(2.0,frq_scale)*mag);
                if (ii==0){
                    uv_trace[jj].push_back(freq); 
                    if (verbose==1)
                        //if (jj==0) std::cout << ",  freq" << (char)('A'+jj) << "=" << freq << ", i" << (char)('A'+jj) << "=" << intens << ";";
                        //else 
                        std::cout << ",  freq" << (char)('A'+jj) << "=" << freq << ", i" << (char)('A'+jj) << "=" << intens << ";";
                } else {
                    if (verbose==1) std::cout << ",  freq" << (char)('A'+jj) << "=" << freq << ", i" << (char)('A'+jj) << "=" << intens << ";";
                }
                uv_trace[jj].push_back(intens);
                if (ii>5000)
                    ii+=0;
            }
            if (verbose==1) std::cout << endl;
            if (ii>5100)
                ii+=0;
        }
    delete[] buf;
    return true;
}

// read UV traces
bool Reader::readUvIdx(const wstring &filename, int channels, std::vector<std::vector<unsigned long long>> &uv_trace, int* length)
{
    uv_trace.resize(channels);
    ifstream myfile;
    myfile.open (filename, ios::in | ios::binary); 
    myfile.seekg(0, myfile.end);
    int flength = (int)myfile.tellg(); *length = flength/22;
    myfile.seekg(0, myfile.beg);
    unsigned char * buf = new unsigned char[flength];
    myfile.read((char *)buf, flength);
    myfile.close();
    if (verbose==1) std::cout << "read " << myfile.gcount() << " chars" << endl;

    unsigned char * ptr = buf;
    for (int ii=0; ii<*length; ii++){
        //unsigned char * ptr = buf+22*ii*channels; _byteswap_ulong() _byteswap_ushort
        ostringstream data; data << "ii=" << ii << ":  freqA=" ;
        for (int jj=0; jj<channels; jj++){
            ptr += 22*jj+22*16;
            int scale = (int)(ptr[2]>>4) - 8;
            //unsigned int val2 = *(unsigned int *)ptr;
            ptr+=4;
            //unsigned short val3 = *(unsigned short *)ptr;
            ptr+=2;

            //unsigned long long val = *(unsigned long long *)ptr;
            //unsigned long long val1a = _byteswap_uint64(val);
            //double dval = *(double *)ptr;
            ptr+=8;
            //double dvala = (double)val1a;

            //unsigned long long val1 = *(unsigned long long *)ptr;
            //unsigned long long val1b = _byteswap_uint64(val1);
            //double dval1 = *(double *)ptr;
            ptr+=8;
            //double dval1b = (double)val1b;

            unsigned long long ptr0 = ptr[0], ptr1 = ptr[1], ptr2 = ptr[2], ptr3 = ptr[3];
            unsigned long long intens = ((ptr2 & 0xF)<<24) | (ptr3<<16) | (ptr1<<8) | ptr0;  
            unsigned int mag = (((unsigned int)ptr[4])<<8) | ptr[5];
            unsigned int freq = (unsigned int)(pow(2.0,scale)*mag);
            if (ii==0){
                uv_trace[jj].push_back(freq); data << freq << ", iA=" << intens;
            } else {
                data << ";  freq" << 'A'+jj << "=" << freq << ", i" << 'A'+jj << "=" << intens;
            }
            uv_trace[jj].push_back(intens);
        }
        if (verbose==1) std::cout << data.str() << endl;

        if (ii>30)
            ii+=0;
    }
    
    delete[] buf;
    return true;
}

// test function type
int Reader::testCompressFunctionType(const wstring &filename)
{
    long long remain_count = getFileSize(filename);
    if (remain_count<64){
        return -1;
    }

    std::vector<unsigned int> mzs, intensity;
    unsigned char * buf = (unsigned char *)malloc(64*sizeof(unsigned char)); 
    if (buf==NULL){
        std::cout << "read buf alloc failed" << endl; return -1;
    }
    unsigned int* pti = (unsigned int *)buf; 

    FILE* inFile = NULL; 
    errno_t err = _wfopen_s(&inFile, filename.c_str(), L"rbS");
    if (err != 0){
        std::cout << "can't open file" << endl; free_safe(buf); return -1;
    }

    size_t count = fread(buf, 1, 64LL, inFile); 
    if (count<64){
        std::cout << "can't read data from file" << filename.c_str() << endl; free_safe(buf); fclose_safe(inFile); return -1;
    }

    for (int ii=0; ii<8; ii++){
        unsigned int raw_intens = *pti++, mz = *pti++; 
        intensity.push_back(raw_intens); mzs.push_back(mz);
    }
    free_safe(buf); fclose_safe(inFile); //fclose_safe(inFile);

    unsigned int mz_prev = mzs[0], mzp_msd = mz_prev>>20;
    for (int ii=1; ii<8; ii++){
        unsigned int mzi = mzs[ii], mzi_msd = mzi>>20;
        if (mzi_msd<mzp_msd || mzi_msd>mzp_msd + 2){
            if (ii > 1){
                return 0;
            }
        }
        if (mzi <= mz_prev){
            return 0;
        }
        mz_prev = mzi_msd, mzp_msd = mzi_msd;
    }
    return 1;
}

// identify function type: 0=Undetermined, 1=MSx, 2=Other(e.g., UVTrace), 3=REF, -1=error
//Note(Yong): We will try and avoid names that conflict with std, such as 'function'; we will use func_val instead.
int Reader::identifyFunctionType(unsigned int func_val, const WatersMetaReading & watersMeta)
            //, string /*raw_dir*/, std::vector<string> /*list*/, std::vector<std::vector<string>> /*info*/, std::vector<string> /*functions*/, int /*idx_size*/, bool /*ms_type_6*/)
{
    if (func_val < 1) {
        std::cout << "out of range func_val=" << func_val << endl; return -1;
    }

    int mslevel = watersMeta.getMSLevelForFunction(func_val);
    if (mslevel < 0) {
        std::cout << "MS level unknown=" << mslevel << endl;
    }

    return mslevel;
}


// identify ms type: 0=normal, 1=ms_type_6
int Reader::identifyFunctionMsType(const wstring &raw_dir, int /*idx_size*/)
{
    unsigned int function = 1;
    wstring name_prefix(L"_FUNC");
    wstring name = appendZeroPaddedNumberForWatersFile(name_prefix, function);

    wstring name1 = name; name1.append(L".DAT");
    wstring filename = raw_dir + name1;

    wstring idx_name = name; idx_name.append(L".IDX");
    wstring idx_filename = raw_dir + idx_name;
    int idx_file_size = (int)getFileSize(idx_filename);
    if (idx_file_size<0){
        std::wcerr << "can't open idx file = " << idx_filename << "\"" << endl; return -1;
    }
    if (idx_file_size==0){
        std::wcerr << "empty idx file = " << idx_filename << "\"" << endl; return -1;
    }
    if (verbose==1) std::cout << "idx file contains " << idx_file_size << " bytes" << endl;

    FILE* idxFile = NULL;
    errno_t err = _wfopen_s(&idxFile, idx_filename.c_str(), L"rbS");
    if (err != 0){
        std::cout << "can't read idx file = " << idx_filename.c_str() << "\"" << endl; return -1;
    }

    unsigned char * idx_buf = (unsigned char *)malloc(idx_file_size*sizeof(unsigned char)); 
    if (idx_buf==NULL){
        std::cout << "mz buf alloc failed" << endl; fclose_safe(idxFile); return -1; //fclose_safe(idxFile); 
    } 

    size_t idx_count = fread(idx_buf, 1, idx_file_size, idxFile); 
    if (idx_count<(size_t)idx_file_size){
        std::cout << "can't read from idx file = " << idx_filename.c_str() << endl; std::cin.get(); //return false;
    }
    unsigned char* ptx = (unsigned char*)idx_buf;

    long long remain_count = getFileSize(filename);
    if (verbose==1) std::cout << "infile contains " << remain_count << " bytes" << endl;
    if (remain_count<0){
        std::cout << "can't find input file = \"" << filename.c_str() << "\"" << endl; free_safe(idx_buf); fclose_safe(idxFile); return -1;
    }
    if (remain_count==0){
        std::cout << "empty input file = \"" << filename.c_str() << "\"" << endl; free_safe(idx_buf); fclose_safe(idxFile); return -1;
    }
    fclose_safe(idxFile);

    FILE* inFile = NULL;
    err = _wfopen_s(&inFile, filename.c_str(), L"rbS");
    if (err != 0){
        std::cout << "can't open file" << endl; free_safe(idx_buf); fclose_safe(idxFile); return -1;
    }

    ptx+=4;
//    for (int scan=0; scan<max_scan; scan++){
        //int parent_scan_number = 0, parent_function = 1;
        unsigned int bb = *ptx++;
        unsigned int bb1 = *ptx++;
        unsigned int bb2 = (*ptx++ & 0x3f);
        unsigned int mz_len = (bb2<<16) | (bb1<<8) | bb; 

        size_t size = (size_t)2*mz_len*sizeof(unsigned int);
        unsigned char * buf = (unsigned char *)malloc(size*sizeof(unsigned char)); 
        if (buf==NULL){
            std::cout << "read buf alloc failed" << endl; fclose_safe(idxFile); fclose_safe(inFile); free_safe(idx_buf); return -1;
        }
        size_t count = fread(buf, 1, size, inFile); 
        if (count<size){
            std::cout << "can't read input data from file = \"" << filename.c_str() << "\"" << endl;
            free_safe(buf); fclose_safe(idxFile); fclose_safe(inFile); free_safe(idx_buf); ptx=NULL; return -1;
        }
        fclose_safe(inFile);

        bool ms_type_6 = true;
        unsigned char* pti2 = (unsigned char *)buf; int zero_intens_cnt = 0;
        std::vector<unsigned int> mz_data, intens_data; double prev_mz = 0; unsigned int prev_intens = 0;
        for (unsigned int ii=0; ii<mz_len; ii++){
            unsigned int intens = (unsigned int) *(unsigned short*)(pti2+6*ii); mz_data.push_back(intens); if (intens==0) zero_intens_cnt++;
            unsigned int mz = *(unsigned int*)(pti2+6*ii+2); intens_data.push_back(mz&0xFFFFFFF);
            unsigned int scl = mz & 0xF0;
            //cout << hex << "mz=" << mz << ", scl=" << scl << ", intens=" << intens << ", del=" << (mz-prev_mz) << endl;
            if (scl != 0x60 && scl != 0x70 && scl != 0x80 && scl != 0x90 && scl != 0xA0 && scl != 0xB0 && scl != 0xC0 && scl != 0xD0 && scl != 0xE0 && scl != 0xF0){
                ms_type_6=false; break;
            }
            double mzi = decodeMzType1_6(mz & 0xFFFFFFF0);
            if (mzi<prev_mz){
                ms_type_6 = false; break;
            } else {
                if (ii>0 && intens>0 && prev_intens == 0){
                    if ((mzi - prev_mz)>MIN_MS_TYPE6_DELTA_DBL){
                        ms_type_6 = false; break;
                    }
                }
            }
/*          unsigned int scl = mz & 0xFF; mz>>=8;
            if (scl==prev_scl){
                if (mz<prev_mz){
                    ms_type_6=false; break;
                } else {
                    if (ii>0 && intens>0 && prev_intens==0){
                        //cout << hex << "  del=" << ((int)mz-prev_mz) << endl; //<-- print deltas
                        if ((mz-prev_mz)>MIN_MS_TYPE6_DELTA){
                            int vvv = mz - prev_mz, vvv1 = MIN_MS_TYPE6_DELTA;
                            ms_type_6=false; break;
                        }
                    }
                }
            }*/
            prev_mz=mzi; prev_intens=intens;
        }
        if (!m_identifyFunctionMsTypeExpectNoZeros && mz_len>MS_TYPE6_CNT && zero_intens_cnt<(int)(mz_len / 3))
            ms_type_6=false; 

        free_safe(buf);
        free_safe(idx_buf); fclose_safe(idxFile); fclose_safe(inFile);
    //}
    return ms_type_6?1:0;
}

// identify function type v3c
int Reader::identifyFunctionType_V3C(const wstring &raw_dir, int func_id)
{
    wstring name_prefix(L"_FUNC");
    wstring name = appendZeroPaddedNumberForWatersFile(name_prefix, func_id);

    wstring name1 = name; name1.append(L".DAT");
    wstring filename = raw_dir + name1;

    wstring idx_name = name; idx_name.append(L".IDX");
    wstring idx_filename = raw_dir + idx_name;
    int idx_file_size = (int)getFileSize(idx_filename);
    if (idx_file_size<0){
        wcerr << "can't open idx file = " << idx_filename << "\"" << endl; return -1;
    }
    if (idx_file_size == 0){
        wcerr << "empty idx file = " << idx_filename << "\"" << endl; return -1;
    }
    if (verbose == 1) std::cout << "idx file contains " << idx_file_size << " bytes" << endl;

    if (idx_file_size < 8){
        wcerr << "too smal idx file = " << idx_filename << "\"" << endl; return -1;
    }

    FILE* idxFile = NULL;
    errno_t err = _wfopen_s(&idxFile, idx_filename.c_str(), L"rbS");
    if (err != 0){
        wcerr << "can't read idx file = " << idx_filename << "\"" << endl; return -1;
    }

    unsigned char * idx_buf = (unsigned char *)malloc(idx_file_size*sizeof(unsigned char));
    if (idx_buf == NULL){
        std::cout << "mz buf alloc failed" << endl; fclose_safe(idxFile); return -1;
    }

    size_t idx_count = fread(idx_buf, 1, idx_file_size, idxFile);
    if (idx_count<(size_t)idx_file_size){
        std::cout << "can't read from idx file = " << idx_filename.c_str() << endl; std::cin.get(); //return false;
    }
    unsigned char* ptx = (unsigned char*)idx_buf;

    long long remain_count = getFileSize(filename);//, file_size = remain_count/8;
    if (verbose == 1) std::cout << "infile contains " << remain_count << " bytes" << endl;
    if (remain_count<0){
        std::cout << "can't find input file = \"" << filename.c_str() << "\"" << endl; free_safe(idx_buf); fclose_safe(idxFile); return -1;
    }
    if (remain_count == 0){
        std::cout << "empty input file = \"" << filename.c_str() << "\"" << endl; free_safe(idx_buf); fclose_safe(idxFile); return -1; //fclose_safe(idxFile); 
    }
    fclose_safe(idxFile);

    FILE* inFile = NULL;
    err = _wfopen_s(&inFile, filename.c_str(), L"rbS");
    if (err != 0){
        std::cout << "can't open file" << endl; free_safe(idx_buf); fclose_safe(idxFile); return -1;
    }

    ptx += 4;
    unsigned int bb = *ptx++;
    unsigned int bb1 = *ptx++;
    unsigned int bb2 = (*ptx++ & 0x3f);
    unsigned int mz_len = (bb2 << 16) | (bb1 << 8) | bb;

    if (mz_len >= 1E7){
        std::cout << "mz_len exceeded max range for file = \"" << filename.c_str() << "\"" << endl;  //<==============
        fclose_safe(idxFile); fclose_safe(inFile); free_safe(idx_buf); ptx = NULL; return -1; //fclose_safe(inFile); 
    }

    size_t size = (size_t)3 * mz_len*sizeof(unsigned int);
    unsigned char * buf = (unsigned char *)malloc(size*sizeof(unsigned char));
    if (buf == NULL){
        std::cout << "read buf alloc failed" << endl; fclose_safe(idxFile); fclose_safe(inFile); free_safe(idx_buf); return -1; //fclose_safe(inFile); 
    }
    size_t count = fread(buf, 1, size, inFile);
    if (count<size){
        std::cout << "can't read input data from file = \"" << filename.c_str() << "\"" << endl;  //<==============
        free_safe(buf); fclose_safe(idxFile); fclose_safe(inFile); free_safe(idx_buf); ptx = NULL; return -1;//fclose_safe(inFile); 
    }
    fclose_safe(inFile); //fclose_safe(inFile); 

    bool v_type_v3c = true;
    std::vector<unsigned int> v3c_tab = buildV3CLookupTable(25);
    unsigned int* pti2 = (unsigned int *)buf; 
    std::vector<double> mz_data, intens_data; double prev_mz = 0.0;
    for (unsigned int ii = 0; ii<mz_len; ii++){
        unsigned int raw_intens = *pti2++;
        double dbl_intensity = decodeIntensityType1_V3C(raw_intens, v3c_tab, false); intens_data.push_back(dbl_intensity);
        if (dbl_intensity == 0.0){
            v_type_v3c = false; break;
        }
        unsigned int decoded_intens = (unsigned int)(dbl_intensity + .5);
        if (decoded_intens == 0){
            v_type_v3c = false; break;
        }

        unsigned int mzi = *pti2++;
        double dbl_mzi = decodeMzType1_6_V3C(mzi); mz_data.push_back(dbl_mzi);
        if (dbl_mzi<32.0 || dbl_mzi >= 32768.0){
            v_type_v3c = false; break;
        }
        if (dbl_mzi<prev_mz){
            v_type_v3c = false; break;
        }

        //unsigned int tbdi = 
            *pti2++;
        prev_mz = dbl_mzi;
    }

    free_safe(buf);
    free_safe(idx_buf); fclose_safe(idxFile); fclose_safe(inFile);
    return v_type_v3c ? 1 : 0;
}

// 
/*!
 * @brief check sts_override returns true to use extra offset (for old machines, likely European micromass precursor mass assignments). 
   false does original offset.
 */
bool Reader::checkStsOverride(const unsigned char* pts, int sts_count, int sts_file_size, bool ms_type_6)
{
    bool bad_mz_found = false;
    if (sts_count == 0)
        return false;

    //Try the normal mode
    unsigned char* ptx = (unsigned char*)pts + (ms_type_6 ? 0x26 : 0x2a);
    if ((ptx - pts) > sts_file_size - 4)
        return false;

    for (int ii = 0; ii < min(50, sts_count); ii++){
        if ((ptx - pts) > sts_file_size-4)
            break;
        double precursor_mz = *(float*)ptx;
        if ((precursor_mz<MIN_PRECURSOR_MZ || precursor_mz>MAX_PRECURSOR_MZ) && precursor_mz != 0.0){
            bad_mz_found = true; break;
        }
        ptx += (ms_type_6 ? 0x76 : 0x95);
    }

    if (!bad_mz_found)
        return false;

    //Try with new offset
    int new_offset = (ms_type_6 ? 0x2a : 0x2e);
    ptx = (unsigned char*)pts + new_offset;
    if ((ptx - pts) > sts_file_size - 4)
        return false;

    for (int ii = 0; ii < min(50, sts_count); ii++){
        if ((ptx - pts) > sts_file_size-4)
            break;
        double precursor_mz = *(float*)ptx;
        //Try reading in this new offset mode.  If it still has bad values, it must not be a good offset.
        //Revert to our original offset by returning false.
        if ((precursor_mz<MIN_PRECURSOR_MZ || precursor_mz>MAX_PRECURSOR_MZ) && precursor_mz != 0.0){
            return false;
        }
        ptx += (ms_type_6 ? 0x7a : 0x99);
    }

    //We are confident that the new offset has good values.
    return true;
}

// check cal offset
bool Reader::checkCalOffset(FILE* inFile, unsigned int mz_len)
{
    int length = min(mz_len, 100U);
    size_t size = (size_t)length * 12 * sizeof(unsigned char);
    unsigned char * buf = (unsigned char *)malloc(size);
    if (buf == NULL){
        std::cout << "check cal buf alloc failed" << endl; return false;
    }

    bool success = true;
    size_t count = fread(buf, 1, size, inFile);
    if (count < size){
        std::cout << "caution: early term while check cal offset" << endl; success = false;
    }

    std::vector<double> mz_data; mz_data.reserve(length);
    if (success){
        unsigned int* pti = (unsigned int *)buf; pti++;
        for (int ii = 0; ii < length; ii++){
            unsigned int mz = *pti; pti += 3;
            double mzi = decodeMzType1(mz);
            if (mzi<MIN_PRECURSOR_MZ || mzi>MAX_PRECURSOR_MZ){
                success = false; break;
            }
            if (ii > 0 && mzi <= mz_data[ii - 1]){
                success = false; break;
            }
            mz_data.push_back(mzi);
        }
    }
    free_safe(buf); rewind(inFile);
    return success;
}

// find channel name
std::size_t Reader::findChromatogramChannelName(const std::wstring &property, std::wstring * chan) const
{
    chan->clear();
    std::size_t found = property.find(L"PDA Ch");
    if (found == std::string::npos){
        found = property.find(L"FLR Ch");
        if (found == std::string::npos){
            found = property.find(L"ELS Ch");
            if (found == std::string::npos){
                found = property.find(L"TUV Ch");
                if (found <= std::string::npos)
                    *chan = L"TUV_Ch";
            } else
                *chan = L"ELS_Ch";
        } else
            *chan = L"FLR_Ch";
    } else 
        *chan = L"PDA_Ch";

    return found;
}


// decode type1 intensity data
unsigned int Reader::decodeIntensityType1(unsigned int intensity)
{
    if (intensity==0)
        return 0;
    unsigned long long vali = intensity;
    //unsigned char ovf = (vali >> 28) & 0x07;; // second bit 0x20 ?
    unsigned char scale = (vali >> 24) & 0x07;
    unsigned long long lsw = vali & 0xFFFFFF;
    double result;

    if (scale==0){ ;
        //if (lsw<0x200000ULL)  //scl==0
        //    vali = (lsw - 0x100000ULL)/8192 + 0x0; 
        //else 
    if (lsw < 0x500000ULL)
            //return 0;
            result = (lsw - 0x100000ULL) / 2097152.0 + 0.5;
        else if (lsw<0x900000ULL) //scl==0
            result = (lsw - 0x500000ULL) / 1048576.0 + 0x1;
        else if (lsw<0xd00000ULL)
            result = (lsw - 0x900000ULL) / 524288.0 + 0x2;
        else
            result = (lsw - 0xd00000ULL) / 262144.0 + 0x4;

    } else if (scale==1){
        if (lsw<0x200000ULL)  //scl==1
            result = (lsw - 0x100000ULL) / 131072.0 + 0x8;
        else if (lsw<0x900000ULL) 
            result = (lsw - 0x500000ULL) / 65536.0 + 0x10;
        else if (lsw<0xd00000ULL)
            result = (lsw - 0x900000ULL) / 32768.0 + 0x20;
        else
            result = (lsw - 0xd00000ULL) / 16384.0 + 0x40;

    } else if (scale==2){
        if (lsw<0x200000ULL)  //scl==2
            result = (lsw - 0x100000ULL) / 8192.0 + 0x80;
        else if (lsw<0x900000ULL) 
            result = (lsw - 0x500000ULL) / 4096.0 + 0x100;
        else if (lsw<0xd00000ULL)
            result = (lsw - 0x900000ULL) / 2048.0 + 0x200;
        else
            result = (lsw - 0xd00000ULL) / 1024.0 + 0x400;

    } else if (scale==3){ // scl==3  
        if (lsw<0x200000ULL) 
            result = (lsw - 0x100000ULL) / 512.0 + 0x800;
        else if (lsw<0x900000ULL) 
            result = (lsw - 0x500000ULL) / 256.0 + 0x1000;
        else if (lsw<0xd00000ULL)
            result = (lsw - 0x900000ULL) / 128.0 + 0x2000;
        else
            result = (lsw - 0xd00000ULL) / 64.0 + 0x4000;

    } else if (scale==4){
        if (lsw<0x200000ULL)  //scl==4
            result = (lsw - 0x100000ULL) / 32.0 + 0x8000ULL;
        else if (lsw<0x900000ULL) 
            result = (lsw - 0x500000ULL) / 16.0 + 0x10000ULL;
        else if (lsw<0xd00000ULL)
            result = (lsw - 0x900000ULL) / 8.0 + 0x20000ULL;
        else
            result = (lsw - 0xd00000ULL) / 4.0 + 0x40000ULL;

    } else if (scale==5){
        if (lsw<0x200000ULL)  //scl==5
            result = (lsw - 0x100000ULL) / 2.0 + 0x80000ULL;
        else if (lsw<0x900000ULL) 
            result = (double)(lsw - 0x500000ULL) + 0x100000ULL;
        else if (lsw<0xd00000ULL)
            result = (lsw - 0x900000ULL)*2.0 + 0x200000ULL;
        else
            result = (lsw - 0xd00000ULL)*4.0 + 0x400000ULL;

    } else if (scale == 6){
        if (lsw<0x200000ULL)  //scl==6
            result = (lsw - 0x100000ULL)*8.0 + 0x800000ULL;
        else if (lsw<0x900000ULL)
            result = (lsw - 0x500000ULL)*16.0 + 0x1000000ULL;
        else if (lsw<0xd00000ULL)
            result = (lsw - 0x900000ULL)*32.0 + 0x2000000ULL;
        else
            result = (lsw - 0xd00000ULL)*64.0 + 0x4000000ULL;

    } else {
        // not supported
        //if (lsw<0x200000ULL)  //scl=0x2x
        std::cout << "unsupported scale=" << (int)scale << endl;
        result = (lsw - 0x100000ULL)*1.0 + 0x80000ULL;
    }
    unsigned int value = (unsigned int)result;
    double frac = result - value;
    if (frac >= 0.5){
        value++; // round up
    } else {
        if (value == 0 && frac > 1E-8)
            value++; // be sure to not drop non-zeros
    }
    //std::cout << (float)result << " --> " << value << endl;
    return value;
}

// build V3C lookup table
std::vector<unsigned int> Reader::buildV3CLookupTable(const unsigned int size)
{
    std::vector<unsigned int> v3c_tab; v3c_tab.push_back(4); v3c_tab.push_back(16);
    unsigned int cur = 16, base = 12, prod = 12;
    for (int ii = 1; ii < (int)size; ii++){
        base += 4;
        prod = base*(1 << ii);
        cur += prod; v3c_tab.push_back(cur);
        ii += 0;
    }
    return v3c_tab;
}

// build lookup tables
void Reader::buildLookupTables(std::vector<unsigned short> &dv_tab, std::vector<unsigned int> &mz_base, std::vector<double> &mz_tab)
{
    dv_tab.clear(); mz_base.clear(); mz_tab.clear();
    for (int ii = 0; ii < MIN_IDX; ii++)
        dv_tab.push_back(0);
    dv_tab.push_back(10264); dv_tab.push_back(7264); dv_tab.push_back(5152);
    dv_tab.push_back(3664); dv_tab.push_back(2592); dv_tab.push_back(1832); dv_tab.push_back(1296);
    dv_tab.push_back(912); dv_tab.push_back(648); dv_tab.push_back(456); dv_tab.push_back(320);

    //unsigned int mz_base[15] = { 0, 0, 0, 0, 738197504, 872415232, 1006632960, 1140850688, 1275068416, 1409286144, 1543503872, 1677721600, 1811939328, 1946157056 };
    for (int ii = 0; ii < MIN_IDX; ii++)
        mz_base.push_back(0);
    mz_base.push_back(738197504); mz_base.push_back(872415232); mz_base.push_back(1006632960); mz_base.push_back(1140850688);
    mz_base.push_back(1275068416); mz_base.push_back(1409286144); mz_base.push_back(1543503872); mz_base.push_back(1677721600);
    mz_base.push_back(1811939328); mz_base.push_back(1946157056); mz_base.push_back(0);

    mz_tab.resize(MAX_IDX * 3);
    double mz_tab2 = -1.03068049758955E-14, mz_tab1 = 4.66931570737946E-06, mz_tab0 = 647.990335979525, sqrt2 = sqrt(2.0);
    mz_tab[33] = mz_tab2; mz_tab[34] = mz_tab1; mz_tab[35] = mz_tab0;
    double mz2 = mz_tab2, mz1 = mz_tab1, mz0 = mz_tab0;
    for (int ii = 10; ii >= MIN_IDX; ii--){
        mz2 *= sqrt2; mz_tab[3 * ii] = mz2;
        mz1 *= sqrt2; mz_tab[3 * ii + 1] = mz1;
        mz0 *= sqrt2; mz_tab[3 * ii + 2] = mz0;
    }
    mz2 = mz_tab2; mz1 = mz_tab1; mz0 = mz_tab0;
    for (int ii = 12; ii < MAX_IDX; ii++){
        mz2 /= sqrt2; mz_tab[3 * ii] = mz2;
        mz1 /= sqrt2; mz_tab[3 * ii + 1] = mz1;
        mz0 /= sqrt2; mz_tab[3 * ii + 2] = mz0;
    }
}

// restore zeros
bool Reader::restoreZeros(unsigned int* packed_mz, int packed_mz_length, std::vector<unsigned short> &dv_tab, std::vector<unsigned int> &mz_base, std::vector<double> &mz_tab, std::vector<unsigned int> &restored_zeros_mz)
{
    restored_zeros_mz.clear();
    //std::vector<unsigned int> mzt, mztd, mzb;
    double mz_tab2 = 0.0, mz_tab1 = 0.0, mz_tab0 = 0.0; unsigned int m_base = 0; //unsigned short dvl = 0, dvh = 0; 
    unsigned int intensi = packed_mz[0], prev_mzi = packed_mz[1];
    int prev_mz_index = getMzIndex(prev_mzi);
    if (prev_mz_index <= 0){
        std::cout << "unsupported mz_index=" << prev_mz_index << endl; return false;
    }
    mz_tab2 = mz_tab[3 * prev_mz_index]; mz_tab1 = mz_tab[3 * prev_mz_index + 1]; mz_tab0 = mz_tab[3 * prev_mz_index + 2];
    m_base = mz_base[prev_mz_index]; //dvl = dv_tab[prev_mz_index + 1];  dvh = dv_tab[prev_mz_index]; 
    unsigned int pdi = prev_mzi - m_base;
    double pdv = (double)(mz_tab2 * pdi*pdi + mz_tab1 * pdi + mz_tab0);
    unsigned int prev_dvi = 8 * (unsigned int)(.5f + pdv / 8);

    restored_zeros_mz.push_back(0); restored_zeros_mz.push_back(prev_mzi - prev_dvi);
    restored_zeros_mz.push_back(intensi); restored_zeros_mz.push_back(prev_mzi);
    //std::vector<std::pair<unsigned int, unsigned int>> new_mzs;
    //new_mzs.push_back(std::pair<unsigned int, unsigned int>(prev_mzi - prev_dvi, 0)); // 1 prev zero
    //new_mzs.push_back(std::pair<unsigned int, unsigned int>(prev_mzi, intensi)); // current nz

    int mz_index = 0; //unsigned int max_dv_err = 0, ercnt = 0, dvi = 0;
    for (int ii = 1; ii < packed_mz_length; ii++){
        unsigned int intens = packed_mz[2 * ii], mzti = packed_mz[2 * ii + 1]; //mzt.push_back(mzti);

        mz_index = getMzIndex(mzti);
        if (mz_index <= 0){
            std::cout << "unsupported mz_index=" << prev_mz_index << endl; return false;
        }
        if (mz_index != prev_mz_index){
            mz_tab2 = mz_tab[3 * mz_index]; mz_tab1 = mz_tab[3 * mz_index + 1]; mz_tab0 = mz_tab[3 * mz_index + 2];
            m_base = mz_base[mz_index]; //dvh = dv_tab[mz_index]; dvl = dv_tab[mz_index + 1]; 
        }
        unsigned int di = mzti - m_base;
        double dv = (double)(mz_tab2 * di*di + mz_tab1 * di + mz_tab0);
        unsigned int dvi = 8 * (unsigned int)(.5f + dv / 8);

        //mzb.push_back(mzti);
        unsigned int delm = mzti - prev_mzi; //mztd.push_back(delm);
        //unsigned int facti = 5 * (dvi + prev_dvi);  // <===================== debug -- factor of 4?
        if (delm > 5 * (dvi + prev_dvi)){ // /4
            restored_zeros_mz.push_back(0); restored_zeros_mz.push_back(prev_mzi + prev_dvi);
            restored_zeros_mz.push_back(0); restored_zeros_mz.push_back(mzti - dvi);
            //new_mzs.push_back(std::pair<unsigned int, unsigned int>(prev_mzi + prev_dvi, 0)); // 1st zero 
            //new_mzs.push_back(std::pair<unsigned int, unsigned int>(mzti - dvi, 0)); // 2nd zero
        } else if (delm > 3 * (dvi + prev_dvi)){ // /4
            restored_zeros_mz.push_back(0); restored_zeros_mz.push_back((prev_mzi + mzti + 1) / 2);
            //new_mzs.push_back(std::pair<unsigned int, unsigned int>((prev_mzi + mzti + 1) / 2, 0)); // singl zero midway
        }
        restored_zeros_mz.push_back(intens); restored_zeros_mz.push_back(mzti);
        //new_mzs.push_back(std::pair<unsigned int, unsigned int>(mzti, intens)); // current nz

        prev_mzi = mzti; prev_dvi = dvi;
        if (mz_index != prev_mz_index){
            prev_mz_index = mz_index;
        }
    }
    mz_index = getMzIndex(prev_mzi);
    if (mz_index <= 0){
        std::cout << "unsupported mz_index=" << prev_mz_index << endl; return false;
    }
    if (mz_index != prev_mz_index){
        mz_tab2 = mz_tab[3 * mz_index]; mz_tab1 = mz_tab[3 * mz_index + 1]; mz_tab0 = mz_tab[3 * mz_index + 2];
        m_base = mz_base[mz_index]; //dvh = dv_tab[mz_index]; dvl = dv_tab[mz_index + 1]; 
    }
    unsigned int di = prev_mzi - m_base;
    double dv = (double)(mz_tab2 * di*di + mz_tab1 * di + mz_tab0);
    unsigned int dvi = 8 * (unsigned int)(.5f + dv / 8);
    restored_zeros_mz.push_back(0); restored_zeros_mz.push_back(prev_mzi + dvi);
    //new_mzs.push_back(std::pair<unsigned int, unsigned int>(prev_mzi + dvi, 0));
/*
    std::vector<unsigned int> mzq, intensqi;
    for (int ii = 0; ii < (int)new_mzs.size(); ii++){
        std::pair<unsigned int, unsigned int> pair = new_mzs[ii];
        mzq.push_back(pair.first); intensqi.push_back(pair.second);
    }*/
    return true;
}

// decode type1 intensity V3C
double Reader::decodeIntensityType1_V3C(unsigned int intens, const std::vector<unsigned int> &v3c_tab, const bool show_msg)
{
    if (intens < 4194304){
        if (show_msg) std::cout << "intensity below 0.5 for value=" << intens << endl; return 0.0;
    } 
    unsigned int clip = intens & 0xfffffff;
    if (clip >= 114294784){
        if (show_msg) std::cout << "unsupported intensity range=" << intens << endl; return 0.0;
    }
    unsigned int range = (unsigned int)((float)(clip >> 20) + 0.5f);
    int index = (range - 5) / 4;
    if (index<0 || index>(int)v3c_tab.size()){
        if (show_msg) std::cout << "unsupported v3c_index intens=" << intens << endl; return 0.0;
    }
    double vali = (double)clip;
    if (index <= 20){
        unsigned int scale = 1 << (20 - index);
        vali /= scale;
    } else {
        unsigned int scale = 1 << (index - 20);
        vali *= scale;
    }
    vali -= v3c_tab[index];
    return vali;
}


// decode type1 intensity vx
double Reader::decodeIntensityType1_V3C_old(unsigned int intens, const bool show_msg){
    double val = 0.0;
    intens &= 0xfffffff;

    if (intens < 4194304){
        if (show_msg) std::cout << "intensity below 0.5 for value=" << intens << endl;
    } else if (intens < 6291456){
        val = (double)intens / 1048576 - 4;
    } else if (intens < 10485760){
        val = (double)intens / 524288 - 16;
    } else if (intens < 14680064){
        val = (double)intens / 262144 - 48;
    } else if (intens < 18874368){
        val = (double)intens / 131072 - 128;
    } else if (intens < 23068672){
        val = (double)intens / 65536 - 320;
    } else if (intens < 27262976){
        val = (double)intens / 32768 - 768;
    } else if (intens < 31457280){
        val = (double)intens / 16384 - 1792;
    } else if (intens < 35651548){
        val = (double)intens / 8192 - 4096;
    } else if (intens < 39845888){
        val = (double)intens / 4096 - 9216;
    } else if (intens < 44040192){
        val = (double)intens / 2048 - 20480;
    } else if (intens < 48234496){
        val = (double)intens / 1024 - 45056;
    } else if (intens < 52428800){
        val = (double)intens / 512 - 98304;
    } else if (intens < 56623104){
        val = (double)intens / 256 - 212992;
    } else if (intens < 60817408){
        val = (double)intens / 128 - 458752;
    } else if (intens < 65011712){
        val = (double)intens / 64 - 983040;
    } else if (intens < 69206016){
        val = (double)intens / 32 - 2097152;
    } else if (intens < 73400320){
        val = (double)intens / 16 - 4456448;
    } else if (intens < 77594624){
        val = (double)intens / 8 - 9437184;
    } else if (intens < 81788928){
        val = (double)intens / 4 - 19922944;
    } else if (intens < 85983232){
        val = (double)intens / 2 - 41943040;
    } else if (intens < 90177536){
        val = (double)intens - 88080384;
    } else if (intens < 94371840){
        val = (double)intens * 2 - 184549376;
    } else if (intens < 98566144){
        val = (double)intens * 4 - 385875968;
    } else if (intens < 102760448){
        val = (double)intens * 8 - 805306368;
    } else if (intens < 106954752){
        val = (double)intens * 16 - 1677721600;
    } else if (intens < 111149056){
        val = (double)intens * 32 - 3489660928;
    } else {
        if (show_msg) std::cout << "unsupported intensity range=" << intens << endl;
    }
    return val;
}

// test decode intensity
bool Reader::test_decodeIntensity_v3c(unsigned int raw_intens, std::vector<unsigned int> &v3c_tab)
{
    double dbl_intensity1 = decodeIntensityType1_V3C(raw_intens, v3c_tab, true);
    double dbl_intensity2 = decodeIntensityType1_V3C_old(raw_intens, true);
    double diff = abs(dbl_intensity1 - dbl_intensity2);
    return diff < 1e-12;
}

// recode type1 intensity vx
unsigned int Reader::recodeIntensityType1_V3C(double dbl_intens, const bool show_msg){
    unsigned int val = 0;
    if (dbl_intens < 0.0){
        if (show_msg) std::cout << "negative intensity = " << setprecision(15) << dbl_intens << endl;
        return 1048576;
    } else if (dbl_intens == 0.0){
        val = 0; // (unsigned int)(2097152 * (dbl_intens + 1));
    } else if (dbl_intens < 1.0){
        val = (unsigned int)(2097152 * (dbl_intens + 2));
    } else if (dbl_intens < 2.0){
        val = (unsigned int)(1048576 * (dbl_intens + 4));
    } else if (dbl_intens < 4.0){
        val = (unsigned int)(524288 * (dbl_intens + 16));
    } else if (dbl_intens < 8.0){
        val = (unsigned int)(262144 * (dbl_intens + 48));
    } else if (dbl_intens < 16.0){
        val = (unsigned int)(131072 * (dbl_intens + 128));
    } else if (dbl_intens < 32.0){
        val = (unsigned int)(65536 * (dbl_intens + 320));
    } else if (dbl_intens < 64.0){
        val = (unsigned int)(32768 * (dbl_intens + 768));
    } else if (dbl_intens < 128.0){
        val = (unsigned int)(16384 * (dbl_intens + 1792));
    } else if (dbl_intens < 256.0){
        val = (unsigned int)(8192 * (dbl_intens + 4096));
    } else if (dbl_intens < 512.0){
        val = (unsigned int)(4096 * (dbl_intens + 9216));
    } else if (dbl_intens < 1024.0){
        val = (unsigned int)(2048 * (dbl_intens + 20480));
    } else if (dbl_intens < 2048.0){
        val = (unsigned int)(1024 * (dbl_intens + 45056));
    } else if (dbl_intens < 4096.0){
        val = (unsigned int)(512 * (dbl_intens + 98304));
    } else if (dbl_intens < 8192.0){
        val = (unsigned int)(256 * (dbl_intens + 212992));
    } else if (dbl_intens < 16384.0){
        val = (unsigned int)(128 * (dbl_intens + 458752));
    } else if (dbl_intens < 32768.0){
        val = (unsigned int)(64 * (dbl_intens + 983040));
    } else if (dbl_intens < 65536.0){
        val = (unsigned int)(32 * (dbl_intens + 2097152));
    } else if (dbl_intens < 131072.0){
        val = (unsigned int)(16 * (dbl_intens + 4456448));
    } else if (dbl_intens < 262144.0){
        val = (unsigned int)(8 * (dbl_intens + 9437184));
    } else if (dbl_intens < 524288.0){
        val = (unsigned int)(4 * (dbl_intens + 19922944));
    } else if (dbl_intens < 1048576.0){
        val = (unsigned int)(2 * (dbl_intens + 41943040));
    } else if (dbl_intens < 2097152.0){
        val = (unsigned int)(dbl_intens + 88080384);
    } else {
        if (show_msg) std::cout << "unsupported intensity range=" << dbl_intens << endl;
        val = 0;
    }
    return val;
}

// get mz index
int8_t Reader::getMzIndex(const unsigned int mzi)
{
    if (mzi == 0)
        return 0;
    int8_t val = ((mzi >> 26)-1)/2-1;
    if (val < MIN_IDX)
        return -1;
    if (val > MAX_IDX)
        return -2;
    return val;
}

// decode type1 Mz data
double Reader::decodeMzType1(unsigned int mz)
{
    if (mz==0)
        return 0;
    double val;
    if (mz<0x34000000UL) {
        val = 32.0;  // clip lower limit at 32.0
    } else if (mz<0x3C000000UL){
        val = mz/2097152.0 - 384.0;//166076660156;  32-64
    } else if (mz<0x44000000UL){
        val = mz/1048576.0 - 896.0;//166076660156;  64-128
    } else if (mz<0x4C000000UL){
        val = mz/524288.0 - 2048.0;//1727142334;  128-256
    } else if (mz<0x54000000UL){
        val = mz/262144.0 - 4608.0;//16940307617;  256-512
    } else if (mz<0x5C000000UL){
        val = mz/131072.0 - 10240.0;//0955200195;  512-1024
    } else if (mz<0x64000000UL){
        val = mz/65536.0 - 22528.0;//7.8426513672;  1024-2048
    } else if (mz<0x6C000000UL){
        val = mz/32768.0 - 49152.0;//1.181640625;   2048-4096
    } else if (mz<0x74000000UL){
        val = mz/16384.0 - 106496.0;//193;  4096-8192
    } else if (mz<0x7C000000UL){
        val = mz/8192.0 - 229375.0;//193;  8193-16384
    } else if (mz<0x84000000UL){
        val = mz/4096.0 - 491520.0;//193;  16385-32768
    } else {
        val = 32768.0;  // clip upper limit at 32768
    }

    return val;
}

// decode type1 Mz_type_6_v3c data
double Reader::decodeMzType1_6_V3C(unsigned int mz) //<==== MAKE LUT !!!!
{
    if (mz == 0)
        return 0;
    double val;
    if (mz<0x34000000UL) {
        val = 32.0;  // clip lower limit at 32.0
    } else if (mz<0x3C000000UL){
        val = mz / 2097152.0 - 384.0;//166076660156;  32-64
    } else if (mz<0x44000000UL){
        val = mz / 1048576.0 - 896.0;//166076660156;  64-128
    } else if (mz<0x4C000000UL){
        val = mz / 524288.0 - 2048.0;//1727142334;  128-256
    } else if (mz<0x54000000UL){
        val = mz / 262144.0 - 4608.0;//16940307617;  256-512
    } else if (mz<0x5C000000UL){
        val = mz / 131072.0 - 10240.0;//0955200195;  512-1024
    } else if (mz<0x64000000UL){
        val = mz / 65536.0 - 22528.0;//7.8426513672;  1024-2048
    } else if (mz<0x6C000000UL){
        val = mz / 32768.0 - 49152.0;//1.181640625;   2048-4096
    } else if (mz<0x74000000UL){
        val = mz / 16384.0 - 106496.0;//193;  4096-8192
    } else if (mz<0x7C000000UL){
        val = mz/8192.0 - 229375.0;//193;  8193-16384
    } else if (mz<0x84000000UL){
        val = mz/4096.0 - 491520.0;//193;  16385-32768
    } else {
        val = 32768.0;  // clip upper limit at 32768
    }
    return val;
}

// decode type1 Mz_type_6 data
double Reader::decodeMzType1_6(unsigned int mz) //<==== MAKE LUT !!!!
{
    if (mz==0)
        return 0;
    unsigned char scl = (unsigned char)mz; unsigned int mzi = mz >> 8;
    if ((scl&0xf) != 0){
        std::cout << "non zero scl lsb" << endl;
        mzi+=0;
    }
    scl>>=4;

    double fact = (double)(16777216ULL>>scl);
    double val = mzi/fact;
    return val;
}

// recode type1 Mz_type_6_v3c data
unsigned int Reader::recodeMzType1_6_V3C(unsigned int mz) //<==== MAKE LUT !!!!
{
    if (mz==0)
        return 0;
    double val = decodeMzType1_6_V3C(mz);
    if (val<32.0)
        return 0x34000000UL;
    if (val<64.0)
        return (unsigned int)((val+384.0)*2097152.0);
    if (val<128.0)
        return (unsigned int)((val+896.0)*1048576.0);
    if (val<256.0)
        return (unsigned int)((val+2048.0)*524288.0);
    if (val<512.0)
        return (unsigned int)((val+4608.0)*262144.0);
    if (val<1024.0)
        return (unsigned int)((val+10240.0)*131072.0);
    if (val<2048.0)
        return (unsigned int)((val+22528.0)*65536.0);
    if (val<4096.0)
        return (unsigned int)((val+49152.0)*32768.0);
    if (val<8192.0)
        return (unsigned int)((val + 106496.0)*16384.0);
    if (val<16384.0)
        return (unsigned int)((val + 229375.0)*8192.0);
    if (val<32768.0)
        return (unsigned int)((val + 491520.0)*4096.0);
    return 0x84000000UL;
}

// recode type1 Mz_type_6 data
unsigned int Reader::recodeMzType1_6(unsigned int mz) //<==== MAKE LUT !!!!
{
    if (mz==0)
        return 0;
    unsigned char scl = (unsigned char)mz; unsigned int mzi = mz >> 8;
    if ((scl&0xf) != 0){
        std::cout << "non zero scl lsb" << endl;
        mzi+=0;
    }
    scl>>=4;

    double fact = (double)(16777216ULL>>scl);
    double val = mzi/fact;

    if (val<32.0)
        return 0x34000000UL;
    if (val<64.0)
        return (unsigned int)((val+384.0)*2097152.0);
    if (val<128.0)
        return (unsigned int)((val+896.0)*1048576.0);
    if (val<256.0)
        return (unsigned int)((val+2048.0)*524288.0);
    if (val<512.0)
        return (unsigned int)((val+4608.0)*262144.0);
    if (val<1024.0)
        return (unsigned int)((val+10240.0)*131072.0);
    if (val<2048.0)
        return (unsigned int)((val+22528.0)*65536.0);
    if (val<4096.0)
        return (unsigned int)((val+49152.0)*32768.0);
    if (val<8192.0)
        return (unsigned int)((val+106496.0)*16384.0);
    if (val<16384.0)
        return (unsigned int)((val+229375.0)*8192.0);
    if (val<32768.0)
        return (unsigned int)((val+491520.0)*4096.0);
    return 0x84000000UL;
}

// read calibration data from file
bool Reader::readCalibrationDataFromMetadataFile(const wstring &raw_dir, std::vector<std::vector<string>> * calib_metadata, std::vector<std::vector<string>> * calib_coeff, std::vector<string> * calib_modf)
{
    wstring hdr_filename = raw_dir; hdr_filename.append(L"_HEADER.TXT");
    int hdr_file_size = (int)getFileSize(hdr_filename);
    if (hdr_file_size<0){
        std::wcerr << "can't find header file = " << hdr_filename << "\"" << endl; return false;
    }
    if (hdr_file_size==0){
        std::wcerr << "empty header file = " << hdr_filename << "\"" << endl; return false;
    }
    if (verbose==1) std::cout << "header file contains " << hdr_file_size << " bytes" << endl;

    FILE* hdrFile = NULL;
    errno_t err = _wfopen_s(&hdrFile, hdr_filename.c_str(), L"rbS");
    if (err != 0){
        wcerr << "can't open header file = " << hdr_filename << "\"" << endl; return false;
    }

    unsigned char * hdr_buf = (unsigned char *)malloc(hdr_file_size*sizeof(unsigned char)); 
    if (hdr_buf==NULL){
        std::cout << "mz buf alloc failed" << endl; fclose_safe(hdrFile); return false;
    } 

    size_t hdr_count = fread(hdr_buf, 1, hdr_file_size, hdrFile); 
    if (hdr_count<(size_t)hdr_file_size){
        std::cout << "can't read from header file = " << hdr_filename.c_str() << endl; 
    }
    fclose_safe(hdrFile); //fclose_safe(hdrFile);
    char* ptc = (char*)hdr_buf;

    while (ptc - (char*)hdr_buf < (int)hdr_count){
        char cc = *ptc++; string line;
        while ((cc != '\n') && (ptc - (char*)hdr_buf < (int)hdr_count)) {
            line += cc;
            cc = *ptc++;
        }
        std::size_t found = line.find("$$ Cal Function ");
        if (found != std::string::npos){
            int f_id = atoi((char*)line.c_str() + 16);
            if (f_id>(int)calib_coeff->size()) calib_coeff->resize(f_id);
            std::size_t found = line.find(": ");
            line = line.substr(found + 2, line.length());
/*          WatersCalibration::Coefficents coeff;
            coeff.parseCalibrationLine(line);
            WatersCalibration wtrs_cal;
            wtrs_cal.checkCoefficients(coeff.coeffients);*/
            found = line.find(",T");
            string tag = "T";
            if (found != std::string::npos){
                if (line.length() > found + 2){
                    tag += line[found + 2];
                }
                line = line.substr(0, found + 1);
            } else {
                tag += "9";
            }
            string data = "<" + tag + ">" + line + "</" + tag + ">";
            (*calib_coeff)[f_id - 1].push_back(data);
            if (verbose == 1) std::cout << "calib data func_" << f_id << ": " << data << '\n';

        } else {
            found = line.find("$$ Cal Modification ");
            if (found != std::string::npos){
                int f_id = atoi((char*)line.c_str() + 20);
                if (f_id > (int)calib_coeff->size()) calib_coeff->resize(f_id);
                std::size_t found = line.find(": ");
                line = line.substr(found + 2, line.length());
                found = line.find(",T");
                string tag = "T";
                if (found != std::string::npos){
                    if (line.length() > found + 2){
                        tag += line[found + 2];
                    }
                    line = line.substr(0, found + 1);
                } else {
                    tag += "9";
                }
                string data = "<" + tag + ">" + line + "</" + tag + ">";
                if (f_id>(int)calib_modf->size()) calib_modf->resize(f_id);
                (*calib_modf)[f_id - 1] = data;
                if (verbose == 1) std::cout << "calib mod func_" << f_id << ": " << data << '\n';

            } else {
                std::size_t found = line.find("$$ Cal MS");
                if (found != std::string::npos){
                    int f_id = atoi((char*)line.c_str() + 9);
                    if (f_id > (int)calib_metadata->size()) calib_metadata->resize(f_id);
                    if (verbose == 1) std::cout << "calib metadata func_" << f_id << ": " << line << '\n';
                    (*calib_metadata)[f_id - 1].push_back(line);
                }
            }
        }
    }
    free_safe(hdr_buf);
    return true;
}

// parse calibration string
bool Reader::parseCalibrationString(const string &coeff_str, int* type, std::vector<double> &coeffs)
{
    if (coeff_str.length()<10)
        return false;
    if (coeff_str[0]!='<' || coeff_str[1]!='T' || coeff_str[3]!='>') 
        return false;
    char ch = coeff_str[2];
    if (coeff_str[coeff_str.length()-5]!='<' || coeff_str[coeff_str.length()-4]!='/' || coeff_str[coeff_str.length()-3]!='T'  || coeff_str[coeff_str.length()-2]!=ch || coeff_str[coeff_str.length()-1]!='>' )
        return false;
    //string tag = "T"; tag.push_back(ch);
    *type = ch - '0';
    string data = coeff_str.substr(4, coeff_str.length()); data = data.substr(0, data.length()-5);
    std::vector<string> splits;
    boost::split(splits, data, boost::is_any_of(","));
    for (unsigned int ii=0; ii<splits.size(); ii++){
        string str = splits[ii];
        if (ii<splits.size()-1 || (ii == splits.size()-1 && str.length()>0))
            coeffs.push_back(atof(str.c_str()));
    }
    return true;
}

// copy initial part to file
bool Reader::copyPartToFile(const wstring &in_filename, int length)
{
    wstring out_filename = in_filename + L"_out.bin";
    ifstream infile;
    infile.open (in_filename, ios::in | ios::binary); 
    if (!infile.good()){
        wcerr << "can't read input file=" << in_filename << endl; return false;
    }
    ofstream outfile;
    outfile.open (out_filename, ios::out | ios::binary); 
    if (!outfile.good()){
        wcerr << "can't open output file=" << out_filename << endl; return false;
    }
    unsigned char * buf = new unsigned char[(int)length];
    infile.read((char *)buf, length);
    outfile.write((char *)buf, length);
    infile.close(); outfile.close();
    delete[] buf;
    return true;
}

// check raw file integrity
void Reader::checkRawFileType1Integrity(std::wstring &raw_dir, std::vector<kFileIntegrityError> & errs)
{
    errs.clear();
    PicoUtil* utl = new PicoUtil();
    if (verbose) std::cout << "checking raw file structure integrity:" << endl;
    if (!utl->dirExists(raw_dir)){
        errs.push_back(kFileIntegrityError(kFileIoError, 0, 0));
        if (verbose) std::cout << "  can't find raw directry = " << raw_dir.c_str() << endl; 
        delete utl; return;
    }
    wstring rdir = utl->stripPathChar(raw_dir); raw_dir = rdir.append(L"\\");
    std::vector<wstring> list = utl->dirList(raw_dir);
    if (list.size() == 0){
        errs.push_back(kFileIntegrityError(kFileIoError, 0, 0));
        if (verbose) std::cout << "  empty raw directry = " << raw_dir.c_str() << endl;
        delete utl; return;
    }

    wstring f_filename = raw_dir; f_filename.append(L"_FUNCTNS.INF");
    if (!utl->fileExists(f_filename)){
        errs.push_back(kFileIntegrityError(kMissingFunctionsInf, 0, 0));
        if (verbose) std::cout << "  missing functns.inf file = " << f_filename.c_str() << endl;
    }

    int ms_func_cnt = 0;
    std::vector<FunctionRec> functionRecords;
    if (!getWatersFunctionInfo(raw_dir, functionRecords, &ms_func_cnt)){
        errs.push_back(kFileIntegrityError(kMissingFunctionsInf, 0, 0));
        if (verbose) std::cout << "  can't parse functns.inf file = " << f_filename.c_str() << endl;
    }

    wstring h_filename = raw_dir; h_filename.append(L"_HEADER.TXT");
    if (!utl->fileExists(h_filename)){
        errs.push_back(kFileIntegrityError(kMissingHeaderTxt, 0, 0));
        if (verbose) std::cout << "  missing header.txt file = " << h_filename.c_str() << endl;
    }

    WatersMetaReading watersMeta;
    Err e = watersMeta.readFromWatersPath(raw_dir);
    if (e != kNoErr) {
        structured_output(ReaderConstants::kMessage, L"Error reading meta information of raw directry = " + raw_dir);
        structured_output(ReaderConstants::kStatus, ReaderConstants::kError);
        if (e == kErrorFileIO){
            errs.push_back(kFileIntegrityError(kFileIoError, 0, 0));
            if (verbose) std::cout << "  file i/o error (missing or can't read extern.inf)" << endl;
            delete utl; return;
        }
        else {
            if (verbose) std::cout << "  file error (can't parse extern.inf metadata)" << endl;
            errs.push_back(kFileIntegrityError(kParseError, 0, 0));
            delete utl; return;
        }
    }

    wstring meta_filename;
    wstring g_extern_inf_lower = g_extern_inf_filename;
    transform(g_extern_inf_lower.begin(), g_extern_inf_lower.end(), g_extern_inf_lower.begin(), tolower);
    int max_func = -1, max_chro = -1;
    for (unsigned int ii = 0; ii<list.size(); ii++){
        wstring name = utl->stripPathChar(list[ii]);
        wstring prefix = name.substr(0, 6), suffix = name.substr(name.size() - 4, name.size());
        if (::pico::iequals(prefix, L"_FUNC0") && ::pico::iequals(suffix, L".DAT") || ::pico::iequals(suffix, L".IDX") || ::pico::iequals(suffix, L".STS")){
            int pos = static_cast<int>(name.find('.'));
            if (pos > 0){
                wstring str = name.substr(5, pos); 
                int indx = _wtoi(str.c_str());
                if (indx > max_func)
                    max_func = indx;
            }
        }
        if (::pico::iequals(prefix, L"_CHRO0")){
            int pos = static_cast<int>(name.find('.'));
            if (pos > 0){
                wstring str = name.substr(5, pos);
                int indx = _wtoi(str.c_str());
                if (indx > max_func)
                    max_chro = indx;
            }
        }
        if (::pico::iequals(name, g_extern_inf_filename))
            meta_filename = raw_dir + name;
    }

    if (max_func >= 0 && max_func < MAX_FUNC){ // check non func
        for (int indx = 1; indx <= max_func; indx++){

            wstring name_prefix = L"_FUNC";
            wstring name = appendZeroPaddedNumberForWatersFile(name_prefix, indx);

            int ms_level = watersMeta.getMSLevelForFunction(indx);
            if (ms_level>=0 && ms_level <= 9){
                wstring name_d = name; name_d.append(L".DAT");
                wstring filename = raw_dir + name_d;
                if (!utl->fileExists(filename)){
                    errs.push_back(kFileIntegrityError(kMissingDatFunction, (short)indx, (short)ms_level));
                    if (verbose) std::cout << "  missing dat function = " << filename.c_str() << endl;
                }

                wstring name_x = name; name_x.append(L".IDX");
                filename = raw_dir + name_x;
                if (!utl->fileExists(filename)){
                    errs.push_back(kFileIntegrityError(kMissingIdxFunction, (short)indx, (short)ms_level));
                    if (verbose) std::cout << "  missing idx function = " << filename.c_str() << endl;
                }

            } else if (ms_level>0) {
                wstring name_s = name; name_s.append(L".STS");
                wstring filename = raw_dir + name_s;
                if (!utl->fileExists(filename)){
                    errs.push_back(kFileIntegrityError(kMissingStsFunction, (short)indx, (short)ms_level));
                    if (verbose) std::cout << "  missing dat function = " << filename.c_str() << endl;                   
                }
            }
        }
    }

    if (max_chro >= 0 && max_chro < MAX_FUNC){ // check chros
        wstring filename = raw_dir; filename.append(L"_CHROMS.INF");
        if (!utl->fileExists(filename)){
            errs.push_back(kFileIntegrityError(kMissingChromInf, 0, 0));
            if (verbose) std::cout << "  missing chroms.inf file = " << filename.c_str() << endl;
        }

        std::vector<std::vector<std::pair<wstring, wstring>>> chroms_metadata;
        if (!readChromsMetadata(raw_dir, chroms_metadata)){
            errs.push_back(kFileIntegrityError(kChromParseError, 0, 0));
            if (verbose == 1) std::cout << "can't read or parse chroms metadata file =" << filename.c_str() <<endl;
        }
        for (unsigned int ii = 0; ii < chroms_metadata.size(); ii++){
            std::vector<std::pair<wstring, wstring>> chroms_i = chroms_metadata[ii];
            wstring fname, chr_filename = raw_dir, key = L"Channel_", value = L"Absorbance at ";
            for (unsigned int jj = 0; jj < chroms_i.size(); jj++){
                std::pair<wstring, wstring> pair_j = chroms_i[jj];
                if (pair_j.first == L"channel")
                    key.append(pair_j.second);
                else if (pair_j.first == L"wavelength")
                    value.append(pair_j.second);
                else if (pair_j.first == L"filename"){
                    fname = pair_j.second; chr_filename.append(fname); chr_filename.append(L".DAT");
                }
            }
        }

        for (int indx = 1; indx <= max_chro; indx++){
            wstring name_prefix(L"_CHRO");
            wstring name = appendZeroPaddedNumberForWatersFile(name_prefix, indx);

            wstring name_c = name; name_c.append(L".DAT");
            wstring filename = raw_dir + name_c;
            if (!utl->fileExists(filename)){
                errs.push_back(kFileIntegrityError(kMissingChromDat, (short)indx, 0));
                if (verbose) std::cout << "  missing chrom dat function = " << filename.c_str() << endl;
            }
        }
    }
    delete utl;
}

// report integrity errors -- 0=no error, 1=fatal, 2=warning
void Reader::error_message(const std::vector<kFileIntegrityError> &errs, FileIntegrityResult * result)
{
    if (errs.size() > 0) {
        bool fatal = false;
        cerr << "\nMissing or corrupt content reported by MS data integrity check:" << endl;
        for (int ii = 0; ii < (int)errs.size(); ii++){
            kFileIntegrityError erri = errs[ii];           

            wstring name_prefix(L"_FUNC");
            wstring wstr_name = appendZeroPaddedNumberForWatersFile(name_prefix, erri.channel);
            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
            string name = conv.to_bytes(wstr_name);
            cerr << "  [" << ii << "] ";
            if (erri.err_type == kFileIoError){
                cerr << "missing or can't read _EXTERN.INF file" << endl; fatal = true;
            } else if (erri.err_type == kParseError){
                cerr << "corrupt _EXTERN.INF file" << endl; fatal = true;
            } else if (erri.err_type == kMissingExternInf){
                cerr << "missing _EXTERN.INF file" << endl; fatal = true;
            } else if (erri.err_type == kMissingFunctionsInf){
                cerr << "missing _FUNCTNS.INF file" << endl; //fatal = true; //<=== note we have a way to recover even if this file is missing (while pwiz crashes)
            } else if (erri.err_type == kMissingHeaderTxt){
                cerr << "missing _HEADER.TXT file" << endl;
            } else if (erri.err_type == kMissingChromInf){
                cerr << "missing or corrupt _CHROMS.INF file" << endl;
            } else if (erri.err_type == kChromParseError){
                cerr << "corrupt _CHROMS.INF file" << endl;
            } else if (erri.err_type == kMissingDatFunction){ 
                cerr << "missing " << name << ".DAT file" << endl; 
                if (erri.ms_level>0 && erri.ms_level<9) 
                    fatal = true;
            } else if (erri.err_type == kMissingIdxFunction){
                cerr << "missing " << name << ".IDX file" << endl;
                if (erri.ms_level>0 && erri.ms_level<9) 
                    fatal = true;
            } else if (erri.err_type == kMissingStsFunction){
                cerr << "missing " << name << ".STS file" << endl;
                if (erri.ms_level>0 && erri.ms_level<9) 
                    fatal = true;
            } else if (erri.err_type == kMissingChromDat){
                cerr << "missing _CHRO" << name.substr(5, name.size()) << ".DAT file" << endl;
            } else {
                cerr << "unidentified error" << endl;
            }
        }
        cerr << "\n";
        if (fatal) {
            *result = FileIntegrityResult_Error;
            structured_output(ReaderConstants::kMessage, L"Error (fatal): missing or corrupt content reported by MS data integrity check");
            structured_output(ReaderConstants::kStatus, ReaderConstants::kError);
        } else {
            *result = FileIntegrityResult_Warning;
            structured_output(ReaderConstants::kMessage, L"Warning: missing or corrupted (non-critical) content reported by MS data integrity check");
            structured_output(ReaderConstants::kStatus, ReaderConstants::kWarning);
        }
    } else {
        cerr << "\n";
        *result = FileIntegrityResult_Sucess;
        structured_output(ReaderConstants::kMessage, L"Success: MS data integrity check completed successfully");
        structured_output(ReaderConstants::kStatus, ReaderConstants::kSuccess);
    }
}

std::wstring Reader::toString(FileIntegrityResult val)
{
    std::wstring str = L"Unknown";
    switch (val) {
    case FileIntegrityResult_Sucess:
        str = ReaderConstants::kSuccess;
        break;
    case FileIntegrityResult_Warning:
        str = ReaderConstants::kWarning;
        break;
    case FileIntegrityResult_Error:
        str = ReaderConstants::kError;
        break;
    }
    return str;
}

// read file 
bool Reader::read(const wstring &in_filename, int length)
{

    //std::vector<double> data;
    //string out_filename = in_filename + "_out.bin";
    ifstream infile;
    infile.open (in_filename, ios::in | ios::binary); 
    if (!infile.good()){
        std::cout << "can't read input file -- no good" << endl; return false;
    }




    unsigned char * buf = new unsigned char[(int)length];
    infile.read((char *)buf, length);
    infile.close(); 


    unsigned int* ptr = (unsigned int *)buf;
    unsigned int intens0=*ptr++, mz0 = *ptr++, prev = mz0, pdel = 0;
    unsigned int scale0 = intens0&0xFFFF; intens0 >>= 16;
    std::cout << dec << setfill('0') << setw(6) << 0 << ": " << hex << setfill('0') << setw(8) << mz0 << "  " << setfill('0') << setw(4) << intens0 << "  " << setfill('0') << setw(4) << scale0 << ";" << endl;
    for (int ii=1; ii<233610; ii++){
        if (ii>length/8)
            break;
        unsigned int val = *ptr++, mz = *ptr++;
        //unsigned int intens = val>>16;
        unsigned int scale = val&0xFFFF;
        unsigned int del = mz - prev;
        //cout << dec << setfill('0') << setw(6) << ii << ": " << hex << setfill('0') << setw(8) << mz << "  " << setfill('0') << setw(4) << intens << "  " << setfill('0') << setw(4) << scale << "; " << setfill(' ') << setw(4) << del << endl;
        if (ii%100==0)
            std::cout << dec << ii << endl;
        prev = mz; 
        if (ii==1) pdel = del;
        else {
            if (del != pdel)
                ii+=0;
        }

        if ((scale&0xFFF)>0)
            ii+=0;

        if (ii>200 && ((mz>>16)==0x4e40))
            ii+=0;

        if (mz==mz0)
            ii+=0;

        if (ii>10) //7534)
            ii+=0;
    }

    delete[] buf;
    return true;
}

// read file
bool Reader::readIdx(const wstring &in_filename, int length)
{
    std::vector<double> data;
    wstring out_filename = in_filename + L"_out.bin";
    ifstream infile;
    infile.open (in_filename, ios::in | ios::binary); 
    if (!infile.good()){
        std::cout << "can't read input file -- no good" << endl; return false;
    }
    unsigned char * buf = new unsigned char[length];
    infile.read((char *)buf, length);
    infile.close(); 

    unsigned short* ptr = (unsigned short *)buf;
    //unsigned int intens0=*ptr++, mz0 = *ptr++, prev = mz0, pdel = 0;
    //unsigned int scale0 = intens0&0xFFFF; intens0 >>= 16;
    //cout << dec << "ii=0 ,y=" << hex << intens0 << ", s=" << scale0 << ", mz=" << mz0 << endl;
    unsigned short* val = new unsigned short[22];
    for (int ii=0; ii<233610; ii++){
        if (ii>length/8)
            break;
        std::cout << dec << std::cout.width(4) << ii << ":" << hex;
        for (int jj=0; jj<11; jj++){
            val[jj] = *ptr++;
            std::cout  << val[jj] << ","; //cout.width(4) << 
        }
        std::cout << endl;
        //prev = mz; 
        /*if (ii==1) pdel = del;
        else {
            if (del != pdel)
                ii+=0;
        }*/


        //unsigned long long val1 = _byteswap_uint64(val);

        if (ii>10) //7534)
            ii+=0;
    }

    delete[] val; delete[] buf;
    return true;
}

// identify idx size
int Reader::identifyIdxSize(const wstring &raw_dir, const std::vector<wstring> &functions){
    const int IDX_REC_SIZE[] = {22, 30}; const int IDX_REC_LEN = 2;
    int cnt_0 = 0, cnt_1 = 0;
    std::vector<unsigned int> counts; counts.resize(IDX_REC_LEN);
    for (unsigned int function=1; function<functions.size()+1; function++){
        wstring name_prefix(L"_FUNC");
        wstring name = appendZeroPaddedNumberForWatersFile(name_prefix, function);

        wstring name1 = name; name1.append(L".IDX");
        wstring idx_filename = raw_dir + name1;
        int idx_file_size = (int)getFileSize(idx_filename);
        if (idx_file_size<0){
            std::wcerr << "can't open idx file = " << idx_filename << "\"" << endl; continue;
        }
        if (idx_file_size==0){
            std::wcerr << "empty idx file = " << idx_filename << "\"" << endl; continue;
        }
        //if (verbose==1) std::cout << "idx file contains " << idx_file_size << " bytes" << endl;
        if ((idx_file_size%IDX_REC_SIZE[0]) == 0)
            cnt_0++;
        if ((idx_file_size%IDX_REC_SIZE[1]) == 0)
            cnt_1++;
        for (int ii = IDX_REC_LEN - 1; ii >= 0; ii--){
            int val = IDX_REC_SIZE[ii];
            if ((idx_file_size%val) == 0){
                if (ii>0 && (idx_file_size%IDX_REC_SIZE[ii-1]==0)){
                    counts[ii-1]++; break;
                    //IDX_SIZE = IDX_REC_SIZE[ii-1];
                } else {
                    counts[ii]++;
                    std::cout << "func="<<function<<", idx_size="<<val<<endl;
                    //IDX_SIZE = val; break;
                }
            }
        }
        function+=0;
    }
    int max = -1, imax = -1;
    for (unsigned int ii=0; ii<counts.size(); ii++){
        unsigned int val = counts[ii];
        if ((int)val>max) { max=val, imax=ii; }
    }
    if (max < 0){
        std::cout << "can't determine index size: neg max"; return -1;
    } else if (max == 0){
        if (cnt_0 == cnt_1){
            std::cout << "can't determine index size: zero max"; return -1;
        } else {
            imax = cnt_0 > cnt_1 ? 0 : 1;
        }
    }
    for (int ii=0; ii<(int)counts.size(); ii++){
        if (ii==imax) continue;
        unsigned int val = counts[ii];
        if ((int)val==max){
            if (cnt_0 == cnt_1){
                std::cout << "caution: non-unique max index size";
                int idx_size = IDX_REC_SIZE[1];
                if (verbose == 1) std::cout << "identify idx file size = " << idx_size << " bytes" << endl;
                return idx_size;
            }
            else {
                imax = cnt_0 > cnt_1 ? 0 : 1; break;
            }
        }
    }

    std::vector<int> results;
    if (!checkIndexSize(raw_dir, results)){
        if (verbose == 1) std::cout << "can't check idx file for intenral size" << endl;
    }

    int idx_size = IDX_REC_SIZE[imax], resulti = results[imax];
    if (resulti < 0){
        idx_size = IDX_REC_SIZE[1 - imax];
    }

    if (verbose==1) std::cout << "identify idx file size = " << idx_size << " bytes" << endl;
    return idx_size;
}

// check index size
bool Reader::checkIndexSize(const wstring &raw_dir, std::vector<int> &results)
{
    const int IDX_REC_SIZE[] = { 22, 30 }; const int IDX_REC_LEN = 2; 
    results.clear();
    std::cout << "checking index size" << endl;
    wstring idx_name(L"_FUNC001.IDX");
    wstring idx_filename = raw_dir + idx_name;
    int idx_file_size = (int)getFileSize(idx_filename);
    if (idx_file_size < 0){
        std::wcerr << "can't open idx file = " << idx_filename << "\"" << endl; return false;
    }
    if (idx_file_size == 0){
        std::wcerr << "empty idx file = " << idx_filename << "\"" << endl; return false;
    }
    if (verbose == 1) std::cout << "idx file contains " << idx_file_size << " bytes" << endl;

    FILE* idxFile = NULL;
    errno_t err = _wfopen_s(&idxFile, idx_filename.c_str(), L"rbS");
    if (err != 0){
        std::cout << "can't open idx file = " << idx_filename.c_str() << "\"" << endl; return false;
    }

    unsigned char * idx_buf = (unsigned char *)malloc(idx_file_size*sizeof(unsigned char));
    if (idx_buf == NULL){
        std::cout << "mz buf alloc failed" << endl; fclose_safe(idxFile); return false; //fclose_safe(idxFile); 
    }

    size_t idx_count = fread(idx_buf, 1, idx_file_size, idxFile);
    if (idx_count < (size_t)idx_file_size){
        std::cout << "can't read from idx file = " << idx_filename.c_str() << endl; free_safe(idx_buf); fclose_safe(idxFile); return false; //fclose_safe(idxFile); 
    }
    fclose_safe(idxFile); //fclose_safe(idxFile);

    for (int kk = 0; kk < IDX_REC_LEN; kk++){
        int idx_size = IDX_REC_SIZE[kk];
        int max_scan = min((int)(idx_file_size / idx_size), 100);
        unsigned char* ptx = idx_buf; std::vector<float> retentioni;
        for (int scan = 0; scan < max_scan; scan++){
            ptx += 4;
            unsigned int bb = *ptx++;
            unsigned int bb1 = *ptx++;
            unsigned int bb2 = (*ptx++ & 0x3f);
            unsigned int mz_len = (bb2 << 16) | (bb1 << 8) | bb; ptx += 5;
            (void)(mz_len); //avoid compile warning
            float RetentionTime = *(float *)ptx; ptx += 4;
            RetentionTime *= 60.0F;
            retentioni.push_back(RetentionTime);
            if (idx_size == 22)
                ptx += 6;
            else
                ptx += 14; //<=========== for df 
        }
        float prev = retentioni[0]; bool no_good = false;
        for (int ii = 1; ii < max_scan; ii++){
            float cur = retentioni[ii];
            if (cur < 0.1 || cur > 7200){
                no_good = true;  break;
            }
            if (cur-prev < 0){
                no_good = true;  break;
            }
            prev = cur;
        }
        if (no_good){
            results.push_back(-1);
        } else {
            results.push_back(1);
        }
        kk += 0;
    }
    free_safe(idx_buf);
    return true;
}

// read metadata file
std::vector<wstring> Reader::readMetadata(const wstring &meta_filename)
{
    std::vector<wstring> content;
    wstring line;
    wifstream metafile (meta_filename.c_str());
    if (metafile.is_open()) {
        while (getline (metafile,line)){
            content.push_back(line);
        }
        metafile.close();
    } else 
        wcerr << "can't open meta file = \"" << meta_filename << "\"" << endl; //
    return content;
}

// extract function metadata: type: MS1,MS2,MSE [start..end] time, [start..end] mz range, 
std::vector<std::vector<wstring>> Reader::extractMetadata(const std::vector<wstring> &metadata_content)
{
    if (verbose==1) std::cout << " -- reading file metadata:" << endl;
    std::vector<std::vector<wstring>> info(1);
    std::vector<wstring> ms_level;
    int function = -1;
    wstring file_type = L"MS1";
    for (size_t ii=0; ii<metadata_content.size(); ii++){
        wstring line = metadata_content[ii];
        if (verbose==1) wcout<< line << endl;
        if (line.size()>=25){
            if (line.substr(0,25).compare(L"Lock Spray Configuration:")==0){
                if (metadata_content.size()>ii){
                    wstring linep = metadata_content[ii+1];
                    if (linep.size()>=29){
                        if (linep.substr(0,29).compare(L"Reference Scan Frequency(sec)")==0){
                            ptrdiff_t pos = linep.size()-1;
                            while (linep[pos]!='\t') pos--;
                            wstring val = linep.substr(pos+1,linep.size());
                            //double lockspray_freq = stod(val,NULL);
                            info[0].push_back(L"LockSprayRefScanFreq(sec)"); info[0].push_back(val); ii++;
                        }
                    }
                }
            }
        }
        if (line.size()>=31){
            if (line.substr(0,31).compare(L"Function Parameters - Function ")==0){
                wstring line2 = line.substr(31,line.size());
                if (line2.size()>2){
                    std::size_t pos = line2.find(L" ");
                    if (pos<=0)
                        continue;
                    wstring vali = line2.substr(0, pos);
                    function = _wtoi(vali.c_str()); wstring line3=line2.substr(pos+3, line2.size());
                    if (function <= 0 || function > MAX_FUNC){
                        return std::vector<std::vector<wstring>>(0); 
                    }
                    if ((int)info.size()<=function){
                        info.resize(function+1); ms_level.resize(function+1);
                    }
                    if (line3.size()>=19){
                        if (line3.substr(0,19).compare(L"TOF PARENT FUNCTION")==0){
                            if (function > 0){
                                info[function].push_back(L"TOF PARENT FUNCTION");
                            }
                            ms_level[function] = L"MS1"; continue;
                        }
                    } 
                    if (line3.size()>=15){
                        if (line3.substr(0,15).compare(L"TOF MS FUNCTION")==0){
                            ms_level[function] = L"MS1"; continue;
                        }
                    } 
                    if (line3.size()>=21){
                        if (line3.substr(0,21).compare(L"TOF FAST DDA FUNCTION")==0){
                            ms_level[function] = L"MS1"; file_type = L"MS2"; continue;
                        }
                    } 
                    if (line3.size()>=19){
                        if (line3.substr(0,19).compare(L"TOF SURVEY FUNCTION")==0){
                            ms_level[function] = L"MS2"; file_type = L"MS2"; continue;
                        }
                    } 
                    if (line3.size() >= 9){
                        if (line3.substr(0,9).compare(L"REFERENCE")==0){
                            ms_level[function] = L"REF"; file_type = L"REF";
                            for (size_t jj = ii + 1;
                                 jj < std::min(std::vector<wstring>::size_type(ii + 4),
                                          metadata_content.size());
                                 jj++)
                            {
                                wstring linej = metadata_content[jj];
                                if (verbose==1) wcout<< linej << endl;
                                if (linej.size()>=8){
                                    if (linej.substr(0,8).compare(L"Interval")==0){
                                        ptrdiff_t pos = linej.size()-1;
                                        while (linej[pos]!='\t') pos--;
                                        wstring val = linej.substr(pos+1,linej.size());
                                        //double interval = stod(val,NULL);
                                        info[0].push_back(L"REF_Interval"); info[0].push_back(val); ii++;
                                    }
                                }
                            }                            
                            continue;
                        }
                    } else {
                        if (verbose==1) wcout << "unknown mstype = " << line3 << endl;
                        function = -1; continue;
                    }
                }
            }
        } 
        if (line.size()>=15){
            if (line.substr(0,15).compare(L"MS Profile Type")==0){
                ptrdiff_t pos = line.size()-1;
                while (line[pos]!='\t') pos--;
                wstring val = line.substr(pos+1,line.size());
                if (val.compare(L"Auto P")==0)
                    val = L"AutoP";
                info[0].push_back(L"ProfileMode"); info[0].push_back(val);
            }
        }
        if (function>=0){
            if (line.size()>=21){
                if (line.substr(0,21).compare(L"Ramp High Energy from")==0){
                    ms_level[function] = L"MS2"; file_type=L"MSE";
                    info[function].push_back(L"Ramp High Energy from");
                    ptrdiff_t pos = line.size() - 1;
                    while (line[pos] != '\t') pos--;
                    wstring val = line.substr(pos + 1, line.size());
                    info[function].push_back(val);
                }
            }
            if (line.size()>=15){
                if (line.substr(0,15).compare(L"MSMS Start Mass")==0){
                    //ms_level[function] = "MS2"; file_type=L"MSE";
                    info[function].push_back(L"MSMS Start Mass");
                }
            }
            if (line.size()>=17){
                if (line.substr(0,17).compare(L"Survey Start Time")==0){
                    ptrdiff_t pos = line.size()-1;
                    while (line[pos]!='\t') pos--;
                    wstring val = line.substr(pos+1,line.size());
                    //double start_time = stod(val,NULL);
                    info[function].push_back(L"StartTime"); info[function].push_back(val);
                }
            }
            if (line.size()>=15){
                if (line.substr(0,15).compare(L"Survey End Time")==0){
                    ptrdiff_t pos = line.size()-1;
                    while (line[pos]!='\t') pos--;
                    wstring val = line.substr(pos+1,line.size());
                    //double end_time = stod(val,NULL);
                    info[function].push_back(L"EndTime"); info[function].push_back(val);
                }
            }
            if (line.size()>=17){
                if (line.substr(0,17).compare(L"Survey Start Mass")==0){
                    ptrdiff_t pos = line.size()-1;
                    while (line[pos]!='\t') pos--;
                    wstring val = line.substr(pos+1,line.size());
                    //double start_time = stod(val,NULL);
                    info[function].push_back(L"StartMass"); info[function].push_back(val);
                }
            }
            if (line.size()>=15){
                if (line.substr(0,15).compare(L"Survey End Mass")==0){
                    ptrdiff_t pos = line.size()-1;
                    while (line[pos]!='\t') pos--;
                    wstring val = line.substr(pos+1,line.size());
                    //double end_time = stod(val,NULL);
                    info[function].push_back(L"EndMass"); info[function].push_back(val); //MS Collision Energy High?
                }
            }
            if (line.size()>=17){
                if (line.substr(0,17).compare(L"FragmentationMode")==0){
                    ptrdiff_t pos = line.size()-1;
                    while (line[pos]!='\t') pos--;
                    wstring val = line.substr(pos+1,line.size());
                    info[function].push_back(L"FragMode"); info[function].push_back(val);
                }
            }
            if (line.size()>=20){
                if (line.substr(0,20).compare(L"Switch to MS/MS when")==0){
                    file_type = L"MS2";
                }
            }
            if (line.size()>=26){
                if (line.substr(0,26).compare(L"MSMS to MS Switch Criteria")==0){
                    file_type = L"MS2";
                }
            }
            if (line.size()>=32){
                if (line.substr(0,32).compare(L"MSMS Collision Energy Ramp Start")==0){
                    file_type = L"MS2";
                }
            }
            if (line.size() >= 21){
                if (line.substr(0, 21).compare(L"Parent Survey High CE") == 0){
                    ptrdiff_t pos = line.size() - 1;
                    while (line[pos] != '\t') pos--;
                    wstring val = line.substr(pos + 1, line.size());
                    info[function].push_back(L"Parent Survey High CE"); info[function].push_back(val);
                }
            }
            if (line.size() >= 20){
                if (line.substr(0, 20).compare(L"Parent Survey Low CE") == 0){
                    ptrdiff_t pos = line.size() - 1;
                    while (line[pos] != '\t') pos--;
                    wstring val = line.substr(pos + 1, line.size());
                    info[function].push_back(L"Parent Survey Low CE"); info[function].push_back(val);
                }
            }
        }
    }
    bool ms2 = false;
    for (unsigned int ii=1; ii<info.size(); ii++){
        if (ms_level[ii].size()>0){
            info[ii].push_back(L"Type"); info[ii].push_back(ms_level[ii]);
            if (ms_level[ii].compare(L"MS2")==0)
                ms2 = true;
        }
    }
    if (info.size()==2){
        info[0].push_back(L"Type"); info[0].push_back(ms_level[1]);
    } else if (info.size()>2){
        if (file_type.compare(L"MSE")==0){
            info[0].push_back(L"Type"); info[0].push_back(file_type);
        } else {
            info[0].push_back(L"Type"); info[0].push_back(ms2?L"MS2":L"MS1");
        }
    }

    for (unsigned int kk=0; kk<info.size(); kk++){
        bool has_ramp=false, has_msms_start=false; unsigned int indx=0;
        std::vector<wstring> entry = info[kk];
        for (unsigned int ii=0; ii<entry.size(); ii++){
            wstring str = entry[ii];
            if (str.compare(L"Ramp High Energy from")==0) has_ramp=true;
            if (str.compare(L"MSMS Start Mass")==0) has_msms_start=true;
            if (str.compare(L"Type")==0) indx=ii;
        }
        if (has_msms_start && indx<entry.size()-1){
            if (has_ramp){
                if (entry[indx+1].compare(L"MS1")==0)
                    info[kk][indx+1]=L"MS2";
            } else {
                if (entry[indx+1].compare(L"MS2")==0)
                    info[kk][indx+1]=L"MS1";
            }
        }
    }

    if (verbose==1) std::cout << " -- done reading metadata" << endl << endl;
    return info;
}

// get function type
int Reader::getWatersFunctionType(const wstring &raw_filename, unsigned int function_id)
{
    std::vector<int> function_types;
    if (!readWaterFunctionTypes(raw_filename, function_types)){
        wcerr << "can't find functions in file=" << raw_filename << endl; return -1;
    }
    if (function_id > function_types.size())
        return -1;    
    return function_types[function_id];
}

// read function type
bool Reader::readWaterFunctionTypes(const wstring &raw_filename, std::vector<int> &function_types)
{
    function_types.clear();
    PicoUtil * utl = new PicoUtil(); 
    if (!utl->dirExists(raw_filename)){
        wcerr << "can't find raw directry = " << raw_filename << endl; delete utl; return false;
    }
    wstring filename = utl->stripPathChar(raw_filename); filename.append(L"\\_FUNCTNS.INF");
#ifdef TEST_MISSING__FUNCTNS_INF
    filename.append("\\_FUNCTNS_XX.INF");
#else
    filename.append(L"\\_FUNCTNS.INF");
#endif
    int fns_file_size = (int)utl->getFileSize(filename);
    delete utl;
    int count = fns_file_size / 0x1a0;
    FILE* fnsFile = NULL;
    errno_t err = _wfopen_s(&fnsFile, filename.c_str(), L"rbS");
    if (err != 0){
        std::cout << "can't open functns file = " << filename.c_str() << "\"" << endl; return false;
    }

    unsigned char * fns_buf = (unsigned char *)malloc(fns_file_size*sizeof(unsigned char));
    if (fns_buf == NULL){
        std::cout << "mz buf alloc failed" << endl; fclose_safe(fnsFile); return false; //fclose_safe(fnsFile); 
    }

    size_t idx_count = fread(fns_buf, 1, fns_file_size, fnsFile);
    if (idx_count<(size_t)(unsigned int)fns_file_size){
        std::cout << "can't read fns file = " << filename.c_str() << endl;
    }
    fclose_safe(fnsFile); //fclose_safe(fnsFile);

    unsigned char* ptr = fns_buf; 
    for (int ii = 0; ii < count; ii++){
        unsigned char cc = *ptr; ptr += 0x1a0;
        unsigned int vali = (unsigned int) cc; function_types.push_back(vali);
    }
    free_safe(fns_buf); return true;
}

// get functions info metadata
bool Reader::getWatersFunctionInfo(const wstring &raw_filename, std::vector<FunctionRec> &function_info, int *ms_func_cnt)
{
    function_info.clear();
    PicoUtil * utl = new PicoUtil();
    if (!utl->dirExists(raw_filename)){
        wcerr << "can't find raw directry = " << raw_filename << endl; delete utl; return false;
    }

    wstring filename = utl->stripPathChar(raw_filename);
#ifdef TEST_MISSING__FUNCTNS_INF
    filename.append("\\_FUNCTNS_XX.INF");
#else
    filename.append(L"\\_FUNCTNS.INF");
#endif
    int fns_file_size = (int)utl->getFileSize(filename);
    delete utl;
    int count = fns_file_size / 0x1a0;
    FILE* fnsFile = NULL;
    errno_t err = _wfopen_s(&fnsFile, filename.c_str(), L"rbS");
    if (err != 0){
        std::cout << "can't open functns file = " << filename.c_str() << "\"" << endl; return false;
    }

    unsigned char * fns_buf = (unsigned char *)malloc(fns_file_size*sizeof(unsigned char));
    if (fns_buf == NULL){
        std::cout << "mz buf alloc failed" << endl; fclose_safe(fnsFile); return false; //fclose_safe(fnsFile); 
    }

    size_t fns_count = fread(fns_buf, 1, fns_file_size, fnsFile);
    if (fns_count<(size_t)fns_file_size){
        std::cout << "can't read fns file = " << filename.c_str() << endl;
    }
    fclose_safe(fnsFile);

    int ms_fcnt = 0;
    unsigned char* ptr = fns_buf;
    unsigned char* iptr = ptr;
    for (int ii = 0; ii < count; ii++){
        int type = (uint8_t)(*iptr) & 0x1f; iptr += 0x1a0;
        cout << ii << hex << ": " << type << endl;
        if (type == 0 || type == 6 || type == 11 || type == 13 || (type >= 15 && type <= 19))
            ms_fcnt++;
    }
    *ms_func_cnt = ms_fcnt;

    if (verbose) std::cout << "Functions:" << endl;
    bool accurate = false;
    for (int ii = 0; ii < count; ii++){
        unsigned char* iptr = ptr; 
        uint8_t type = (uint8_t)(*iptr++); 
        uint8_t mode = (uint8_t)(*iptr++);    // 71=MSn, F1=calib, 24=UVtrace; 25,31,A5,B1,B5
        bool continuum = mode & 0x1;
        uint8_t function_type = type & 0x1f;  // 0c=DAD, 0d=TOF, 0e=PSD, 0f=TOFS, 10=TOFD, 11=MTOF, 12=TOFMS, 13=TOFP, 14=ASPEC_VSCAN, 15=ASPEC_MAGNET, 16=ASPEC_VSIR, 17=ASPEC_MAGNET_SIR
        // 18=QUAD_AUTO_DAU, 19=ASPEC_BE, 1a=ASPEC_B2E, 1b=ASPEC_CNL, 1c=ASPEC_MIKES, 1d=ASPEC_MRM, 1e=ASPEC_NRMS, 1f=ASPEC_MAGNET_MRMQ
        uint8_t ion_mode = ((type >> 5) & 7) | ((mode & 0x1) << 3); // 0=EI+, 1=EI-, 2=CI+, 3=CI-, 4=FB+, 5=FB-, 6=TS+, 7=TS-, 8=ES+, 9=ES-, 10=AI+, 11=AI-, 12=LD+, 13=LD-, 14=FI+, 15=FP- 
        uint8_t data_type = (mode >> 2) & 0xf;// 0=Compressed, 1=Standard, 2=SIR or MRM, 3=Scanning Contimuum, 4=MCA, 5=MCA with SD, 6=MCB, 7=MCB with SD, 8=Molecular weight data, 
                                              // 9=High accuracy calibrated data, 10=Single float precision (not used), 11=Enhanced uncalibrated data, 12=Enhanced calibrated data, 13=Enhanced calibrated accurate mass data
        if (ion_mode>13)
            std::cout << "warning: ion_mode=" << ion_mode << " out of range" << std::endl;

        if (data_type == 0xd) // && last func?
            accurate = true;
        bool cal_function = isCalibrationFunction(mode);
        float scan_time = *(float *)iptr; iptr += 4;
        float survey_intescan_time = *(float *)iptr; iptr += 4;
        float survey_start_time = *(float *)iptr; iptr += 4;
        float survey_end_time = *(float *)iptr; iptr += 14;
        float interscan_time = *(float *)iptr; iptr += 4;
        float mass_window_pm = *(float *)iptr; iptr += 4;
        float survey_start_mass = *(float *)(ptr+0xa0);
        float survey_end_mass = *(float *)(ptr+0x120); ptr += 0x1a0;
        if (verbose){
            std::cout << "  _Func_" << (ii + 1) << ": type=0x" << hex << (int)type << ", mode=0x" << (int)mode << "\n    function_type=" << FunctionTypeString[function_type] << "(" << dec << (int)function_type << "), ion_mode=" << IonModeString[ion_mode] << "(" << (int)ion_mode << "), data_type=" << DataTypeString[data_type] << "(" << (int)data_type << "), continuum=" << (continuum ? "yes" : "no") << "(" << (continuum ? 1 : 0) << ")" << endl;
            std::cout << "    scan_time=" << scan_time << ", intescan_time=" << survey_intescan_time << ";   start_time=" << survey_start_time << ", end_time=" << survey_end_time << endl;
            std::cout << "    interscan_time=" << interscan_time << ", mass_window_pm=" << mass_window_pm << ";   start_mass=" << survey_start_mass << ", end_mass=" << survey_end_mass << endl;

            if (g_output_function_info) {
                FunctionRec rec(type, mode, accurate, cal_function, function_type, 
                                ion_mode, data_type, continuum, scan_time, 
                                survey_intescan_time, interscan_time, survey_start_time, 
                                survey_end_time, mass_window_pm, survey_start_mass, survey_end_mass);
                rec.function = ii+1;
                rec.filename = raw_filename.c_str();
                functionInfoMetaDataPrinter.pushFunctionInfoMetaData(rec);
            }
        }
        function_info.push_back(FunctionRec(type, mode, accurate, cal_function, function_type, ion_mode, data_type, continuum, scan_time, survey_intescan_time, interscan_time, survey_start_time, survey_end_time, mass_window_pm, survey_start_mass, survey_end_mass));
    }
    if (accurate){
        if (verbose) std::cout << "  accurate_data=" << (accurate?"yes":"no") << endl;
        for (int ii = 0; ii < (int)function_info.size(); ii++){
            function_info[ii].accurate = true;
        }
    }
    free_safe(fns_buf);
    return true;
}

bool Reader::getWatersFunctionInfoOnlyUsingExternInf(wstring filename, std::vector<FunctionRec> &functionRecords)
{
    functionRecords.clear();
    std::vector<wstring> metadata = readMetadata(filename);
    if (metadata.size() == 0){
        return false;
    }
    std::vector<std::vector<wstring>> info = extractMetadata(metadata);
    for (int ii = 1; ii < (int)info.size(); ii++){
        std::vector<wstring> info_i = info[ii]; bool found = false;
        for (int jj = (int)info_i.size() - 1; jj >= 0; jj--){
            wstring sti = info_i[jj];
            if (sti.compare(L"Type") == 0){
                if (jj < (int)info_i.size() - 1){
                    wstring stx = info_i[jj + 1];
                    if (stx.length() > 2){
                        wchar_t cc = stx[2];
                        int type = cc - L'0';
                        if (type >= 0 && type <= 9){
                            found = true;
                            int funtionType = 1, funtionMode = 1;  //Let's assume we have MSE by default.
                            if (type == 1){
                                funtionType = 1; funtionMode = FunctionMode_MSN;
                            } else if (type == 2){
                                funtionType = 2; funtionMode = FunctionMode_MSN;
                            } else if (type == 9){
                                funtionType = 9; funtionMode = FunctionMode_Calibrate;
                            } else {
                                funtionType = 1; funtionMode = FunctionMode_MSN;
                            }
                            functionRecords.push_back(FunctionRec((uint8_t)funtionType, (uint8_t)funtionMode, false, ((funtionMode & 0x80)>0), 0, 0, 0, false, 0, 0, 0, 0, 0, 0, 0, 0));
                        }
                    }
                }
            }
        }
    }
    return true;
}

// get reference function index
int Reader::getReferenceFunctionIndex(const std::vector<std::vector<std::wstring>>& info)
{
    //int FilesId = 1;
    //bool profile_mode = true;  // true for MS1, false for centroid or MS2
    //string MS_Type;
    for (unsigned int ii=1; ii<info.size(); ii++){
        std::vector<wstring> info_i = info[ii];
        for (unsigned int kk=0; kk<info_i.size(); kk++){
            wstring entry = info_i[kk];
            if (entry.size()>=4){
                if (entry.compare(L"Type")==0){
                    if (kk<info[0].size()-1){
                        wstring next = info_i[kk+1];
                        if (next.size()>=3){
                            if (next.substr(0,3).compare(L"REF")==0){
                                return ii;
                            }
                        }
                    }
                }
            }
        }
    }
    return -1;
}

// read uv_trace
std::vector<int> Reader::readUvTraceData(const wstring &uv_filename)
{
    std::vector<int> data;
    int file_size = (int)getFileSize(uv_filename);
    if (file_size<0){
        std::wcerr << "can't open idx file = " << uv_filename << "\"" << endl; return data;
    }
    if (file_size==0){
        std::wcerr << "empty idx file = " << uv_filename << "\"" << endl; return data;
    }
    if (verbose==1) std::cout << "idx file contains " << file_size << " bytes" << endl;

    FILE* uvFile = NULL;
    errno_t err = _wfopen_s(&uvFile, uv_filename.c_str(), L"rbS");
    if (err != 0){
        wcerr << "can't open idx file = " << uv_filename << "\"" << endl; return data;
    }

    unsigned char * uv_buf = (unsigned char *)malloc(file_size*sizeof(unsigned char)); 
    if (uv_buf==NULL){
        std::cout << "mz buf alloc failed" << endl; fclose_safe(uvFile); return data; //fclose_safe(uvFile);
    } 

    size_t count = fread(uv_buf, 1, file_size, uvFile); 
    if (count<(size_t)file_size){
        wcerr << "can't read from uv trace file = " << uv_filename << endl;
    }
    unsigned char* ptx = (unsigned char*)uv_buf;
    fclose_safe(uvFile); //fclose_safe(uvFile);

    int size = file_size / 6; unsigned int mz0 = 0;
    for (int ii=0; ii<size; ii++){ 
        unsigned int bb = *ptx++; // 24-bit sensor
        unsigned int bb1 = *ptx++;
        unsigned int bb2 = *ptx++; 
        unsigned int intens = (bb2<<16) | (bb1<<8) | bb; 

        bb = *ptx++;
        bb1 = *ptx++;
        bb2 = *ptx++;
        unsigned int mz = (bb<<16) | (bb1<<8) | bb2; 

        if (ii==0){
            mz0 = mz; data.push_back(mz); 
        } else {
            if (mz != mz0){
                std::cout << "caution: different uv freq = " << mz << " nm, orig=" << mz0 << endl;
            }
        }
        data.push_back(intens-0x800000); 
    }
    free_safe(uv_buf); 
    PicoUtil* utl = new PicoUtil();
    utl->writeIntToFile(data, wstring(L"d:\\uv_data.csv"));
    delete utl;
    return data;
}

std::vector<wstring> Reader::readUvMetadata(const wstring &raw_dir)
{
    std::vector<wstring> uv_metadata;
    PicoUtil *utl = new PicoUtil();
    wstring uv_filename = utl->stripPathChar(raw_dir); uv_filename.append(L"\\_INLET.INF");
    if (!utl->fileExists(uv_filename)){
        delete utl; return uv_metadata;
    }
    std::vector<wstring> metadata = readMetadata(uv_filename);
    wstring channel;
    for (int ii=0; ii<(int)metadata.size(); ii++){
        wstring dati = metadata[ii];
        if (dati.size()>18){
            if (dati.substr(0,18).compare(L"Waters Acquity TUV")==0){
                uv_metadata.push_back(L"Type"); uv_metadata.push_back(L"Waters Acquity TUV");
            }
        }
        if (dati.size()>17){
            if (dati.substr(0,17).compare(L" Wavelength Mode:")==0){
                uv_metadata.push_back(L"WavelengthMode"); uv_metadata.push_back(dati.substr(18, dati.size()));
            }
        } 
        if (dati.size()>8){
            if (dati.substr(0,8).compare(L"Channel ")==0){
                channel = dati[8];
            }
        }
        if (dati.size()>12){
            if (dati.substr(0,12).compare(L" Wavelength:")==0){
                wstring chan(L"Channel_"); chan.append(channel); chan.append(L"_Wavelength");
                uv_metadata.push_back(chan); uv_metadata.push_back(dati.substr(13, dati.size()));
            }
        } 
        if (dati.size()>15){
            if (dati.substr(0,15).compare(L" Sampling Rate:")==0){
                wstring chan(L"Channel_"); chan.append(channel); chan.append(L"_SamplingRate");
                uv_metadata.push_back(chan); uv_metadata.push_back(dati.substr(16, dati.size()));
            }
        } 
        if (dati.size()>11){
            if (dati.substr(0,11).compare(L" Data Mode:")==0){
                wstring chan(L"Channel_"); chan.append(channel); chan.append(L"_DataMode");
                uv_metadata.push_back(chan); uv_metadata.push_back(dati.substr(12, dati.size()));
            }
        }
    }
    delete utl;
    return uv_metadata;
}

//check for MSE file
bool Reader::checkMSE(const std::vector<std::vector<wstring>> info)
{
    //Criteria #1
    //raw file contains at least 2 or more functions labeled as "TOF PARENT FUNCTIONS"
    int tofParentFunctionCnt = 0;
    for (unsigned int function=1; function<info.size(); function++){
        std::vector<wstring> info_i = info[function];
        for (int unsigned ii = 0; ii < info_i.size(); ii++){
            wstring stri = info_i[ii];
            if (stri.compare(L"TOF PARENT FUNCTION") == 0)
                tofParentFunctionCnt++;
        }
    }
    if (tofParentFunctionCnt < 2)
        return false;

    //bool isMSE = false;
    //Criteria #2
    //the first of these functions (typically Function 1, but may not always be) contains a [PARENT MS SURVEY] section, which includes *BOTH* of the following line entries:
    //a) Parent Survey High CE (v)   xxx
    //b) Parent Survey Low CE (v)    yyy
    //c) xxx >= yyy*1.5
    double f1_high_energy = -1.0, f1_low_energy = -1.0; int f1_index = -1;
    for (unsigned int function = 1; function<info.size(); function++){
        std::vector<wstring> info_i = info[function];
        for (int unsigned ii = 0; ii < info_i.size(); ii++){
            wstring stri = info_i[ii];
            if (stri.compare(L"Parent Survey High CE") == 0)
                if (ii<info_i[ii].size()-1)
                    f1_high_energy = _wtof(info_i[++ii].c_str());
            if (stri.compare(L"Parent Survey Low CE") == 0)
                if (ii<info_i[ii].size() - 1)
                    f1_low_energy = _wtof(info_i[++ii].c_str());
        }
        if (f1_low_energy >= 0.0 && f1_high_energy >= 0.0){
            if (f1_high_energy < 1.5*f1_low_energy)
                return false;
            f1_index = function; break;
        }
    }

    //Criteria #3
    //ALL subsequent "TOF PARENT FUNCTIONS" functions (other than the first) also contain a [PARENT MS SURVEY] section, which includes one of the following entries:
    //1) Ramp High Energy from   uuu to zzz
    //  1a) zzz is larger than uuu 
    //  1b) BOTH uuu and zzz are larger than yyy
    //2) Parent Survey High CE & Parent Survey Low CE
    int fcnt = 0;
    if (f1_index > 0){
        for (unsigned int function = f1_index+1; function < info.size(); function++){
            double high_energy = -1.0, low_energy = -1.0, ramp_high = -1.0, ramp_low = -1.0;
            std::vector<wstring> info_i = info[function];
            bool tof_parent = false;
            for (int unsigned ii = 0; ii < info_i.size(); ii++){
                wstring stri = info_i[ii];
                if (stri.compare(L"TOF PARENT FUNCTION") == 0){
                    tof_parent = true; break;
                }
            }
            if (!tof_parent)
                continue;
            for (int unsigned ii = 0; ii < info_i.size(); ii++){
                wstring stri = info_i[ii];
                if (stri.compare(L"Parent Survey High CE") == 0)
                    if (ii < info_i[ii].size() - 1)
                        high_energy = _wtof(info_i[++ii].c_str());
                if (stri.compare(L"Parent Survey Low CE") == 0)
                    if (ii < info_i[ii].size() - 1)
                        low_energy = _wtof(info_i[++ii].c_str());
                if (stri.compare(L"Ramp High Energy from") == 0){
                    if (ii < info_i[ii].size() - 1){
                        wstring val = info_i[++ii];
                        size_t pos = val.find(L" to ");
                        if (pos != val.npos){
                            ramp_low = _wtof(val.substr(0, pos).c_str());
                            ramp_high = _wtof(val.substr(pos + 4, val.npos).c_str());
                        }
                    }
                }
            }
            bool ce = low_energy >= 0.0 && high_energy >= 0.0 && high_energy > low_energy;
            bool ramp = ramp_high >= 0.0 && ramp_low >= 0.0 && ramp_high > ramp_low && ramp_low > f1_low_energy;
            if (!ce && !ramp){
                return false;
            }
            fcnt++;
        }
    }
    if (f1_index>0 && fcnt>0)
        return true;
    return false;
}

// identify function mode
bool Reader::identifyFunctionMode(unsigned int function, int max_scan, int mz_len, 
    bool f_type_v3c, bool ms_type_6, bool cal_offset, const std::wstring &raw_dir, const std::vector<unsigned int> &v3c_tab, 
    const unsigned short *fact_tab, const unsigned char* idx_buf, int idx_size, FILE* inFile, CentroidMode *centroid_mode)
{
    *centroid_mode = UndeterminedData;
    unsigned char* ptx = (unsigned char*)idx_buf;
    unsigned int z_cnt = 0, max_mzlen = 0, imax_mzlen = 0;
    bool is_centroid_data = false;
    double prev_mzi = 0;
    for (int scan = 0; scan<max_scan; scan++){
        ptx += 4;
        unsigned int bb = *ptx++;
        unsigned int bb1 = *ptx++;
        unsigned int bb2 = (*ptx++ & 0x3f);
        unsigned int mz_len = (bb2 << 16) | (bb1 << 8) | bb;
        if (mz_len > max_mzlen){
            max_mzlen = mz_len; 
            imax_mzlen = scan;
        }
        if (idx_size == 22){
            ptx += 15;
        } else {
            ptx += 23;
        }

        size_t size = (size_t)(ms_type_6 ? (f_type_v3c ? 12 : 6) : (f_type_v3c ? 12 : cal_offset ? 12 : 8))*mz_len*sizeof(unsigned char);
        unsigned char * buf = (unsigned char *)malloc(size);
        if (buf == NULL){
            std::cout << "read buf alloc failed" << endl; 
            if (is_centroid_data){
                *centroid_mode = CentroidData; // use default metadata
            }
            return false;
        }

        wstring name_prefix(L"_FUNC");
        wstring name = appendZeroPaddedNumberForWatersFile(name_prefix, function);
        wstring name1 = name + L".DAT";
        wstring filename = raw_dir + name1;

        size_t count = fread(buf, 1, size, inFile);
        if (count<size){
            std::cout << "caution: early finish while reading data from file = \"" << filename.c_str() << "\"" << endl;
            free_safe(buf); 
            break;
        }
        
        if (mz_len < MIN_CENTROID_LEN){
            free_safe(buf); 
            continue;
        }

        std::vector<std::tuple<double, unsigned int, double>> comb_data; 
        comb_data.reserve(mz_len);
        if (f_type_v3c){
            unsigned int* pti = (unsigned int *)buf;
            for (unsigned int ii = 0; ii<mz_len; ii++){
                unsigned int raw_intens = *pti++;
                double dbl_intensity = decodeIntensityType1_V3C(raw_intens, v3c_tab, true);
                unsigned int decoded_intens = (unsigned int)(dbl_intensity + .5);
                if (decoded_intens == 0){
                    z_cnt++;
                }
                unsigned int mz = *pti++;
                double mzi = decodeMzType1_6_V3C(mz);
                double delta = mzi - prev_mzi;
                comb_data.push_back(std::tuple<double, unsigned int, double>(mzi, decoded_intens, delta));
                prev_mzi = mzi;
                pti++;
            }
        } else {
            if (ms_type_6){
                unsigned char* ptc = (unsigned char*)buf;
                for (unsigned int ii = 0; ii < mz_len; ii++){
                    unsigned char* ptci = ptc + 6 * ii, *ptcm = ptci + 2;
                    unsigned int decoded_intens = *(unsigned short*)(ptci), mz = *(unsigned int*)(ptcm);
                    int scl = mz & 0xF, fact = fact_tab[scl];
                    unsigned int intensity = decoded_intens*fact; //ptc+=2;                 
                    if (intensity == 0){
                        z_cnt++;
                    }
                    double mzi = decodeMzType1_6(mz & 0xFFFFFFF0);
                    double delta = mzi - prev_mzi;
                    comb_data.push_back(std::tuple<double, unsigned int, double>(mzi, decoded_intens, delta));
                    prev_mzi = mzi;
                }
            } else {
                unsigned int* pti = (unsigned int *)buf;
                for (unsigned int ii = 0; ii<mz_len; ii++){
                    unsigned int raw_intens = *pti++, decoded_intens = decodeIntensityType1(raw_intens);
                    if (decoded_intens == 0){
                        z_cnt++;
                    }
                    unsigned int mz = *pti++; 
                    double mzi = decodeMzType1(mz);
                    double delta = mzi - prev_mzi;
                    comb_data.push_back(std::tuple<double, unsigned int, double>(mzi, decoded_intens, delta));
                    prev_mzi = mzi;
                    if (cal_offset){
                        pti++;
                    }
                }
            }
        }
        
        free_safe(buf);
        if (z_cnt > 0){ // must be profile
            *centroid_mode = ProfileData;
            return true;
        }

        if (comb_data.size() > MIN_CENTROID_LEN){
            bool monotone = true;
            double prev_delta = std::get<2>(comb_data[1]);
            for (unsigned int ii = 1; ii < comb_data.size(); ii++){
                double delta = std::get<2>(comb_data[ii]);
                double diff = abs(delta - prev_delta);
                if (diff > MIN_DELTA){
                    monotone = false; 
                    break;
                }
                prev_delta = delta;
            }
            if (monotone){
                *centroid_mode = ProfileData;
            } else {
                *centroid_mode = CentroidData;
            }
            return true;
        }
    }
    std::cout << "can't auto-determine centroid mode -- all scans are fewer than " << MIN_CENTROID_LEN << " points, revert to metadata" << endl;
    return false;
}

// get file size (in bytes)
long long Reader::getFileSize(const wstring &in_filename)
{
    struct _stat64 stat;
    if (_wstat64(std::wstring(in_filename.begin(), in_filename.end()).c_str(), &stat) < 0){
        wcerr << "can't find input file=" << in_filename << endl; return -1;
    }
    return stat.st_size;
}


// write tic to file
bool Reader::writeTicToFile(const std::vector<TicRec> tic, const wstring &out_filename)
{
    FILE* outFile = NULL;
    errno_t err = _wfopen_s(&outFile, out_filename.c_str(), L"w");
    if (err != 0){
        wcerr << "can't open file = " << out_filename << endl; return false;
    }
    for (unsigned int ii=0; ii<tic.size(); ii++){
        TicRec trec = tic[ii];
        fprintf(outFile, "%f %f\n", trec.retention_time, trec.tic_value);
    }
    fflush(outFile); fclose_safe(outFile); //fclose_safe(outFile);
    return true;
}

// set verbose level: 0=none, n=every n-th line (n>=1)
void Reader::setVerboseLevel(int verbose_level1)
{
    verbose = verbose_level1;
}

// set uncertainty scaling
void Reader::setUncertaintyScaling(UncertaintyScaling uncert_type1)
{
    uncert_type = uncert_type1;
}

// get uncertainty scaling
UncertaintyScaling Reader::getUncertaintyScaling()
{
    return uncert_type;
}

// set uncertainty value
bool Reader::setUncertaintyValue(double uncert_val1)
{
    if (uncert_val1>0){
        uncert_val = uncert_val1; return true;
    } else {
        if (verbose==1) std::cout << "non valid uncertainty value -- reset to default 0.01" << endl;
        uncert_val = 0.01; return false;
    }
}

// get uncertainty scaling
double Reader::getUncertaintyValue()
{
    return uncert_val;
}

// set merge radius
bool Reader::setMergeRadius(double merge_factor1){
    if (merge_factor1<0.5 || merge_factor1>10.0){
        if (verbose == 1) std::cout << "non valid merge_radius value -- reset to default of 4.0*uncertainty_value" << endl;
        merge_radius = 4.0*uncert_val; return true;
    } else {
        merge_radius = merge_factor1*uncert_val; return true;
    }
}

// get merge radius
double Reader::getMergeRadius(){
    return merge_radius;
}

// set uncertainty value
bool Reader::setTopK(int top_k1)
{
    if (top_k1>0){
        top_k = top_k1; return true;
    } else if (top_k1==-1){
        top_k = -1; return true;
    } else {
        if (verbose==1) std::cout << "non valid TopK value -- reset to default -1" << endl;
        top_k = -1; return false;
    }
}

// get uncertainty scaling
int Reader::getTopK()
{
    return top_k;
}

// set intact mass file
void Reader::setIdentifyFunctionMsTypeExpectNoZeros(bool intact_mode1)
{
    m_identifyFunctionMsTypeExpectNoZeros = intact_mode1;
}

// get intact mass file
bool Reader::getIdentifyFunctionMsTypeExpectNoZeros() const
{
    return m_identifyFunctionMsTypeExpectNoZeros;
}

// disable profile centroiding
void Reader::disableProfileCentroid()
{
    disable_profile_centroiding = true;
}

_PICO_END

