// ********************************************************************************************
// PicoLocalDecompress.cpp : The main decompression module
//
//      @Author: D.Kletter    (c) Copyright 2014, PMI Inc.
// 
// This module reads in a highly compressed byspec2 file and produces an uncompressed byspec2 file.
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
// PicoLocalDecompress::decompressSpectra(unsigned char** compressed, double** restored_mz, float** restored_intensity, unsigned int* length)
// to decompress one scanline at a time. In order to support high performance, the input file is opened only
// once in the beginning, and an sqlite pointer is passed for subsequent spectra, thereby avoiding the extra
// overhead associated with any subsequent closing and re-opeining the input file for each scan. The caller
// is responsible for freeing the restored mz and intensity memory after they have been been written or copied.
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
#endif
#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <vector>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include "Sqlite.h"
#include "PicoLocalDecompress.h"
#include "PicoLocalCompress.h"
#include "Reader.h"
#include "Centroid.h"
#include "WatersCalibration.h"
#include "PicoUtil.h"

using namespace std;
using namespace pico;

_PICO_BEGIN

string commas2(unsigned long long n)
{
  string s;
  int cnt = 0;
  do
  {
    s.insert(0, 1, char('0' + n % 10));
    n /= 10;
    if (++cnt == 3 && n)
    {
      s.insert(0, 1, ',');
      cnt = 0;
    }
  } while (n);
  return s;
}

PicoLocalDecompress::PicoLocalDecompress()
{
    //LSB_FACTOR = 4;
    verbose = false;
}

// constructor
PicoLocalDecompress::PicoLocalDecompress(bool verbose1)
{
    //LSB_FACTOR = 4;
    verbose = verbose1;
}

PicoLocalDecompress::~PicoLocalDecompress()
{
}

// decompress file
bool PicoLocalDecompress::decompress(const wstring &input_compressed_db_filename, 
                                     const wstring &out_uncompressed_db_filename,
                                     CompressType type, int single_scan, bool restore_zero_peaks)
{
    if (type==Type_Bruker1){
        return decompressType0(input_compressed_db_filename, out_uncompressed_db_filename);

    } else if (type==Type_Waters1){
        return decompressType1(input_compressed_db_filename, out_uncompressed_db_filename, single_scan, restore_zero_peaks);

    } else if (type==Type_Bruker2){
        return decompressType02(input_compressed_db_filename, out_uncompressed_db_filename);

    } else if (type==Type_Centroided1){
        return decompressTypeCentroided1(input_compressed_db_filename, out_uncompressed_db_filename, single_scan);

    } else if (type==Type_ABSciex1){
        return decompressType4(input_compressed_db_filename, out_uncompressed_db_filename, single_scan, restore_zero_peaks);

    } else {
        cout << "unsupported decompression type" << endl;
        return false;
    }
}

// decompress type 0 file
bool PicoLocalDecompress::decompressType0(const wstring &input_compressed_db_filename, const wstring &out_uncompressed_db_filename)
{
    wstring out_filename = out_uncompressed_db_filename;
    if (out_filename.length() == 0){
        out_filename = input_compressed_db_filename + L"_uncompressed.byspec2";
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
    if (!sql_out->createCopyByspecTablesToSqlite(input_compressed_db_filename, false)){
        wcerr << "can't create table in sqlite database = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }


    Sqlite* sql_in = new Sqlite(input_compressed_db_filename);
    if(!sql_in->openDatabase()){
        wcerr << "can't open output sqlite database = " << input_compressed_db_filename << endl;
        delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 

    int scan_count = sql_in->getPeakBlobCount(); 
    if (scan_count<0){
        std::cout << "can't read peak blob count from sqlite database" << endl; 
        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if (verbose) std::cout << "\ndecompress:   spectrum contains " << scan_count << " scans" << endl;
        
    unsigned long long original_size = 0, compressed_size = 0;  
    int scan = 0, compressed_length = 0;
    sqlite3* db_in = sql_in->getDatabase(), *db_out = sql_out->getDatabase();
    sqlite3_stmt *stmt; bool success = false;
    sqlite3_exec(db_in, "BEGIN;", NULL, NULL, NULL);
    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    ostringstream cmd; cmd << "Select PeaksMz FROM Peaks";
    ostringstream cmd1; cmd1 << "INSERT into Peaks(PeaksMz,PeaksIntensity) values (?,?)";
    if (sqlite3_prepare_v2(db_in, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                compressed_length = sqlite3_column_bytes(stmt, 0);
                unsigned char* compressed = (unsigned char*)sqlite3_column_blob(stmt, 0);

                double* restored_mz = 0;
                float* restored_intensity = 0;
                int length = 0;
                auto start = chrono::system_clock::now();
                if (!decompressSpectraType0(&compressed, &restored_mz, &restored_intensity, &length)){
                    std::cout << "malloc out of memory while decompress spectra for scan = " << scan << endl; 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; free_safe(restored_mz); free_safe(restored_intensity); return false;
                }
                auto duration = chrono::system_clock::now() - start;
                auto decompress_millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

                sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                if ((rc1=sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                    sqlite3_bind_blob(stmt1, 1, (unsigned char*)restored_mz, length*8, SQLITE_STATIC); 
                    sqlite3_bind_blob(stmt1, 2, (unsigned char*)restored_intensity, length*4, SQLITE_STATIC); 
                    rc1 = sqlite3_step(stmt1);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt1);
                free_safe(restored_mz); free_safe(restored_intensity); 
                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                compressed_size += compressed_length; original_size += length*12; // 8 bytes mz + 4 bytes intensity 
                if (verbose) std::cout << "   scan: " << scan << ", compress=" << compressed_length << ", decomp=" << length*12 << " bytes (cr=" << setprecision(5) << (float)length*12/compressed_length << setprecision(0) << ") [" << decompress_millis << "]" << endl;
                scan++; success = true; 
                //if (scan==3)
                //    break;

            } else if (rc == SQLITE_DONE) {
                success = true; break;
            } else {
                if (verbose) cout << "rc=" << rc << endl; break;
            } 
        } 
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db_out, "COMMIT;", NULL, NULL, NULL);
    sqlite3_exec(db_in, "COMMIT;", NULL, NULL, NULL);
    if (verbose) std::cout << "done! overall compression=" << commas2(compressed_size).c_str() << " decomp=" << commas2(original_size).c_str() << " bytes (cr=" << setprecision(5) << (float)original_size/compressed_size << setprecision(0) << ")" << endl;
    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; 
    if (!success)
        cout << "can't read mz blob from sqlite -- " << string(sqlite3_errmsg(db_in)) << endl; 
    return success;
}



// decompress type 02 file
bool PicoLocalDecompress::decompressType02(const wstring &input_compressed_db_filename, const wstring &out_uncompressed_db_filename)
{
    wstring out_filename = out_uncompressed_db_filename;
    if (out_filename.length() == 0){
        out_filename = input_compressed_db_filename + L"_uncompressed.byspec2";
    }
    
    Sqlite* sql_out = new Sqlite(out_filename.c_str()); 
    if(!sql_out->openDatabase()){
        wcerr << "can't open output sqlite database = " << out_filename << endl;
        delete sql_out; return false;
    } 
    if(!sql_out->setMode()){
        wcerr << "can't set output sqlite mode = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }
    if (!sql_out->createCopyByspecTablesToSqlite(input_compressed_db_filename, false)){
        wcerr << "can't create table in sqlite database = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }


    Sqlite* sql_in = new Sqlite(input_compressed_db_filename);
    if(!sql_in->openDatabase()){
        wcerr << "can't open output sqlite database = " << input_compressed_db_filename << endl;
        delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 

    int scan_count = sql_in->getPeakBlobCount(); 
    if (scan_count<0){
        std::cout << "can't read peak blob count from sqlite database" << endl; 
        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if (verbose) std::cout << "\ndecompress:   spectrum contains " << scan_count << " scans" << endl;
        
    unsigned long long original_size = 0, compressed_size = 0;  
    int scan = 0, compressed_length = 0;
    sqlite3* db_in = sql_in->getDatabase(), *db_out = sql_out->getDatabase();
    sqlite3_stmt *stmt; bool success = false;
    sqlite3_exec(db_in, "BEGIN;", NULL, NULL, NULL);
    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    ostringstream cmd; cmd << "Select PeaksMz FROM Peaks";
    ostringstream cmd1; cmd1 << "INSERT into Peaks(PeaksMz,PeaksIntensity) values (?,?)";
    if (sqlite3_prepare_v2(db_in, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                compressed_length = sqlite3_column_bytes(stmt, 0);
                unsigned char* compressed = (unsigned char*)sqlite3_column_blob(stmt, 0);

                double* restored_mz = 0;
                float* restored_intensity = 0;
                int length = 0;
                auto start = chrono::system_clock::now();
                if (!decompressSpectraType02(&compressed, &restored_mz, &restored_intensity, &length)){
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; free_safe(restored_mz); free_safe(restored_intensity); return false;
                }
                auto duration = chrono::system_clock::now() - start;
                auto decompress_millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

                sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                if ((rc1=sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                    sqlite3_bind_blob(stmt1, 1, (unsigned char*)restored_mz, length*8, SQLITE_STATIC); 
                    sqlite3_bind_blob(stmt1, 2, (unsigned char*)restored_intensity, length*4, SQLITE_STATIC); 
                    rc1 = sqlite3_step(stmt1);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt1);
                free_safe(restored_mz); free_safe(restored_intensity); 
                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                compressed_size += compressed_length; original_size += length*12; // 8 bytes mz + 4 bytes intensity 
                if (verbose) std::cout << "   scan: " << scan << ", compress=" << compressed_length << ", decomp=" << length*12 << " bytes (cr=" << setprecision(5) << (float)length*12/compressed_length << setprecision(0) << ") [" << decompress_millis << "]" << endl;
                scan++; success = true; 
                //if (scan==3)
                //    break;

            } else if (rc == SQLITE_DONE) {
                success = true; break;
            } else {
                if (verbose) cout << "rc=" << rc << endl; break;
            } 
        } 
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db_out, "COMMIT;", NULL, NULL, NULL);
    sqlite3_exec(db_in, "COMMIT;", NULL, NULL, NULL);
    if (verbose) std::cout << "done! overall compression=" << commas2(compressed_size).c_str() << " decomp=" << commas2(original_size).c_str() << " bytes (cr=" << setprecision(5) << (float)original_size/compressed_size << setprecision(0) << ")" << endl;
    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; 
    if (!success)
        cout << "can't read mz blob from sqlite -- " << string(sqlite3_errmsg(db_in)) << endl; 
    return success;
}


// decompress type 1 file
bool PicoLocalDecompress::decompressType1(const wstring &input_compressed_db_filename, const wstring &out_uncompressed_db_filename, int single_scan, bool restore_zero_peaks)
{
    wstring out_filename = out_uncompressed_db_filename;
    if (out_filename.length() == 0){
        out_filename = input_compressed_db_filename + L"_uncompressed.byspec2";
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
    if (!sql_out->createCopyByspecTablesToSqlite(input_compressed_db_filename, false)){
        wcerr << "can't create table in sqlite database = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }


    Sqlite* sql_in = new Sqlite(input_compressed_db_filename.c_str()); 
    if(!sql_in->openDatabase()){
        wcerr << "can't open output sqlite database = " << input_compressed_db_filename << endl;
        delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 

    WatersCalibration wtrs_cal;
    if (!wtrs_cal.readFromSqlite(sql_in)){
        wcerr << "can't read calibration modification data from sqlite" << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }

/*    bool reader_compression = false; string type_value;
    if(sql_in->getPropertyValueFromCompressionInfo("Type", type_value)){
        if (type_value.substr(0, 19).compare("pico:reader:waters_")==0)
            reader_compression = true;
        if (!restore_zero_peaks){
            restore_zero_peaks=true; if (verbose) cout << "restore_zero_peaks switched on for now" << endl;
        }
    } */

    int scan_count = sql_in->getPeakBlobCount(); 
    if (scan_count<0){
        std::cout << "can't read peak blob count from sqlite database" << endl; 
        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if (verbose) std::cout << "\ndecompress:   spectrum contains " << scan_count << " scans" << endl;

    WatersCalibration::Coefficents cal_mod_coef; int cal_mod_func = 1, next_cal_mod_scan = 0;
    std::vector<double> calib_coefi;
    unsigned long long original_size = 0, compressed_size = 0; 
    unsigned int compressed_mz_length = 0, compressed_intens_length = 0; int scan = 0;//, prev_calib_id = 0;
    sqlite3* db_in = sql_in->getDatabase(), *db_out = sql_out->getDatabase();
    sqlite3_stmt *stmt; bool success = false;
    sqlite3_exec(db_in, "BEGIN;", NULL, NULL, NULL);
    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    ostringstream cmd; 
    if (single_scan>=0){
        scan = single_scan;
        cmd << "Select PeaksMz,CompressionInfoId FROM Peaks WHERE Id='" << single_scan << "'"; //<============= ,CalibrationId
    } else {
        cmd << "Select PeaksMz,PeaksIntensity,CompressionInfoId FROM Peaks"; // note performance hit in bringing nulls  ,CalibrationId
    }
    ostringstream cmd1; cmd1 << "INSERT into Peaks(PeaksMz,PeaksIntensity,PeaksCount) values (?,?,'";
    if (sqlite3_prepare_v2(db_in, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                compressed_mz_length = sqlite3_column_bytes(stmt, 0);
                //int pk_id = sqlite3_column_int(stmt, 0);
                unsigned char* compressed_mz = (unsigned char*)sqlite3_column_blob(stmt, 0);
                unsigned char* compressed_intensity = (unsigned char*)sqlite3_column_blob(stmt, 1);
                int compress_info_id = sqlite3_column_int(stmt, 2);
/*                int calibration_id = sqlite3_column_int(stmt, 3);
                if (calibration_id != prev_calib_id){
                    sqlite3_stmt *stmt2; bool success2 = false; int start = 0; 
                    ostringstream cmd2; cmd2 << "SELECT MzCoeffs FROM Calibration WHERE ID='" << calibration_id << "'"; //<=============
                    if (sqlite3_prepare_v2(db_in, cmd2.str().c_str(), -1, &stmt2, NULL) == SQLITE_OK){
                        int rc2 = sqlite3_step(stmt2); calib_coefi.clear();
                        if (rc2 == SQLITE_ROW){
                            string str = (char*)sqlite3_column_text(stmt2, 0);
                            if (!parseCalibrationString(str, calib_coefi)){
                                cout << "can't parse calib data = " << str << "\"" << endl; return false;
                            }
                            start = calib_coefi.size(); success2 = true;
                            prev_calib_id = calibration_id;
                        } 
                        for (int jj=start; jj<6; jj++){
                            if (jj==1) calib_coefi.push_back(1.0);
                            else calib_coefi.push_back(0.0);
                        }
                    }
                    sqlite3_finalize(stmt2);
                    if (!success2){
                        if (verbose) cout << "can't read calibration data from sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                        sqlite3_finalize(stmt); 
                        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                    }
                }*/
                if (scan == next_cal_mod_scan-1){
                    std::vector<int> max_scan_id = wtrs_cal.getMaxScanId();
                    if (cal_mod_func < (int)max_scan_id.size())
                        next_cal_mod_scan = max_scan_id[cal_mod_func - 1];
                    cal_mod_coef = wtrs_cal.calibrationModificationCoefficients(cal_mod_func++);
                }
/*
                sqlite3_stmt *stmt2; bool success2 = false;
                ostringstream cmd2; cmd2 << "SELECT NativeId FROM Spectra WHERE Id='" << pk_id << "'"; //<=============
                if (sqlite3_prepare_v2(sql_in->getDatabase(), cmd2.str().c_str(), -1, &stmt2, NULL) == SQLITE_OK){
                    int rc2 = sqlite3_step(stmt2);
                    if (rc2 == SQLITE_ROW || rc2 == SQLITE_DONE){
                        char* text = (char*)sqlite3_column_text(stmt2, 0);
                        if (text != NULL){
                            string str(text);
                            const char *sptr = strstr(str.c_str(), "function=");
                            if (sptr != NULL){
                                int function = atoi(sptr + 9);
                                if (function != cal_mod_func){
                                    cal_mod_coef = wtrs_cal.calibrationModificationCoefficients(function);
                                    cal_mod_func = function;
                                }
                            }
                        }
                        success2 = true;
                    }
                }
                sqlite3_finalize(stmt2);
                if (!success2){
                    if (verbose) cout << "can't read calibration modification data from sqlite -- " << string(sqlite3_errmsg(db_out)) << endl;
                    sqlite3_finalize(stmt);
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }
*/
                double* restored_mz = 0; float* restored_intensity = 0;
                int restored_mz_length = 0;//, restored_intens_length = 0;
                long long decompress_millis;
                auto start = chrono::system_clock::now();

                if (compress_info_id==WATERS_READER_INFO_ID){ //reader_compression
                    if (!decompressSpectraType1(&compressed_mz, &compressed_intensity, &cal_mod_coef, compress_info_id, compressed_mz_length, &restored_mz, &restored_intensity, &restored_mz_length, restore_zero_peaks, false)){
                        std::cout << "  occured at decompress type 1 mz spectra for scan = " << scan << endl; 
                        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                    }
                    auto duration = chrono::system_clock::now() - start;
                    decompress_millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

                    sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                    ostringstream cmd2; 
                    cmd2 << cmd1.str() << restored_mz_length << "')";
                    if ((rc1=sqlite3_prepare_v2(db_out, cmd2.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                        sqlite3_bind_blob(stmt1, 1, (unsigned char*)restored_mz, restored_mz_length*8, SQLITE_STATIC); 
                        sqlite3_bind_blob(stmt1, 2, (unsigned char*)restored_intensity, restored_mz_length*4, SQLITE_STATIC); 
                        rc1 = sqlite3_step(stmt1);
                        if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                            success1 = true;
                        } 
                    }
                    sqlite3_finalize(stmt1);
                    if (compressed_mz != (unsigned char*)restored_mz)
                        free_safe(restored_mz); 
                    if (compressed_intensity != (unsigned char*)restored_intensity)
                        free_safe(restored_intensity); 

                    if (!success1){
                        if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                        sqlite3_finalize(stmt); 
                        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                    }
                    original_size += restored_mz_length*12; // 8 bytes mz + 4 bytes intensity 

                } else if (compress_info_id==WATERS_READER_CENTROIDED_INFO_ID){ //reader_centroid, with centroid compression

                    //double *packed_mz = 0; float *packed_intensity = 0; int packed_mz_length = 0;
                    auto duration = chrono::system_clock::now() - start;
                    if (!decompressSpectraType1(&compressed_mz, &compressed_intensity, &cal_mod_coef, compress_info_id, compressed_mz_length, &restored_mz, &restored_intensity, &restored_mz_length, restore_zero_peaks, false)){ //&packed_mz, &packed_intensity, &packed_mz_length, restore_zero_peaks, scan)){
                        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                    }

                    decompress_millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

                    sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                    ostringstream cmd2; 
                    cmd2 << cmd1.str() << restored_mz_length << "')";
                    if ((rc1=sqlite3_prepare_v2(db_out, cmd2.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                        sqlite3_bind_blob(stmt1, 1, (unsigned char*)restored_mz, restored_mz_length*8, SQLITE_STATIC); 
                        sqlite3_bind_blob(stmt1, 2, (unsigned char*)restored_intensity, restored_mz_length*4, SQLITE_STATIC); 
                        rc1 = sqlite3_step(stmt1);
                        if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                            success1 = true;
                        } 
                    }
                    sqlite3_finalize(stmt1);
                    free_safe(restored_mz); free_safe(restored_intensity); 
                    if (!success1){
                        if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                        sqlite3_finalize(stmt); 
                        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                    }
                    original_size += restored_mz_length*12; // 8 bytes mz + 4 bytes intensity 

                } else if (compress_info_id==WATERS_CENTROID_NO_COMPRESSION_ID){ //reader_centroid, no compression
                    auto duration = chrono::system_clock::now() - start;
                    decompress_millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

                    sqlite3_stmt *stmt1; bool success1 = false; int rc1; 
                    ostringstream cmd2; 
                    cmd2 << cmd1.str() << compressed_mz_length/8 << "')";
                    if ((rc1=sqlite3_prepare_v2(db_out, cmd2.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                        sqlite3_bind_blob(stmt1, 1, (unsigned char*)compressed_mz, compressed_mz_length, SQLITE_STATIC); 
                        sqlite3_bind_blob(stmt1, 2, (unsigned char*)compressed_intensity, compressed_mz_length/2, SQLITE_STATIC); 
                        rc1 = sqlite3_step(stmt1);
                        if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                            success1 = true;
                        } 
                    }
                    sqlite3_finalize(stmt1);
                    free_safe(restored_mz); free_safe(restored_intensity); 
                    if (!success1){
                        if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                        sqlite3_finalize(stmt); 
                        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                    }
                    restored_mz_length = compressed_mz_length+compressed_mz_length/2; //no compression 8 bytes mz + 4 bytes intensity 
                    original_size += restored_mz_length; 
                    restored_mz_length /= 12;

                } else { // non-reader decompression

                    if (!decompressMzType1(&compressed_mz, &restored_mz, &restored_mz_length)){
                        std::cout << "  occured at decompress mz spectra for scan = " << scan << endl; 
                        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                    }
                
                    float* restored_intensity = 0;
                    int intens_length = 0;
                    start = chrono::system_clock::now();
                    if (!decompressIntensityType1(&compressed_intensity, &restored_intensity, &intens_length)){
                        std::cout << "  occured at decompress intensity spectra for scan = " << scan << endl; 
                        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                    }
                    auto duration = chrono::system_clock::now() - start;
                    decompress_millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

                    sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                    ostringstream cmd2; 
                    cmd2 << cmd1.str() << restored_mz_length << "')";
                    if ((rc1=sqlite3_prepare_v2(db_out, cmd2.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                        sqlite3_bind_blob(stmt1, 1, (unsigned char*)restored_mz, restored_mz_length*8, SQLITE_STATIC); 
                        sqlite3_bind_blob(stmt1, 2, (unsigned char*)restored_intensity, intens_length*4, SQLITE_STATIC); 
                        rc1 = sqlite3_step(stmt1);
                        if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                            success1 = true;
                        } 
                    }
                    sqlite3_finalize(stmt1);
                    free_safe(restored_mz); free_safe(restored_intensity); 
                    if (!success1){
                        if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                        sqlite3_finalize(stmt); 
                        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                    }
                    original_size += restored_mz_length*12; // 8 bytes mz + 4 bytes intensity 
                }

                int compressed_length = compressed_mz_length + compressed_intens_length;
                compressed_size += compressed_length; 
                if (verbose) std::cout << "   scan: " << scan << ", compress=" << compressed_length << ", decomp=" << restored_mz_length*12 << " bytes (cr=" << setprecision(5) << (float)restored_mz_length*12/compressed_length << setprecision(0) << ") [" << decompress_millis << "]" << endl;
                scan++; success = true; 
                //if (scan==3)
                //    break;

            } else if (rc == SQLITE_DONE) {
                success = true; break;
            } else {
                if (verbose) cout << "rc=" << rc << endl; break;
            } 

            //if (scan > 10) 
            //    break;
        } 
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db_out, "COMMIT;", NULL, NULL, NULL);
    sqlite3_exec(db_in, "COMMIT;", NULL, NULL, NULL);
    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; 
    if (verbose) std::cout << "done! overall compression=" << commas2(compressed_size).c_str() << " decomp=" << commas2(original_size).c_str() << " bytes (cr=" << setprecision(5) << (float)original_size/compressed_size << setprecision(0) << ")" << endl;
    return success;
}


// decompress centroided type 1 file
bool PicoLocalDecompress::decompressTypeCentroided1(const wstring &input_compressed_db_filename, const wstring &out_uncompressed_db_filename, int single_scan)
{
    wstring out_filename = out_uncompressed_db_filename;
    if (out_filename.length() == 0){
        out_filename = input_compressed_db_filename + L"_uncompressed.byspec2";
    }
    
    Sqlite* sql_out = new Sqlite(out_filename.c_str()); 
    if(!sql_out->openDatabase()){
        wcerr << "can't open output sqlite database = " << out_filename << endl;
        delete sql_out; return false;
    } 
    if(!sql_out->setMode()){
        wcerr << "can't set output sqlite mode = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }
    if (!sql_out->createCopyByspecTablesToSqlite(input_compressed_db_filename, false)){
        wcerr << "can't create table in sqlite database = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }


    Sqlite* sql_in = new Sqlite(input_compressed_db_filename);
    if(!sql_in->openDatabase()){
        wcerr << "can't open output sqlite database = " << input_compressed_db_filename << endl;
        delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 

    int scan_count = sql_in->getPeakBlobCount(); 
    if (scan_count<0){
        std::cout << "can't read peak blob count from sqlite database" << endl; 
        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if (verbose) std::cout << "\ndecompress:   spectrum contains " << scan_count << " scans" << endl;

    unsigned long long original_size = 0, compressed_size = 0;  
    int scan = 0, compressed_mz_length = 0, compressed_intens_length = 0;
    sqlite3* db_in = sql_in->getDatabase(), *db_out = sql_out->getDatabase();
    sqlite3_stmt *stmt; bool success = false;
    sqlite3_exec(db_in, "BEGIN;", NULL, NULL, NULL);
    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    ostringstream cmd; 
    if (single_scan>=0){
        scan = single_scan;
        cmd << "Select PeaksMz,PeaksIntensity FROM Peaks WHERE Id='" << single_scan << "'"; //<=============
    } else
        cmd << "Select PeaksMz,PeaksIntensity FROM Peaks";
    ostringstream cmd1; cmd1 << "INSERT into Peaks(PeaksMz,PeaksIntensity) values (?,?)";
    if (sqlite3_prepare_v2(db_in, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                compressed_mz_length = sqlite3_column_bytes(stmt, 0);
                unsigned char* compressed_mz = (unsigned char*)sqlite3_column_blob(stmt, 0);

                double* restored_mz = 0;
                int mz_length = 0;
                auto start = chrono::system_clock::now();
                if (!decompressMzCentroidedType1(&compressed_mz, &restored_mz, &mz_length)){
                    std::cout << "  occured at decompress mz spectra for scan = " << scan << endl; 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                } 

                compressed_intens_length = sqlite3_column_bytes(stmt, 1);
                unsigned char* compressed_intens = (unsigned char*)sqlite3_column_blob(stmt, 1);

                float* restored_intensity = 0;
                int intens_length = 0;
                start = chrono::system_clock::now();
                if (!decompressIntensityCentroidedType1(&compressed_intens, &restored_intensity, &intens_length)){
                    std::cout << "  occured at decompress intensity spectra for scan = " << scan << endl; 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; free_safe(restored_mz); free_safe(restored_intensity); return false;
                } 
                auto duration = chrono::system_clock::now() - start;
                auto decompress_millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

                sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                if ((rc1=sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                    sqlite3_bind_blob(stmt1, 1, (unsigned char*)restored_mz, mz_length*8, SQLITE_STATIC); 
                    sqlite3_bind_blob(stmt1, 2, (unsigned char*)restored_intensity, intens_length*4, SQLITE_STATIC); 
                    rc1 = sqlite3_step(stmt1);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt1);
                free_safe(restored_mz); free_safe(restored_intensity); 
                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                int compressed_length = compressed_mz_length + compressed_intens_length;
                compressed_size += compressed_length; original_size += mz_length*12; // 8 bytes mz + 4 bytes intensity 
                if (verbose) std::cout << "   scan: " << scan << ", compress=" << compressed_length << ", decomp=" << mz_length*12 << " bytes (cr=" << setprecision(5) << (float)mz_length*12/compressed_length << setprecision(0) << ") [" << decompress_millis << "]" << endl;
                scan++; success = true; 
                //if (scan==3)
                //    break;

            } else if (rc == SQLITE_DONE) {
                success = true; break;
            } else {
                if (verbose) cout << "rc=" << rc << endl; break;
            } 
        } 
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db_out, "COMMIT;", NULL, NULL, NULL);
    sqlite3_exec(db_in, "COMMIT;", NULL, NULL, NULL);
    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; 
    if (verbose) std::cout << "done! overall compression=" << commas2(compressed_size).c_str() << " decomp=" << commas2(original_size).c_str() << " bytes (cr=" << setprecision(5) << (float)original_size/compressed_size << setprecision(0) << ")" << endl;
    return success;
}

// decompress type 4 file (ABSciex)
bool PicoLocalDecompress::decompressType4(const wstring &input_compressed_db_filename, const wstring &out_uncompressed_db_filename, int single_scan, bool restore_zero_peaks)
{
    wstring out_filename = out_uncompressed_db_filename;
    if (out_filename.length() == 0){
        out_filename = input_compressed_db_filename + L"_uncompressed.byspec2";
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
    if (!sql_out->createCopyByspecTablesToSqlite(input_compressed_db_filename.c_str(), false)){
        wcerr << "can't create table in sqlite database = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }

    Sqlite* sql_in = new Sqlite(input_compressed_db_filename);
    if(!sql_in->openDatabase()){
        wcerr << "can't open output sqlite database = " << input_compressed_db_filename << endl;
        delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 

    int scan_count = sql_in->getPeakBlobCount(); 
    if (scan_count<0){
        std::cout << "can't read peak blob count from sqlite database" << endl; 
        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if (verbose) std::cout << "\ndecompress:   spectrum contains " << scan_count << " scans" << endl;

    unsigned long long original_size = 0, compressed_size = 0;  
    int scan = 0, compressed_mz_length = 0, compressed_intens_length = 0;
    sqlite3* db_in = sql_in->getDatabase(), *db_out = sql_out->getDatabase();
    sqlite3_stmt *stmt; bool success = false;
    sqlite3_exec(db_in, "BEGIN;", NULL, NULL, NULL);
    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    ostringstream cmd; 
    if (single_scan>=0)
        cmd << "Select PeaksMz FROM Peaks WHERE Id='" << single_scan << "'"; //<=============
    else
        cmd << "Select PeaksMz FROM Peaks";
    ostringstream cmd1; cmd1 << "INSERT into Peaks(PeaksMz,PeaksIntensity) values (?,?)";
    if (sqlite3_prepare_v2(db_in, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                compressed_mz_length = sqlite3_column_bytes(stmt, 0);
                unsigned char* compressed_mz = (unsigned char*)sqlite3_column_blob(stmt, 0);

                double* restored_mz = 0;
                float* restored_intensity = 0;
                int restored_mz_length = 0;
                auto start = chrono::system_clock::now();
                if (!decompressRawType4(compressed_mz, compressed_mz_length, &restored_mz, &restored_intensity, &restored_mz_length, restore_zero_peaks)){
                    std::cout << "  occured at decompress mz spectra for scan = " << scan << endl; 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; free_safe(restored_mz); free_safe(restored_intensity); return false;
                } 
                auto duration = chrono::system_clock::now() - start;
                auto decompress_millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
                //PicoLocalCompress* pfc = new PicoLocalCompress();
                //pfc->writeMzsToFile(restored_mz, restored_mz_length, "c:\\data\\ABSciex\\scan1_out.csv"); delete pfc;

                sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                if ((rc1=sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                    sqlite3_bind_blob(stmt1, 1, (unsigned char*)restored_mz, restored_mz_length*8, SQLITE_STATIC); 
                    sqlite3_bind_blob(stmt1, 2, (unsigned char*)restored_intensity, restored_mz_length*4, SQLITE_STATIC); 
                    rc1 = sqlite3_step(stmt1);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt1);
                free_safe(restored_mz); free_safe(restored_intensity); 
                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                int compressed_length = compressed_mz_length + compressed_intens_length;
                compressed_size += compressed_length; original_size += restored_mz_length*12; // 8 bytes mz + 4 bytes intensity 
                if (verbose) std::cout << "   scan: " << scan << ", compress=" << compressed_length << ", decomp=" << restored_mz_length*12 << " bytes (cr=" << setprecision(5) << (float)restored_mz_length*12/compressed_length << setprecision(0) << ") [" << decompress_millis << "]" << endl;
                scan++; success = true; 
                //if (scan==3)
                //    break;

            } else if (rc == SQLITE_DONE) {
                success = true; break;
            } else {
                if (verbose) cout << "rc=" << rc << endl; break;
            } 
        } 
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db_out, "COMMIT;", NULL, NULL, NULL);
    sqlite3_exec(db_in, "COMMIT;", NULL, NULL, NULL);
    if (verbose) std::cout << "done! overall compression=" << commas2(compressed_size).c_str() << " decomp=" << commas2(original_size).c_str() << " bytes (cr=" << setprecision(5) << (float)original_size/compressed_size << setprecision(0) << ")" << endl;
    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; 
    if (!success)
        cout << "can't read mz blob from sqlite -- " << string(sqlite3_errmsg(db_in)) << endl; 
    return success;
}



// decompress type 0 spectra
bool PicoLocalDecompress::decompressSpectraType0(unsigned char** compressed, double** restored_mz, float** restored_intensity, int* length) const
{
    unsigned char* ptr = *compressed; 
    unsigned int restored_mz_size = *(unsigned int*)ptr; ptr+=4; *length = restored_mz_size; 
    double restored_b0 = *(double*)ptr; ptr+=8; 
    double restored_b1 = *(double*)ptr; ptr+=8; 
    double restored_b2 = *(double*)ptr; ptr+=8; 
    double restored_b3 = *(double*)ptr; ptr+=8; 
        
    // ------ restore hop table -------
    unsigned short mz_rlc_size = *(unsigned short*)ptr; ptr+=2; 
    map<unsigned int, unsigned int> hmap_rlc; 
    for (unsigned int ii=0; ii<mz_rlc_size; ii++){
        unsigned short val = *(unsigned short*)ptr; ptr+=2; 
        unsigned short val1 = *(unsigned short*)ptr; ptr+=2; 
        hmap_rlc.insert(std::pair<unsigned int, unsigned int>(val, val1));
    }

    // ------ restore intensity table -------
    unsigned char LSB_FACTOR1 = *(unsigned char*)ptr; ptr++;
    unsigned int LSB_FACTOR = LSB_FACTOR1;
    unsigned int intens_tbl_size = *(unsigned int*)ptr; ptr+=4; 
    map<unsigned int, unsigned int> hmap_intens; 
    for (unsigned int ii=0; ii<intens_tbl_size; ii++){
        unsigned int val = (unsigned int)*(unsigned short*)ptr; ptr+=2; 
        unsigned int val1 = (unsigned int)*(unsigned short*)ptr; ptr+=2; 
        if (val1>32767){
            val1 -= 32768;
        } else {
            unsigned char lsb = *(unsigned char*)ptr; ptr++; 
            val1 = val1<<8 | lsb;
        }
        hmap_intens.insert(std::pair<unsigned int, unsigned int>(val, val1*LSB_FACTOR));
    }

        
    // --------- decompress scan -----
    *restored_mz = (double *)malloc(restored_mz_size*sizeof(double)); 
    if (*restored_mz==NULL){
        std::cout << "null mz buf alloc" << endl; return false; 
    }
    for (unsigned int ii=0; ii<restored_mz_size; ii++){
        double val = restored_b0+restored_b1*ii+restored_b2*ii*ii+restored_b3*ii*ii*ii;
        (*restored_mz)[ii] = val;
    }
        
    unsigned int current = 0;
    unsigned int indices_size = *(unsigned int*)ptr; ptr+=4; 
    *restored_intensity = (float *)calloc(restored_mz_size, sizeof(float));
    if (*restored_intensity==NULL){
        std::cout << "null intensity buf alloc" << endl; return false; 
    }
    for (unsigned int ii=0; ii<indices_size; ii++){
        unsigned int bb = *(unsigned char*)ptr; ptr++; int val; //msb
        if (bb>127)
            val = bb - 128;
        else {
            unsigned int bb1 = *(unsigned char*)ptr; ptr++; //lsb
            val = bb<<8 | bb1;
        }                
        int del = hmap_rlc[val]; current += del; 
        if (del==0)
            ii+=0;
        if (del==139)
            ii+=0;
        bb = *(unsigned char*)ptr; ptr++; int val1;
        if (bb>127)
            val1 = bb - 128;
        else {
            unsigned int bb1 = *(unsigned char*)ptr; ptr++; 
            val1 = bb<<8 | bb1;
        }                
        int intens = hmap_intens[val1];
        if (current>restored_mz_size){
            std::cout << "current hop exceeds range" << endl; return false;
        }
        (*restored_intensity)[current] = (float)intens;
    }
    //writeFile(hop_del, "c://data//file_out_d.txt");     
    hmap_rlc.clear(); hmap_intens.clear();
    return true;
}

// decompress type 0 spectra
bool PicoLocalDecompress::decompressSpectraType02(unsigned char** compressed, double** restored_mz, float** restored_intensity, int* length)
{
    unsigned char* ptd = *compressed; 
    unsigned int restored_mz_size = *(unsigned int*)ptd; ptd+=4; *length = restored_mz_size; 
    double restored_b0 = *(double*)ptd; ptd+=8; 
    double restored_b1 = *(double*)ptd; ptd+=8; 
    double restored_b2 = *(double*)ptd; ptd+=8; 
    double restored_b3 = *(double*)ptd; ptd+=8; 
        
    // ------ restore dict table -------
    unsigned char LSB_FACTOR1 = *(unsigned char*)ptd; ptd++;
    unsigned int LSB_FACTOR = LSB_FACTOR1;
    unsigned int intens_tbl_size = *(unsigned int*)ptd; ptd += 4;
    std::vector<unsigned int> intes_vals;
    for (unsigned int ii=0; ii<intens_tbl_size; ii++){
        unsigned int val1 = (unsigned int)*(unsigned short*)ptd; ptd+=2; 
        if (val1>32767){
            val1 -= 32768;
        } else {
            unsigned char lsb = *(unsigned char*)ptd; ptd++; 
            val1 = val1<<8 | lsb;
        }
        intes_vals.push_back(val1);
    }

    // --------- decompress scan -----
    *restored_mz = (double *)malloc(restored_mz_size*sizeof(double)); 
    if (*restored_mz==NULL){
        std::cout << "null mz buf alloc" << endl; return false; 
    }
    for (unsigned int ii=0; ii<restored_mz_size; ii++){
        double val = restored_b0+restored_b1*ii+restored_b2*ii*ii+restored_b3*ii*ii*ii;
        (*restored_mz)[ii] = val;
    }
        
    *restored_intensity = (float *)calloc(restored_mz_size, sizeof(float));
    if (*restored_intensity==NULL){
        std::cout << "null intensity buf alloc" << endl; return false; 
    }
    float * intens_ptr = (float*)*restored_intensity;

    // ------------ decompress indices --------------
    //std::vector<unsigned int> base(restored_mz_size);
    bool hflag = false; unsigned char hold = 0;
    for (unsigned int ii=0; ii<intes_vals.size(); ii++){ //intens_recs.size(); ii++){                    
        unsigned int vali = intes_vals[ii], cur = 0, inc = 0;  //intens_recs[ii].value, cur = 0, inc = 0; 
        int cnt = 0;
        for ( ; ; ){
            if (ii==0x77 && cnt>=0x12)//3632)
                cnt+=0;
            if (!hflag){
                unsigned int bb = *ptd++; 
                if (bb>127){  // 0.5
                    bb -= 128;
                    hold = bb & 0xF; hflag = true; 
                    inc = (bb>>4); 
                    if (inc==7)
                        break;
                } else if (bb>63){  // 1
                    inc = (bb - 64) + 7;
                } else if (bb>31){  // 1.5
                    bb -= 32;
                    unsigned int bb1 = *ptd++; 
                    inc = ((bb<<4) | (bb1>>4)) + 71;
                    hold = bb1 & 0x0F; hflag = true;
                } else if (bb>15){  // 2
                    bb -= 16;
                    unsigned int bb1 = *ptd++; 
                    inc = ((bb<<8) | bb1) + 583;
                } else if (bb>7){  // 2.5
                    bb -= 8;
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    inc = (bb<<12) | (bb1<<4) | (bb2>>4);
                    inc += 4679;
                    hold = bb2 & 0x0F; hflag = true;
                } else { // 3
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    inc = (bb<<16) | (bb1<<8) | bb2;
                    inc += 37446;
                }
            } else {
                if (hold>7){  // 0.5
                    hold -= 8;
                    hflag = false;
                    if (hold==7)
                        break;
                    inc = hold; 
                } else if (hold>3){  // 1
                    hold -= 4;
                    unsigned int bb = *ptd++; 
                    inc = ((hold<<4) | (bb>>4)) + 7;
                    hold = bb & 0x0F; 
                } else if (hold>1){  // 1.5
                    hold -= 2;
                    hflag = false;
                    unsigned int bb = *ptd++; 
                    inc = ((hold<<8) | bb) + 71;
                } else if (hold==1){  // 2
                    unsigned int bb = *ptd++; 
                    unsigned int bb1 = *ptd++; 
                    inc = ((bb<<4) | (bb1>>4)) + 583;
                    hold = bb1 & 0x0F; 
                } else { 
                    unsigned int bb = *ptd++; 
                    if (bb>127){ // 2.5
                        bb -= 128;
                        hflag = false;
                        unsigned int bb1 = *ptd++; 
                        inc = ((bb<<8) | bb1) + 4679;
                    } else {
                        unsigned int bb1 = *ptd++; 
                        unsigned int bb2 = *ptd++; 
                        inc = ((bb<<12) | (bb1<<4) | (bb2>>4)) + 37446;
                        hold = bb2 & 0x0F;
                    }
                }
            }
            cur += inc+1;
            if ((int)cur>=restored_mz_size){
                cout << "cur index exceeded m/z range" << endl; return false;
            }
            //base[cur] = vali; 
            intens_ptr[cur] = (float)vali*LSB_FACTOR; //hmap_intens[idxi]*LSB_FACTOR; 
        /*    unsigned int posc = array[ii][cnt]; 
            if (cur != posc){
                cout << "cur=" << cur << ", pos=" << posc << endl;
            } */
            cnt++;
        }
        ii+=0;
    }

    // --------- restore intensity --------------
/*    std::vector<unsigned int> intens(restored_mz_size);
    for (unsigned int ii=0; ii<restored_mz_size; ii++){
        unsigned int idxi = base[ii];
        if (idxi>0){
            unsigned int vali = idxi*LSB_FACTOR; //hmap_intens[idxi]*LSB_FACTOR; 
            intens[ii] = vali;
        /*    if (vali!= (int)(intens_blob[ii])){
                cout << "dif at ii=" << ii << ", val=" << vali << ", orig=" << intens_blob[ii] << endl;
                ii+=0;
            } *//*
        }
    }*/
    return true;
}

// decompress type 1 spectra
bool PicoLocalDecompress::decompressSpectraType1(unsigned char** compressed_mz, unsigned char** compressed_intensity, const WatersCalibration::Coefficents *cal_mod_coef, int compress_info_id, unsigned int compressed_length, double** restored_mz, float** restored_intensity, int* restored_mz_length, bool restore_zero_peaks, const bool file_decompress)
{
    if (compress_info_id==WATERS_READER_INFO_ID){ // reader profile compression
        if (!decompressRawType1_N(*compressed_mz, cal_mod_coef, restored_mz, restored_intensity, restored_mz_length, restore_zero_peaks, file_decompress)){
            return false;
        }
        return true;

    } else if (compress_info_id==WATERS_READER_CENTROIDED_INFO_ID){ // reader centroid compression
        if (!decompressSpectraCentroidedType1(compressed_mz, compressed_intensity, restored_mz, restored_intensity, restored_mz_length)){
            return false;
        }
        return true;

    } else if (compress_info_id==WATERS_CENTROID_NO_COMPRESSION_ID){ // reader centroid, no_compression

        *restored_mz = (double *)*compressed_mz;
        *restored_intensity = (float *)*compressed_intensity;
        *restored_mz_length = compressed_length/8;
        return true;

    } else {
        cout << "unrecognized compress_info_id tag=" << compress_info_id << endl; return false;
    }
}

bool PicoLocalDecompress::decompressMzType1(unsigned char** compressed, double** restored_mz, int* length) const
{
    unsigned char* ptd = *compressed;
    unsigned short dictd_len = *(unsigned short*)ptd;
    unsigned short based_len = *(unsigned short*)(ptd+2);
    //unsigned int hfrd_len = *(unsigned int*)(ptd+4);
    //unsigned short lfrd_len = *(unsigned short*)(ptd+8);
    unsigned int mzd_len = *(unsigned int*)(ptd+10);
    *length = mzd_len; //mz_len = mzd_len;
    double mz0 = *(double*)(ptd+14);
    ptd += 22;

    *restored_mz = (double *)malloc(mzd_len*sizeof(double)); //<===============
    if (*restored_mz==NULL){
        std::cout << "mz buf allocation failed" << endl; return false; 
    }

    // ----- dict ---------
    map<unsigned short, unsigned short> hmpd; 
    for (unsigned short ii=0; ii<dictd_len/2; ii++){
        unsigned short vali = *(unsigned short*)ptd; ptd+=2;
        hmpd.insert(std::pair<unsigned short, unsigned short>(static_cast<unsigned short>(hmpd.size()), vali));
    }

    // -------- base --------
    std::vector<unsigned int> base1; 
    for (unsigned short ii=0; ii<based_len/3; ii++){
        unsigned char vali = *ptd++;  
        unsigned int rept = *ptd++; 
        rept = (rept<<8) | *ptd++; 
        for (unsigned int jj=0; jj<rept; jj++)
            base1.push_back(vali);
    }

    // -------- hfr --------
    for (int ii=0; ii<min(256,(int)dictd_len/2); ii++){
        if (ii==182)
            ii+=0;
        unsigned int cur7 = 0; 
        for ( ; ; ){
            unsigned int skip = *ptd++; 
            if (skip==255)
                break;
            if (skip>127)  
                skip -= 128;
            else {
                unsigned int skip1 = *ptd++; 
                skip = (skip<<8) | skip1;
                if (skip>16384)
                    skip -= 16384;
                else
                    skip = (skip<<8) | *ptd++;
            }
            cur7 += skip;
            if ((int)cur7>mzd_len-1){
                cout << "hfr index exceeded range" << endl;    return false; 
            }                        
            base1[cur7] = ii; 
        }
        //ii+=0;
    }

    // -------- lfr --------
    for (unsigned short ii=256; ii<dictd_len/2; ii++){ // series of loc x val 
        for ( ; ; ){
            unsigned int loc1 = *ptd++;
            bool last = false;
            if (loc1>127){
                loc1 = loc1-128; last = true;
            }
            unsigned int loc2 = *ptd++, loc3 = *ptd++;
            unsigned int loci = (loc1<<16) | (loc2<<8) | loc3; //*ptd++;
            if ((int)loci>mzd_len-1){
                cout << "lfr index exceeded range" << endl;    return false; 
            }
            base1[loci] = ii;
            if (last)
                break;
        }
        //ii+=0;
    }

/*    // --------- verify indices ----------
    for (unsigned int ii=0; ii<base1.size(); ii++){
        unsigned short idx = indices[ii], idx1 = base1[ii];
        if (idx!=idx1){
            ii+=0; 
        }
    }

    // --------- restore m/z and verify  ----------
    double * mzv = new double[mz_len]; mzv[0]=mz0; double d_pos = mz0;
        for (int ii=1; ii<mz_len; ii++){
/*            double err1 = abs(mzv[ii-1] - d_pos);
            double del0x = mz_blob[ii]-mz_blob[ii-1];
            unsigned short idx6 = hmp6[del0x];
            unsigned short vid8 = hmpd[idx6]; *//*
            unsigned short idx = base1[ii-1];
            unsigned short vid = hmpd[idx];
            double vali = (double)vid/32768.0;
            double step = d_pos + vali;
            mzv[ii] = step; 
            d_pos = step; 
            double err = abs(mzv[ii] - mz_blob[ii]);
            if (err>0) //1E-7)
                ii+=0;
        }
*/

    double d_pos = mz0;
    double * ptv = *restored_mz; *ptv++ = d_pos;
    for (unsigned int ii=1; ii<mzd_len; ii++){
        unsigned short idx = base1[ii-1];
        unsigned short vid = hmpd[idx];
        double vali = (double)vid/32768.0;
        double step = d_pos + vali;
        *ptv++ = step; 
        d_pos = step; 
    }
    return true;
}

bool PicoLocalDecompress::decompressIntensityType1(unsigned char** compressed, float** restored_intensity, int* length) const
{
    unsigned char* ptr = *compressed; 
    unsigned int restored_mz_size = *(unsigned int*)ptr; ptr+=4;
    *length = (int)restored_mz_size;
    
    map<unsigned int, unsigned int> hmap_intens; 
    bool hflag = false; unsigned char hold = 0;
    unsigned int idz = *(unsigned int*)ptr; ptr+=4;
    unsigned int idz1 = *(unsigned int*)ptr; ptr+=4;
    unsigned int idz2 = *(unsigned int*)ptr; ptr+=4;
    unsigned int total_cnt = *(unsigned int*)ptr; ptr+=4;

    for (unsigned int ii=0; ii<min(idz,total_cnt); ii++){
        if (!hflag){
            unsigned int val1 = *(unsigned char*)ptr; ptr++;
            unsigned int val = *(unsigned char*)ptr; ptr++;
            hold = (unsigned char)(val&0x0F); 
            val = (val1<<4) | (val>>4); hmap_intens.insert(std::pair<unsigned int, unsigned int>(ii, val)); hflag = true; 
        } else {
            unsigned int val1 = *(unsigned char*)ptr; ptr++;
            unsigned int val = val1 | (((unsigned int)hold)<<8);
            hmap_intens.insert(std::pair<unsigned int, unsigned int>(ii, val)); hflag = false; 
        }
    }

    hold = 0; hflag = false;
    for (unsigned int ii = 0; ii < min(idz1, total_cnt); ii++) {
        unsigned int val = *(unsigned int *)ptr;
        ptr += 4;
        hmap_intens.insert(std::pair<unsigned int, unsigned int>(
            static_cast<unsigned int>(hmap_intens.size()), val));
    }

    for (unsigned int ii=0; ii<min(idz2,total_cnt); ii++){
        if (!hflag){
            unsigned int val = *(unsigned int*)ptr; ptr+=4; val <<= 4;
            hold = *(unsigned char*)ptr; ptr++;
            val |= (hold>>4); hmap_intens.insert(std::pair<unsigned int, unsigned int>(static_cast<unsigned int>(hmap_intens.size()), val)); 
            hflag = true; 
        } else {
            unsigned int val = *(unsigned int*)ptr; ptr+=4; 
            unsigned int val1 = (hold & 0x0F); 
            val |= (val1<<16); hmap_intens.insert(std::pair<unsigned int, unsigned int>(static_cast<unsigned int>(hmap_intens.size()), val));
            hflag = false; 
        }
    }

    hold = 0; hflag = false;
    for (unsigned int ii=idz+idz1+idz2; ii<total_cnt; ii++){
        unsigned int val1 = *(unsigned short*)ptr; ptr+=2; 
        unsigned int val = *(unsigned int*)ptr; ptr+=4; 
        val |= (val1<<16); hmap_intens.insert(std::pair<unsigned int, unsigned int>(static_cast<unsigned int>(hmap_intens.size()), val));
    }

    // --------- decompress scan -----
/*    *restored_mz = (double *)malloc(restored_mz_size*sizeof(double)); 
    if (*restored_mz==NULL){
        std::cout << "null mz buf alloc" << endl; return false; 
    }*/
        
    unsigned int current = 0;
    unsigned int indices_size = *(unsigned int*)ptr; ptr+=4; 
    *restored_intensity = (float *)calloc(restored_mz_size, sizeof(float)); 
    if (*restored_intensity==NULL){
        std::cout << "null intensity buf alloc" << endl; return false; 
    }

    bool first = true; 
    hflag = false; hold = 0; unsigned int val; 
    for (unsigned int ii=0; ii<indices_size; ii++){
        if (!hflag) { // no hold
            unsigned int bb = *(unsigned char*)ptr; ptr++; 
            if (bb>127)
                val = bb - 128;
            else {
                unsigned int bb1 = *(unsigned char*)ptr; ptr++;                 
                if (bb>63){
                    val = (bb & 0x3F)<<4;
                    val |= (bb1>>4); hold = bb1 & 0x0F; 
                    val += 128; hflag = true;
                } else {
                    val = bb<<8 | bb1;
                    val += 1152; 
                }
            }

        } else { // hold

            unsigned int bb = *(unsigned char*)ptr; ptr++; //msb
            if (hold>7) {
                val = ((hold & 0x7)<<4) + (bb>>4);
                hold = bb & 0x0F;
            } else if (hold>3){
                val = (((unsigned int)hold & 0x3)<<8) + bb;
                val += 128; hflag = false;
            } else {
                unsigned int bb1 = *(unsigned char*)ptr; ptr++; // mid - sb
                val = ((unsigned int)hold<<12) + (bb<<4) + (bb1>>4);
                hold = bb1 & 0x0F;
                val += 1152; 
            }
        }
        
        unsigned int cword = hmap_intens[val]; 
        unsigned int del = cword & 0x03; unsigned int intens = cword >> 2; 
        if (ii>0)
            del++; 
        current += del; 

        if (current>=(unsigned int)restored_mz_size){
            if (first) {
                std::cout << "current hop exceeds range at ii=" << ii << endl; first = false; return false;
            }
        } else {
            (*restored_intensity)[current] = (float)intens; 
        }

    /*    hflag = false; hold = 0; unsigned int val; 
        for (unsigned int ii=0; ii<indices_size; ii++){
            if (!hflag) { // no hold
                unsigned int bb = *(unsigned char*)ptr; ptr++; //msb
                if (bb>127)
                    val = bb - 128;
                else if (bb>63){
                    val = (bb & 0x3F)<<4;
                    unsigned int bb1 = *(unsigned char*)ptr; ptr++; //lsb
                    val |= (bb1>>4); hold = bb1 & 0x0F; hflag = true;
                } else {
                    unsigned int bb1 = *(unsigned char*)ptr; ptr++; 
                    val = bb<<8 | bb1;
                }

            } else { // hold

                if (hold>7) {
                    unsigned int bb = *(unsigned char*)ptr; ptr++; //lsb
                    val = ((hold & 0x7)<<4) + (bb>>4);
                    hold = (bb & 0x0F)<<4;
                } else if (hold>3){
                    unsigned int bb = *(unsigned char*)ptr; ptr++; //lsb
                    val = (((unsigned int)hold & 0x3)<<8) + bb;
                    hflag = false;
                } else {
                    unsigned int bb = *(unsigned char*)ptr; ptr++; // mid - sb
                    unsigned int bb1 = *(unsigned char*)ptr; ptr++; // mid - sb
                    val = ((unsigned int)hold<<12) + (bb<<4) + (bb1>>4);
                    hold = (bb1 & 0x0F)<<4; 
                }
            } */
        }
        //writeFile(hop_del, "c://data//file_out_d.txt"); 
    
    hmap_intens.clear();
    return true;
}

// decompress centroided type 1 spectra
bool PicoLocalDecompress::decompressSpectraCentroidedType1(unsigned char** compressed_mz, unsigned char** compressed_intensity, double** restored_mz, float** restored_intensity, int* length) const
{
    auto start = chrono::system_clock::now();
    if (!decompressMzCentroidedType1(compressed_mz, restored_mz, length)){
        return false;
    }

    int intens_length = 0;
    if (!decompressIntensityCentroidedType1(compressed_intensity, restored_intensity, &intens_length)){
        return false;
    }
    auto duration = chrono::system_clock::now() - start;
    //auto decompress_millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
    return true;
}

// decompress mz centroided Type1 
bool PicoLocalDecompress::decompressMzCentroidedType1(unsigned char** compressed, double** restored_mz, int* length) const
{
    unsigned char* ptd = *compressed;
    unsigned int mz_len1 = *(unsigned int *)ptd; ptd+=4; 

    if ((mz_len1&0x80000000)>0){ // no compression
        mz_len1 &= 0x7FFFFFFF; *length = (int)mz_len1;
        *restored_mz = (double*)malloc((mz_len1+1)*sizeof(double)); 
        if (*restored_mz==NULL){
            if (verbose) std::cout << "compressed mz buf allocation failed" << endl; return false;
        }
        for (int ii=0; ii<(int)mz_len1; ii++){
            (*restored_mz)[ii] = *(double *)ptd; ptd+=8;
        }
        return true;
    }

    *length = (int)mz_len1;
    double aa0 = *(double *)ptd; ptd+=8;
    double bb0 = *(double *)ptd; ptd+=8;
    double cc0 = *(double *)ptd; ptd+=8;
    double dd0 = *(double *)ptd; ptd+=8;
    unsigned int k_min = *(unsigned int *)ptd; ptd+=4;

    *restored_mz = (double*)malloc((mz_len1+1)*sizeof(double)); 
    if (*restored_mz==NULL){
        if (verbose) std::cout << "compressed mz buf allocation failed" << endl; return false;
    }

    (*restored_mz)[0] = dd0;
    bool hflag = false; unsigned char hold = 0;
    unsigned int cur = 0, inc = 0; 
    int cnt = 0;
    for ( ; ; ){
        if (cnt==31)
            cnt+=0;
        if (cnt>=(int)mz_len1)
            break;
        unsigned int bb = *ptd++; 
        if (!hflag){
            if (bb>127){  // 1
                bb -= 128;
                inc = bb;
            } else if (bb>63){  // 1.5
                bb -= 64;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<4) | (bb1>>4)) + 128;
                hold = bb1 & 0x0F; hflag = true;
            } else if (bb>31){  // 2
                bb -= 32;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<8) | bb1) + 1152;
            } else if (bb>15){  // 2.5
                bb -= 16;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = (bb<<12) | (bb1<<4) | (bb2>>4);
                inc += 9344;
                hold = bb2 & 0x0F; hflag = true;
            } else if (bb>7){ // 3
                bb -= 8;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = (bb<<16) | (bb1<<8) | bb2;
                inc += 74880;
            } else if (bb>3){ // 3.5
                bb -= 4;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = (bb<<20) | (bb1<<12) | (bb2<<4) | (bb3>>4);
                inc += 599168;
                hold = bb3 & 0x0F; hflag = true;
            } else if (bb>1){ // 4
                bb -= 2;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = (bb<<24) | (bb1<<16) | (bb2<<8) | bb3;
                inc += 4793472;
            } else if (bb>0){ // 4.5
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                unsigned int bb4 = *ptd++; 
                inc = (bb1<<20) | (bb2<<12) | (bb3<<4) | (bb4>>4);
                inc += 38347904;
                hold = bb4 & 0x0F; hflag = true;
            } else { // 5
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                unsigned int bb4 = *ptd++; 
                inc = (bb1<<24) | (bb2<<16) | (bb3<<8) | bb4;
                inc += 306783360; 
            }
        } else {
            unsigned char hbb = (hold<<4) | (bb>>4);
            if (hbb>127){  // 1
                inc = hbb - 128;
                hold = bb & 0x0F; 
            } else if (hbb>63){  // 1.5
                hold -= 4;
                hflag = false;
                inc = ((hold<<8) | bb) + 128;
            } else if (hbb>31){  // 2
                hold -= 2;
                unsigned int bb1 = *ptd++; 
                inc = ((hold<<12) | (bb<<4) | (bb1>>4)) + 1152;
                hold = bb1 & 0x0F; 
            } else if (hbb>15){  // 2.5 
                hold--;
                hflag = false;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<8) | bb1) + 9344;
            } else if (hbb>7){  // 3
                bb -= 128;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = ((bb<<12) | (bb1<<4) | (bb2>>4)) + 74880;
                hold = bb2 & 0x0F;
            } else if (hbb>3){ // 3.5
                bb -= 64;
                hflag = false;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = ((bb<<16) | (bb1<<8) | bb2) + 599168;
            } else if (hbb>1){  // 4
                bb -= 32;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = ((bb<<20) | (bb1<<12) | (bb2<<4) | (bb3>>4)) + 4793472;
                hold = bb3 & 0x0F;
            } else if (hbb>0){ // 4.5
                hflag = false; bb -= 16;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = ((bb<<24) | (bb1<<16) | (bb2<<8) | bb3) + 38347904;
            } else {  // 5
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                unsigned int bb4 = *ptd++; 
                inc = ((bb1<<20) | (bb2<<12) | (bb3<<4) | (bb4>>4)) + 306783360;
                hold = bb4 & 0x0F;
            }
        }
        inc += k_min;
        cnt++;
        cur += inc;
        double k2 = (double)cur*cur;
        double vali = aa0*cur*k2+bb0*k2+cc0*cur+dd0;
        if (cnt > (int)mz_len1){
            if (verbose) std::cout << "cur index exceeded m/z range" << endl;
        } else
            (*restored_mz)[cnt] = vali;
    }
    //int compressed_mz_length = ptd - *compressed;
    return true;
}

// decompress intensity centroided Type1 
bool PicoLocalDecompress::decompressIntensityCentroidedType1(unsigned char** compressed, float** restored_intensity, int* length) const
{
    unsigned char* ptd = *compressed;
    unsigned int mz_len1 = *(unsigned int *)ptd; ptd+=4;  

    if ((mz_len1&0x80000000)>0){ // no compression
        mz_len1 &= 0x7FFFFFFF; *length = (int)mz_len1;
        *restored_intensity = (float*)malloc((mz_len1+1)*sizeof(float)); 
        if (*restored_intensity==NULL){
            if (verbose) std::cout << "restores mz buf allocation failed" << endl; return false;
        }
        for (int ii=0; ii<(int)mz_len1; ii++){ 
            (*restored_intensity)[ii] = *(float *)ptd; ptd+=4;
        }
        return true;
    }

    *length = (int)mz_len1;
    float s_factor = *(float *)ptd; ptd+=4;  
    unsigned int min_intens = *(unsigned int *)ptd; ptd+=4;  
    unsigned short hop_size = *(unsigned short *)ptd; ptd+=2;
    int scale_fact = *ptd++;

    *restored_intensity = (float*)malloc((mz_len1+1)*sizeof(float)); 
    if (*restored_intensity==NULL){
        if (verbose) std::cout << "compressed intensity buf allocation failed" << endl; return false;
    }

    // ---- dict ---------
    map<unsigned int, unsigned int> hm9; 
    bool hflag = false; unsigned char hold = 0;
    //unsigned int cur = 0;
    unsigned int inc = 0; int cnt = 0;
    for ( ; ; ){
        if (cnt>=(int)hop_size)
            break;
        unsigned int bb = *ptd++; 
        if (!hflag){
            if (bb>127){  // 1
                bb -= 128;
                inc = bb;
            } else if (bb>63){  // 1.5
                bb -= 64;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<4) | (bb1>>4)) + 128;
                hold = bb1 & 0x0F; hflag = true;
            } else if (bb>31){  // 2
                bb -= 32;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<8) | bb1) + 1152;
            } else if (bb>15){  // 2.5
                bb -= 16;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = (bb<<12) | (bb1<<4) | (bb2>>4);
                inc += 9344;
                hold = bb2 & 0x0F; hflag = true;
            } else if (bb>7){ // 3
                bb -= 8;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = (bb<<16) | (bb1<<8) | bb2;
                inc += 74880;
            } else if (bb>3){ // 3.5
                bb -= 4;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = (bb<<20) | (bb1<<12) | (bb2<<4) | (bb3>>4);
                inc += 599168;
                hold = bb3 & 0x0F; hflag = true;
            } else if (bb>1){ // 4
                bb -= 2;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = (bb<<24) | (bb1<<16) | (bb2<<8) | bb3;
                inc += 4793472;
            } else if (bb>0){ // 4.5
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                unsigned int bb4 = *ptd++; 
                inc = (bb1<<20) | (bb2<<12) | (bb3<<4) | (bb4>>4);
                inc += 38347904;
                hold = bb4 & 0x0F; hflag = true;
            } else { 
                unsigned int bb1 = *ptd++; 
                if (bb1>127){ // 5
                    bb1 -= 128;
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    inc = (bb1<<24) | (bb2<<16) | (bb3<<8) | bb4;
                    inc += 306783360; 
                } else { // 5.5
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    unsigned int bb5 = *ptd++; 
                    inc = (bb1<<28) | (bb2<<20) | (bb3<<12) | (bb4<<4) | (bb5>>4);
                    inc += 2454267008;
                    hold = bb5 & 0x0F; hflag = true;
                }
            }
        } else {
            unsigned char hbb = (hold<<4) | (bb>>4);
            if (hbb>127){  // 1
                inc = hbb - 128;
                hold = bb & 0x0F; 
            } else if (hbb>63){  // 1.5
                hold -= 4;
                hflag = false;
                inc = ((hold<<8) | bb) + 128;
            } else if (hbb>31){  // 2
                hold -= 2;
                unsigned int bb1 = *ptd++; 
                inc = ((hold<<12) | (bb<<4) | (bb1>>4)) + 1152;
                hold = bb1 & 0x0F; 
            } else if (hbb>15){  // 2.5 
                hold--;
                hflag = false;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<8) | bb1) + 9344;
            } else if (hbb>7){  // 3
                bb -= 128;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = ((bb<<12) | (bb1<<4) | (bb2>>4)) + 74880;
                hold = bb2 & 0x0F;
            } else if (hbb>3){ // 3.5
                bb -= 64;
                hflag = false;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = ((bb<<16) | (bb1<<8) | bb2) + 599168;
            } else if (hbb>1){  // 4
                bb -= 32;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = ((bb<<20) | (bb1<<12) | (bb2<<4) | (bb3>>4)) + 4793472;
                hold = bb3 & 0x0F;
            } else if (hbb>0){ // 4.5
                hflag = false; bb -= 16;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = ((bb<<24) | (bb1<<16) | (bb2<<8) | bb3) + 38347904;
            } else {  
                unsigned int bb1 = *ptd++; 
                if (bb>7){ // 5
                    bb = bb - 8;
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    inc = ((bb<<28) | (bb1<<20) | (bb2<<12) | (bb3<<4) | (bb4>>4)) + 306783360;
                    hold = bb4 & 0x0F;
                } else { // 5.5
                    hflag = false; 
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    inc = (bb1<<24) | (bb2<<16) | (bb3<<8) | bb4;
                    inc += 2454267008;
                }
            }
        }
        unsigned int zi = inc*scale_fact + min_intens; cnt++; 
        hm9.insert(std::pair<unsigned int, unsigned int>(static_cast<unsigned int>(hm9.size()), zi));
    }

    // ---- indices ---------
    cnt = 0; hflag = false;
    for ( ; ; ){
        if (cnt==108)
            cnt+=0;
        if (cnt>=(int)mz_len1)
            break;
        unsigned int bb = *ptd++; 
        if (!hflag){
            if (bb>127){  // 1
                bb -= 128;
                inc = bb;
            } else if (bb>63){  // 1.5
                bb -= 64;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<4) | (bb1>>4)) + 128;
                hold = bb1 & 0x0F; hflag = true;
            } else if (bb>31){  // 2
                bb -= 32;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<8) | bb1) + 1152;
            } else if (bb>15){  // 2.5
                bb -= 16;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = (bb<<12) | (bb1<<4) | (bb2>>4);
                inc += 9344;
                hold = bb2 & 0x0F; hflag = true;
            } else if (bb>7){ // 3
                bb -= 8;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = (bb<<16) | (bb1<<8) | bb2;
                inc += 74880;
            } else if (bb>3){ // 3.5
                bb -= 4;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = (bb<<20) | (bb1<<12) | (bb2<<4) | (bb3>>4);
                inc += 599168;
                hold = bb3 & 0x0F; hflag = true;
            } else if (bb>1){ // 4
                bb -= 2;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = (bb<<24) | (bb1<<16) | (bb2<<8) | bb3;
                inc += 4793472;
            } else if (bb>0){ // 4.5
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                unsigned int bb4 = *ptd++; 
                inc = (bb1<<20) | (bb2<<12) | (bb3<<4) | (bb4>>4);
                inc += 38347904;
                hold = bb4 & 0x0F; hflag = true;
            } else { 
                unsigned int bb1 = *ptd++; 
                if (bb1>127){ // 5
                    bb1 -= 128;
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    inc = (bb1<<24) | (bb2<<16) | (bb3<<8) | bb4;
                    inc += 306783360; 
                } else { // 5.5
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    unsigned int bb5 = *ptd++; 
                    inc = (bb2<<20) | (bb3<<12) | (bb4<<4) | (bb5>>4);
                    inc += 2454267008;
                    hold = bb5 & 0x0F; hflag = true;
                }
            }
        } else {
            unsigned char hbb = (hold<<4) | (bb>>4);
            if (hbb>127){  // 1
                inc = hbb - 128;
                hold = bb & 0x0F; 
            } else if (hbb>63){  // 1.5
                hold -= 4;
                hflag = false;
                inc = ((hold<<8) | bb) + 128;
            } else if (hbb>31){  // 2
                hold -= 2;
                unsigned int bb1 = *ptd++; 
                inc = ((hold<<12) | (bb<<4) | (bb1>>4)) + 1152;
                hold = bb1 & 0x0F; 
            } else if (hbb>15){  // 2.5 
                hold--;
                hflag = false;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<8) | bb1) + 9344;
            } else if (hbb>7){  // 3
                bb -= 128;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = ((bb<<12) | (bb1<<4) | (bb2>>4)) + 74880;
                hold = bb2 & 0x0F;
            } else if (hbb>3){ // 3.5
                bb -= 64;
                hflag = false;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = ((bb<<16) | (bb1<<8) | bb2) + 599168;
            } else if (hbb>1){  // 4
                bb -= 32;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = ((bb<<20) | (bb1<<12) | (bb2<<4) | (bb3>>4)) + 4793472;
                hold = bb3 & 0x0F;
            } else if (hbb>0){ // 4.5
                hflag = false; bb -= 16;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = ((bb<<24) | (bb1<<16) | (bb2<<8) | bb3) + 38347904;
            } else {  
                unsigned int bb1 = *ptd++; 
                if (bb>7){ // 5
                    bb = bb - 8;
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    inc = ((bb<<28) | (bb1<<20) | (bb2<<12) | (bb3<<4) | (bb4>>4)) + 306783360;
                    hold = bb4 & 0x0F; 
                } else { // 5.5
                    hflag = false; 
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    unsigned int bb5 = *ptd++; 
                    inc = (bb2<<24) | (bb3<<16) | (bb4<<8) | bb5;
                    inc += 2454267008;
                }
            }
        }
        unsigned int zi = hm9[inc];  
        if (cnt>=(int)mz_len1){
            if (verbose) std::cout << "intensity index exceeded range" << endl; 
        } else {
            float intens_i = (float)zi;
            if (s_factor!=1.0f)
                intens_i /= s_factor;
            (*restored_intensity)[cnt] = intens_i; //(float)zi; 
        }
        cnt++; 
    }
    //int compressed_intensity_length = ptd - *compressed;
    return true;
}

// decompress raw file type 4 (ABSciex)
bool PicoLocalDecompress::decompressRawType4(unsigned char* compressed, int /*compressed_length*/, double** restored_mz, float** restored_intensity, int* restored_mz_length, bool /*restore_zero_peaks*/) const
{
        // ------- decompress ---------
    double mz0d = *(double*)(compressed);
    //double mznd = *(double*)(compressed+8), mz_ranged = mznd-mz0d;
    double da0 = *(double*)(compressed+16);
    double da1 = *(double*)(compressed+24);
    double da2 = *(double*)(compressed+32);
    double da3 = *(double*)(compressed+40);
    unsigned int psized = *(unsigned int*)(compressed+48);
    unsigned short levelsd = *(unsigned short*)(compressed+52);
    unsigned int mzd_len = *(unsigned int*)(compressed+54);

    *restored_mz = (double*)calloc(((int)(mzd_len*1.2)), sizeof(double)); 
    if (*restored_mz==NULL){
        if (verbose) std::cout << "decompress mz buf allocation failed" << endl; return false;
    }

    *restored_intensity = (float*)calloc(((int)(mzd_len*1.2)), sizeof(float)); 
    if (*restored_intensity==NULL){
        if (verbose) std::cout << "decompress intensity buf allocation failed" << endl; return false;
    }

    // ------ decompress base level ------
    unsigned char *ptd = (unsigned char*)(compressed+58);
     
    bool hflag = false; unsigned char hold = 0;
    std::vector<unsigned int> indexd;  //based,
    //unsigned int cur = 0;
    unsigned int inc = 0, val0 = -1; int cnt = 0;
    for ( ; ; ){
        if (cnt==190)
            cnt+=0;
        unsigned int bb = *ptd++; 
        if (!hflag){
            if (bb>127){  // 1
                bb -= 128;
                inc = bb;
            } else if (bb>63){  // 1.5
                bb -= 64;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<4) | (bb1>>4)) + 128;
                hold = bb1 & 0x0F; hflag = true;
            } else if (bb>31){  // 2
                bb -= 32;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<8) | bb1) + 1152;
            } else if (bb>15){  // 2.5
                bb -= 16;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = (bb<<12) | (bb1<<4) | (bb2>>4);
                inc += 9344;
                hold = bb2 & 0x0F; hflag = true;
            } else { //if (bb>7){ // 3
                bb -= 8;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = (bb<<16) | (bb1<<8) | bb2;
                inc += 74880;
            }
        } else {
            unsigned char hbb = (hold<<4) | (bb>>4);
            if (hbb>127){  // 1
                inc = hbb - 128;
                hold = bb & 0x0F; 
            } else if (hbb>63){  // 1.5
                hold -= 4;
                hflag = false;
                inc = ((hold<<8) | bb) + 128;
            } else if (hbb>31){  // 2
                hold -= 2;
                unsigned int bb1 = *ptd++; 
                inc = ((hold<<12) | (bb<<4) | (bb1>>4)) + 1152;
                hold = bb1 & 0x0F; 
            } else if (hbb>15){  // 2.5 
                hold--;
                hflag = false;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<8) | bb1) + 9344;
            } else { //if (hbb>7){  // 3
                bb -= 128;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = ((bb<<12) | (bb1<<4) | (bb2>>4)) + 74880;
                hold = bb2 & 0x0F;
            }
        }
        if (inc==0)
            break;
        if (cnt==0){
            val0 = inc;
        } else {
            indexd.push_back(inc);
        }
        cnt++;
    }
    //int dictd_length = ptd - compressed - 58;

    std::vector<unsigned int> upkd_idx; 
    for (unsigned int ii=0; ii<indexd.size(); ii++){ 
        unsigned int idxi = indexd[ii];
        if ((idxi&1)==0){
            upkd_idx.push_back(idxi/2);
        } else {
            int cntj = idxi/2;
            for (int jj=0; jj<cntj; jj++)
                upkd_idx.push_back(1);
        }
    }

    std::vector<unsigned int> offset, value; offset.push_back(0); unsigned int prev = 0;
    for (unsigned int ii=0; ii<upkd_idx.size(); ii++){
        unsigned int cur = prev + upkd_idx[ii];
        offset.push_back(cur); value.push_back(val0);
        prev = cur;
    }
    value.push_back(val0);
    if (offset[offset.size()-1] != psized){
        cout << "offset size mismatch " << offset[offset.size()-1] << " differs from psize=" << psized << endl;
    }

    // ---------- generate other levels -------
    for (unsigned int levels=1; levels<levelsd; levels++){ 
        std::vector<unsigned int> idx_d; 
        //unsigned int cur = 0;
        unsigned int inc = 0, vald = -1; int cnt = 0;
        for ( ; ; ){
            if (cnt==190)
                cnt+=0;
            unsigned int bb = *ptd++; 
            if (!hflag){
                if (bb>127){  // 1
                    bb -= 128;
                    inc = bb;
                } else if (bb>63){  // 1.5
                    bb -= 64;
                    unsigned int bb1 = *ptd++; 
                    inc = ((bb<<4) | (bb1>>4)) + 128;
                    hold = bb1 & 0x0F; hflag = true;
                } else if (bb>31){  // 2
                    bb -= 32;
                    unsigned int bb1 = *ptd++; 
                    inc = ((bb<<8) | bb1) + 1152;
                } else if (bb>15){  // 2.5
                    bb -= 16;
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    inc = (bb<<12) | (bb1<<4) | (bb2>>4);
                    inc += 9344;
                    hold = bb2 & 0x0F; hflag = true;
                } else { //if (bb>7){ // 3
                    bb -= 8;
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    inc = (bb<<16) | (bb1<<8) | bb2;
                    inc += 74880;
                }
            } else {
                unsigned char hbb = (hold<<4) | (bb>>4);
                if (hbb>127){  // 1
                    inc = hbb - 128;
                    hold = bb & 0x0F; 
                } else if (hbb>63){  // 1.5
                    hold -= 4;
                    hflag = false;
                    inc = ((hold<<8) | bb) + 128;
                } else if (hbb>31){  // 2
                    hold -= 2;
                    unsigned int bb1 = *ptd++; 
                    inc = ((hold<<12) | (bb<<4) | (bb1>>4)) + 1152;
                    hold = bb1 & 0x0F; 
                } else if (hbb>15){  // 2.5 
                    hold--;
                    hflag = false;
                    unsigned int bb1 = *ptd++; 
                    inc = ((bb<<8) | bb1) + 9344;
                } else { //if (hbb>7){  // 3
                    bb -= 128;
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    inc = ((bb<<12) | (bb1<<4) | (bb2>>4)) + 74880;
                    hold = bb2 & 0x0F;
                }
            }
            if (cnt>1 && inc==0)
                break;
            if (cnt==0){
                if (inc==0) continue;
                vald = inc;
            } else {
                idx_d.push_back(inc);
            }
            cnt++;
        }

        unsigned int prev = 0;
        for (unsigned int ii=0; ii<idx_d.size(); ii++){
            unsigned int cur = prev + idx_d[ii];
            if (cur>value.size()){
                cout << "index exceeds buf range" << endl;
            }
            value[cur] = vald;
            prev = cur;
        }

        levels+=0;
    }

    // pack and restore mz
    prev = 0; int cnti = 0;
    std::vector<unsigned int> intens_r; std::vector<double> mzsd; 
    double pid = mz0d, zid = log10(mz0d); 
    for (unsigned int ii=0; ii<(unsigned int)offset.size(); ii++){
        unsigned int vali = value[ii], offseti = offset[ii]; //, cur = -1; int index = -1;
        if (ii>0){
            unsigned int delta = offseti - prev;
            if (delta==1){
                delta+=0;
            } else if (delta==2){
                double dzi = da0+da1*zid+da2*zid*zid+da3*zid*zid*zid; pid = pow(10.0, zid)+dzi;
                zid = log10(pid);    
                (*restored_mz)[cnti] = pid; (*restored_intensity)[cnti++] = 0; mzsd.push_back(pid); intens_r.push_back(0); 
            } else if (delta>2){
                double dzi = da0+da1*zid+da2*zid*zid+da3*zid*zid*zid; pid = pow(10.0, zid)+dzi;
                zid = log10(pid);    
                (*restored_mz)[cnti] = pid; (*restored_intensity)[cnti++] = 0; mzsd.push_back(pid); intens_r.push_back(0); 

                for (unsigned int jj=1; jj<delta-1; jj++){
                    dzi = da0+da1*zid+da2*zid*zid+da3*zid*zid*zid; pid = pow(10.0, zid)+dzi;
                    zid = log10(pid);    
                }
                (*restored_mz)[cnti] = pid; (*restored_intensity)[cnti++] = 0; mzsd.push_back(pid); intens_r.push_back(0); 
            }
            double dzi = da0+da1*zid+da2*zid*zid+da3*zid*zid*zid; pid = pow(10.0, zid)+dzi;
            zid = log10(pid);
        }
        (*restored_mz)[cnti] = pid; (*restored_intensity)[cnti++] = (float)vali; mzsd.push_back(pid); intens_r.push_back(vali); prev = offseti;
    }
    *restored_mz_length = cnti;

    if (cnti != mzd_len){
        cout << "restored len=" << cnti << " differs from original mz_len=" << mzd_len << endl;
    }
    return true;
}

//#define DEBUG_OUTPUT_OLD_DECOMPRESS

// decompress raw type 1 memory to memory (Waters)
bool PicoLocalDecompress::decompressRawType1_N(unsigned char* compressed, const WatersCalibration::Coefficents *cal_mod_coef, double** restored_mz, float** restored_intensity, int* restored_mz_length, bool restore_zero_peaks, const bool file_decompress) const
{
    const int steps_size = 12;
    const unsigned int steps[] = { 3520, 2496, 1768, 1248, 880, 624, 440, 312, 256, 0, 0, 0 };
    // ----- dict ---------
    std::vector<unsigned int> delta_d, pos_d, stp_d;
    unsigned char *ptd = compressed;
    unsigned int base_len_intens = *(unsigned int*)ptd; ptd+=4;

    bool calibration = false; std::vector<double> calib_coefi;
    if ((base_len_intens&0x40000000)>0){ // calibration
        calibration = true; base_len_intens &= 0xBFFFFFFF;
        for (int ii=0; ii<5; ii++){
            calib_coefi.push_back(*(double *)ptd); ptd +=8;
        }
        if (calib_coefi[1]>0){
            calib_coefi.push_back(*(double *)ptd); ptd += 8;
        }
    }

    if ((base_len_intens&0x80000000)>0){ // no compression
        base_len_intens &= 0x7FFFFFFF; *restored_mz_length = (int)base_len_intens;
        if (base_len_intens == 0)
            return true;

        if (!restore_zero_peaks){ // -- no need to restore zeros

            *restored_mz = (double*)malloc((size_t)(base_len_intens*sizeof(double)));
            if (*restored_mz == NULL){
                if (verbose) std::cout << "compressed mz buf allocation failed" << endl; return false;
            }
            *restored_intensity = (float*)malloc((size_t)(base_len_intens*sizeof(float)));
            if (*restored_intensity == NULL){
                if (verbose) std::cout << "compressed mz buf allocation failed" << endl; return false;
            }

            unsigned int prev_mz = 0;
            for (int ii = 0; ii < (int)base_len_intens; ii++){
                unsigned int mziu = prev_mz + *(unsigned int *)ptd; ptd += 4;
                prev_mz = mziu;
                double mzi = decodeAndCalibrateMzType1(mziu, calib_coefi, cal_mod_coef);
                (*restored_mz)[ii] = mzi;

                unsigned int intensi = *(unsigned int *)ptd; ptd += 4;
                (*restored_intensity)[ii] = (float)intensi;
            }
            return true;
        }

        double* data = (double*)calloc(4, sizeof(double));
        if (data == NULL){
            std::cout << "decomp data malloc failed" << std::endl; return false;
        }

        bool old_pred_style = false;
        double d0 = *(double *)ptd, d1 = *(double *)(ptd + 8), d2 = *(double *)(ptd + 16), d3 = *(double *)(ptd + 24);
        double dz_100 = d0 + d1 * 100 + d2 * 10000 + d3 * 1000000;
        if (abs(d0)>1e-30 && abs(d1)>1e-30 && abs(d2)>1e-30 && abs(d3)>1e-30 && abs(dz_100)>1e-6 && abs(dz_100)<100.0){
            if (base_len_intens > 0){
                for (int ii = 0; ii < 4; ii++){
                    data[ii] = *(double *)ptd; ptd += 8;
                }
            }
            else {
                data[1] = 1.0;
            }
        }
        else {
            data[1] = 1.0; old_pred_style = true;
        }
#ifdef DEBUG_OUTPUT_OLD_DECOMPRESS
        std::cout << "<DECOMPRESS_STYLE_FIRST>" << (old_pred_style ? "OLD" : "NEW") << "_STYLE_PRED</DECOMPRESS_STYLE_FIRST> dz0 = " << dz_100 << std::endl;
        std::cout << "d0=" << d0 << ",d1=" << d1 << ",d2=" << d2 << ",d3=" << d3 << std::endl;

        ofstream file;
        // append mode
        file.open("output_decompress_style.csv", std::ios_base::app);
        file << "1," << d0 << "," << d1 << "," << d2 << "," << d3 << "," << base_len_intens << "," << (old_pred_style ? "old" : "new") << std::endl;
        file.close();
#endif

        // restore zeros
        // -------- convert to double mz -----------
        std::vector<double> dbl_mz, delta_mz; std::vector<unsigned int> intense;
        unsigned int prev_mzh = *(unsigned int *)ptd; ptd += 4;
        double prev_mz = decodeAndCalibrateMzType1(prev_mzh, calib_coefi, cal_mod_coef); dbl_mz.push_back(prev_mz);
        unsigned int prev_intensi = *(unsigned int *)ptd; ptd += 4; intense.push_back(prev_intensi);
        for (int ii = 1; ii < (int)base_len_intens; ii++){
            unsigned int mziu = prev_mzh + *(unsigned int *)ptd; ptd += 4;
            prev_mzh = mziu;
            double mzi = decodeAndCalibrateMzType1(mziu, calib_coefi, cal_mod_coef); dbl_mz.push_back(mzi);
            delta_mz.push_back(mzi - prev_mz);
            prev_mz = mzi;
            unsigned int intensi = *(unsigned int *)ptd; ptd += 4; intense.push_back(intensi);
        }

        // restore zeros
        std::vector<double> restore_dbl_mzs; std::vector<unsigned int> restore_intens;
        if (!old_pred_style){
            double mz0 = dbl_mz[0], mz2 = mz0*mz0; unsigned int intensi = intense[0];
            double aa = data[0], bb = data[1], cc = data[2], dd = data[3];
            double prev_mzi = mz0, previous_delta_mzi = aa + bb*mz0 + cc*mz2 + dd*mz0*mz2;
            restore_intens.push_back(0); restore_dbl_mzs.push_back(mz0 - previous_delta_mzi);
            restore_intens.push_back(intensi); restore_dbl_mzs.push_back(mz0);
            for (unsigned int ii = 1; ii < dbl_mz.size(); ii++){
                double mzi = dbl_mz[ii], mzi2 = mzi*mzi; unsigned int intensi = intense[ii];
                double delta_mzi = aa + bb*mzi + cc*mzi2 + dd*mzi*mzi2;
                double delmz = mzi - prev_mzi;
                if (delmz > 2.5 * delta_mzi){
                    restore_intens.push_back(0); restore_dbl_mzs.push_back(prev_mzi + previous_delta_mzi);
                    restore_intens.push_back(0); restore_dbl_mzs.push_back(mzi - delta_mzi);
                } else if (delmz > 1.5 * delta_mzi){
                    restore_intens.push_back(0); restore_dbl_mzs.push_back((prev_mzi + mzi) / 2);
                }
                restore_intens.push_back(intensi); restore_dbl_mzs.push_back(mzi);
                prev_mzi = mzi; previous_delta_mzi = delta_mzi;
            }
            restore_intens.push_back(0); restore_dbl_mzs.push_back(prev_mzi + previous_delta_mzi);
            free_safe(data);
            dbl_mz.clear(); intense.clear();

            *restored_intensity = (float*)malloc(restore_intens.size()*sizeof(float));
            if (*restored_intensity == NULL){
                if (verbose) std::cout << "decomp intensity buf allocation failed" << endl; return false;
            }

            *restored_mz = (double*)malloc(restore_dbl_mzs.size()*sizeof(double));
            if (*restored_mz == NULL){
                cout << "decomp mz buf alloc failed" << endl; return false;
            }

            for (unsigned int ii = 0; ii < restore_dbl_mzs.size(); ii++)
                (*restored_mz)[ii] = restore_dbl_mzs[ii];
            for (unsigned int ii = 0; ii < restore_intens.size(); ii++)
                (*restored_intensity)[ii] = (float)restore_intens[ii];

            *restored_mz_length = static_cast<int>(restore_dbl_mzs.size());
            return true;

        } else {  // old style

            double* vect = (double *)calloc(11, sizeof(double));
            double* sx = &vect[0], *sy = &vect[7]; int pcnt = 0;
            std::vector<double> mzmn, mzmz; double dmin = 0.09;
            for (ptrdiff_t ii = delta_mz.size() - 1; ii >= 0; ii--){
                double vali = delta_mz[ii];
                if (vali <= dmin + 2E-9) {
                    dmin = vali; mzmn.push_back(vali); mzmz.push_back(dbl_mz[ii]);
                }
            }

            std::vector<double> dbl_mz_restore; std::vector<unsigned int> intens_restore;
            if (mzmn.size() < 2){ // all pts
                dmin = DEFAULT_ZERO_GAP; // arbitrary
                if (mzmn.size() > 0) dmin = mzmn[0];
                dbl_mz_restore.reserve(dbl_mz.size() * 3); intens_restore.reserve(dbl_mz.size() * 3);
                for (unsigned int ii = 0; ii < dbl_mz.size(); ii++){
                    double vali = dbl_mz[ii];
                    if (dbl_mz_restore.size() == 0){
                        dbl_mz_restore.push_back(vali - dmin); intens_restore.push_back(0);
                    }
                    else {
                        double dmz = vali - dbl_mz_restore[dbl_mz_restore.size() - 1];
                        if (dmz > dmin){
                            dbl_mz_restore.push_back(vali - dmin); intens_restore.push_back(0);
                        }
                    }
                    dbl_mz_restore.push_back(vali); intens_restore.push_back(intense[ii]);
                    if (ii == dbl_mz.size() - 1){
                        dbl_mz_restore.push_back(vali + dmin); intens_restore.push_back(0);
                    } else {
                        double dmz = dbl_mz[ii + 1] - vali;
                        if (dmz > dmin){
                            dbl_mz_restore.push_back(vali + dmin); intens_restore.push_back(0);
                        }
                    }
                }

            } else { // mzmn size >=2

                /*            int lmin = 1; dmin = mzmn[0];
                                for (; lmin < (int)mzmn.size(); lmin++){
                                double vali = mzmn[lmin];
                                if (abs(vali - dmin) < 1E-10)
                                break;
                                dmin = vali;
                                }
                                */
                double aa = 0, bb = 0, cc = 0, dd = 0;
                if (mzmn.size() == 2){
                    double slope = (mzmn[0] - mzmn[1]) / (mzmz[0] - mzmz[1] + 1E-12);
                    dd = mzmn[1] - mzmz[1] * slope; cc = slope;
                }
                else if (mzmn.size() == 3){
                    double sxx = 0, sxy = 0, sx = 0, sy = 0;
                    for (int ii = 0; ii < (int)mzmn.size(); ii++){
                        double xi = mzmz[ii], yi = mzmn[ii];
                        sx += xi; sxx += xi*xi; sy += yi; sxy += xi*yi;
                    }
                    cc = (sxy*mzmn.size() - sx*sy) / (sxx*mzmn.size() - sx*sx + 1E-12); dd = (sy - cc*sx) / mzmn.size();
                }
                else {
                    for (int ii = 0; ii < (int)mzmn.size(); ii++){
                        double deli = mzmn[ii], mzi = mzmz[ii];
                        double xpi = 1.0;
                        sy[0] += deli; pcnt++;
                        for (int jj = 1; jj < 7; jj++){
                            xpi *= mzi; sx[jj] += xpi;
                            if (jj < 4) sy[jj] += xpi*deli;
                        }
                    }
                    sx[0] = pcnt;
                    Predictor* pred = new Predictor(3);
                    pred->initialize(vect);
                    double* data1 = pred->predict();

                    aa = *(data1 + 3); bb = *(data1 + 2); cc = *(data1 + 1); dd = *data1;
                    delete pred;
                    free_safe(vect);
                }

                // -------- restore zeros -----------
                dbl_mz_restore.reserve(dbl_mz.size()); intens_restore.reserve(dbl_mz.size());
                double mzi = dbl_mz[0], mz2 = mzi*mzi; int zcnt1 = 0, zcnt2 = 0;
                double val0 = aa*mzi*mz2 + bb*mz2 + cc*mzi + dd; if (val0 <= 0.0) val0 = DEFAULT_ZERO_GAP; val0 = min(val0, MAX_ZERO_GAP);
                double prev_mzi = mzi, prev_val = val0; dbl_mz_restore.push_back(mzi - val0); intens_restore.push_back(0);
                dbl_mz_restore.push_back(mzi); intens_restore.push_back(intense[0]);

                for (unsigned int ii = 1; ii < dbl_mz.size(); ii++){
                    //mzi = dbl_mz[ii], mz2 = mzi*mzi, val0 = aa*mzi*mz2+bb*mz2+cc*mzi+dd; 
                    double mzp = dbl_mz[ii], mp2 = mzp*mzp; val0 = aa*mzp*mp2 + bb*mp2 + cc*mzp + dd;
                    if (val0 <= 0.0) val0 = DEFAULT_ZERO_GAP; val0 = min(val0, MAX_ZERO_GAP);
                    double gap = mzp - prev_mzi, sval = (prev_val + val0) / 2;
                    if (gap > sval*2.5){
                        dbl_mz_restore.push_back(prev_mzi + prev_val); intens_restore.push_back(0);
                        dbl_mz_restore.push_back(mzp - val0); intens_restore.push_back(0); zcnt1++;
                    }
                    else if (gap > sval*1.5){ //max(val0,valp)){
                        double midpt = (prev_mzi + mzp) / 2; zcnt2++;
                        dbl_mz_restore.push_back(midpt); intens_restore.push_back(0);
                    }
                    dbl_mz_restore.push_back(mzp); intens_restore.push_back(intense[ii]); prev_mzi = mzp; prev_val = val0;
                }

                mzi = dbl_mz[dbl_mz.size() - 1], mz2 = mzi*mzi; val0 = aa*mzi*mz2 + bb*mz2 + cc*mzi + dd;
                if (val0 <= 0.0) val0 = DEFAULT_ZERO_GAP; val0 = min(val0, MAX_ZERO_GAP);
                dbl_mz_restore.push_back(mzi + val0); intens_restore.push_back(0);
            }

            double prev_dbl_mz = dbl_mz_restore[0]; std::vector<double> dbl_mz_final; dbl_mz_final.push_back(prev_dbl_mz);
            unsigned int prev_intens = intens_restore[0]; std::vector<unsigned int> intens_final; intens_final.push_back(prev_intens);
            for (unsigned int ii = 1; ii < dbl_mz_restore.size(); ii++){
                double mzi = dbl_mz_restore[ii], mz_delta = mzi - prev_dbl_mz; unsigned int intensi = intens_restore[ii];
                if (intensi > 0 && prev_intens > 0 && mz_delta >= MAX_ZERO_GAP){
                    dbl_mz_final.push_back(prev_dbl_mz + DEFAULT_ZERO_GAP); intens_final.push_back(0);
                    dbl_mz_final.push_back(mzi - DEFAULT_ZERO_GAP); intens_final.push_back(0);
                }
                dbl_mz_final.push_back(mzi); intens_final.push_back(intensi);
                prev_dbl_mz = mzi; prev_intens = intensi;
            }

            *restored_mz = (double*)malloc((size_t)(dbl_mz_final.size()*sizeof(double)));
            if (*restored_mz == NULL){
                if (verbose) std::cout << "compressed mz buf allocation failed" << endl; return false;
            }
            *restored_intensity = (float*)malloc((size_t)(dbl_mz_final.size()*sizeof(float)));
            if (*restored_intensity == NULL){
                if (verbose) std::cout << "compressed mz buf allocation failed" << endl; return false;
            }

            for (unsigned int ii = 0; ii < dbl_mz_final.size(); ii++)
                (*restored_mz)[ii] = dbl_mz_final[ii];
            for (unsigned int ii = 0; ii < intens_final.size(); ii++){
                unsigned int restored_intensi = intens_final[ii];
                (*restored_intensity)[ii] = (float)restored_intensi;
            }
            *restored_mz_length = static_cast<int>(dbl_mz_final.size());
            return true;
        }
    }

    double* data = (double*)calloc(4, sizeof(double));
    if (data == NULL){
        std::cout << "decomp data malloc failed" << std::endl; return false;
    }

    // test valid predictor data for backward compatability -- D.K
    bool old_pred_style = false;
    double d0 = *(double *)ptd, d1 = *(double *)(ptd + 8), d2 = *(double *)(ptd + 16), d3 = *(double *)(ptd + 24);
    double dz_100 = d0 + d1 * 100 + d2 * 10000 + d3 * 1000000;
    if (abs(d0)>1e-30 && abs(d1)>1e-30 && abs(d2)>1e-30 && abs(d3)>1e-30 && abs(dz_100)>1e-6 && abs(dz_100)<100.0){
        for (int ii = 0; ii < 4; ii++){
            data[ii] = *(double *)ptd; ptd += 8;
        }
    }
    else {
        data[1] = 1.0; old_pred_style = true;
    }
#ifdef DEBUG_OUTPUT_OLD_DECOMPRESS
    std::cout << "<DECOMPRESS_STYLE_SECOND>" << (old_pred_style ? "OLD" : "NEW") << "_STYLE_PRED</DECOMPRESS_STYLE_SECOND> dz0 = " << dz_100 << std::endl;
    std::cout << "d0=" << d0 << ",d1=" << d1 << ",d2=" << d2 << ",d3=" << d3 << std::endl;

    ofstream file;
    // append mode
    file.open("output_decompress_style.csv", std::ios_base::app);
    file << "2," << d0 << "," << d1 << "," << d2 << "," << d3 << "," << base_len_intens << "," << (old_pred_style ? "old" : "new") << std::endl;
    file.close();
#endif
    
    bool ms_type_6 = (base_len_intens&0x20000000)>0; base_len_intens &= 0x0FFFFFFF; 
    std::vector<unsigned char> shift_levsd; std::vector<unsigned short> shift_idxsd; shift_idxsd.push_back(0);
    unsigned int mz0d = *(unsigned int*)ptd; ptd+=4;
    unsigned short base_val_intens = *(unsigned short*)ptd; ptd+=2;
    if (ms_type_6){
        double* data1 = NULL;
        unsigned char cnt = mz0d>>24; mz0d&=0x0FFFFFF;
        data1 = (double*)malloc(4*sizeof(double));
        if (data1==NULL){
            std::cout << "data curve buf allocation failed" << endl; return false;
        }
        for (int ii=0; ii<3; ii++){
            *(data1+ii) = *(double *)ptd; ptd+=8;
        }
        free_safe(data1);
        unsigned char pval = 0;
        for (int ii=0; ii<cnt; ii++){
            pval += *(unsigned char*)ptd++; shift_levsd.push_back(pval); 
        }
        unsigned int pidx = 0;
        for (int ii=1; ii<cnt; ii++){
            pidx += *(unsigned short*)ptd; ptd+=2;
            shift_idxsd.push_back(pidx);
        }

    } else {

        unsigned char stp_size = *ptd++; 
        for (int ii=0; ii<stp_size; ii++)
            stp_d.push_back(*ptd++);
        for (int ii=0; ii<stp_size; ii++){
            unsigned short val = *(unsigned short*)ptd; ptd+=2;
            delta_d.push_back(val); 
        }
        unsigned int val = *(unsigned short*)ptd; ptd+=2; 
        val |= ((unsigned int)(*ptd++))<<16; pos_d.push_back(val);
        for (int ii=0; ii<stp_size; ii++){
            unsigned int val1 = *(unsigned short*)ptd; ptd+=2;
            val1 |= ((unsigned int)(*ptd++))<<16; 
            val += val1; pos_d.push_back(val);
        }
        val = pos_d[pos_d.size()-1];
        pos_d.pop_back(); pos_d.push_back(val+1);

    }
    unsigned int uncompressed_len = *(unsigned int*)ptd; ptd+=4;
    *restored_mz_length = uncompressed_len;
    if (delta_d.size()==pos_d.size()-1)
        delta_d.push_back(delta_d[delta_d.size()-1]);

    // --------- decompress intensity ------------
    std::vector<unsigned int> base_intens(base_len_intens, base_val_intens); 
    bool hflag = false; unsigned char hold = 0; bool bdone = false, first_run = true;
    for ( ; ; ){
        unsigned int cur = 0, inc = 0, vali = 0; int cnt = 0; 
        for ( ; ; ){
            if (cnt==190)
                cnt+=0;
            unsigned int bb = *ptd++; 
            if (!hflag){
                if (bb>127){  // 1
                    bb -= 128;
                    inc = bb;
                } else if (bb>63){  // 1.5
                    bb -= 64;
                    unsigned int bb1 = *ptd++; 
                    inc = ((bb<<4) | (bb1>>4)) + 128;
                    hold = bb1 & 0x0F; hflag = true;
                } else if (bb>31){  // 2
                    bb -= 32;
                    unsigned int bb1 = *ptd++; 
                    inc = ((bb<<8) | bb1) + 1152;
                } else if (bb>15){  // 2.5
                    bb -= 16;
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    inc = (bb<<12) | (bb1<<4) | (bb2>>4);
                    inc += 9344;
                    hold = bb2 & 0x0F; hflag = true;
                } else if (bb>7){ // 3
                    bb -= 8;
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    inc = (bb<<16) | (bb1<<8) | bb2;
                    inc += 74880;
                } else if (bb>3){ // 3.5
                    bb -= 4;
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    inc = (bb<<20) | (bb1<<12) | (bb2<<4) | (bb3>>4);
                    inc += 599168;
                    hold = bb3 & 0x0F; hflag = true;
                } else if (bb>1){ // 4
                    bb -= 2;
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    inc = (bb<<24) | (bb1<<16) | (bb2<<8) | bb3;
                    inc += 4793472;
                } else if (bb>0){ // 4.5
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    inc = (bb1<<20) | (bb2<<12) | (bb3<<4) | (bb4>>4);
                    inc += 38347904;
                    hold = bb4 & 0x0F; hflag = true;
                } else { 
                    unsigned int bb1 = *ptd++; 
                    if (bb1>127){ // 5
                        bb1 -= 128;
                        unsigned int bb2 = *ptd++; 
                        unsigned int bb3 = *ptd++; 
                        unsigned int bb4 = *ptd++; 
                        inc = (bb1<<24) | (bb2<<16) | (bb3<<8) | bb4;
                        inc += 306783360; 
                    } else { // 5.5
                        unsigned int bb2 = *ptd++; 
                        unsigned int bb3 = *ptd++; 
                        unsigned int bb4 = *ptd++; 
                        unsigned int bb5 = *ptd++; 
                        inc = (bb1<<28) | (bb2<<20) | (bb3<<12) | (bb4<<4) | (bb5>>4);
                        inc += 2454267008;
                        hold = bb5 & 0x0F; hflag = true;
                    }
                }
            } else {
                unsigned char hbb = (hold<<4) | (bb>>4);
                if (hbb>127){  // 1
                    inc = hbb - 128;
                    hold = bb & 0x0F; 
                } else if (hbb>63){  // 1.5
                    hold -= 4;
                    hflag = false;
                    inc = ((hold<<8) | bb) + 128;
                } else if (hbb>31){  // 2
                    hold -= 2;
                    unsigned int bb1 = *ptd++; 
                    inc = ((hold<<12) | (bb<<4) | (bb1>>4)) + 1152;
                    hold = bb1 & 0x0F; 
                } else if (hbb>15){  // 2.5 
                    hold--;
                    hflag = false;
                    unsigned int bb1 = *ptd++; 
                    inc = ((bb<<8) | bb1) + 9344;
                } else if (hbb>7){  // 3
                    bb -= 128;
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    inc = ((bb<<12) | (bb1<<4) | (bb2>>4)) + 74880;
                    hold = bb2 & 0x0F;
                } else if (hbb>3){ // 3.5
                    bb -= 64;
                    hflag = false;
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    inc = ((bb<<16) | (bb1<<8) | bb2) + 599168;
                } else if (hbb>1){  // 4
                    bb -= 32;
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    inc = ((bb<<20) | (bb1<<12) | (bb2<<4) | (bb3>>4)) + 4793472;
                    hold = bb3 & 0x0F;
                } else if (hbb>0){ // 4.5
                    hflag = false; bb -= 16;
                    unsigned int bb1 = *ptd++; 
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    inc = ((bb<<24) | (bb1<<16) | (bb2<<8) | bb3) + 38347904;
                } else {  
                    unsigned int bb1 = *ptd++; 
                    if (bb>7){ // 5
                        bb = bb - 8;
                        unsigned int bb2 = *ptd++; 
                        unsigned int bb3 = *ptd++; 
                        unsigned int bb4 = *ptd++; 
                        inc = ((bb<<28) | (bb1<<20) | (bb2<<12) | (bb3<<4) | (bb4>>4)) + 306783360;
                        hold = bb4 & 0x0F;
                    } else { // 5.5
                        hflag = false; 
                        unsigned int bb2 = *ptd++; 
                        unsigned int bb3 = *ptd++; 
                        unsigned int bb4 = *ptd++; 
                        inc = (bb1<<24) | (bb2<<16) | (bb3<<8) | bb4;
                        inc += 2454267008;
                    }
                }
            }
            if (inc==0){
                if (cnt==0){
                    bdone = true; break;
                } else if (cnt>1){
                    break;
                }
            }
            if (cnt==0){
                vali = inc;
            } else {
                cur += inc;
                if (cur>=base_len_intens){
                    if (first_run){
                        cout << "index exceeded allowed range" << endl; first_run = false;
                    }
                } else {
                    base_intens[cur] = vali;
                }
            }
            cnt++;
        }
        if (bdone)
            break;
    }
    //int dictd_length = ptd - compressed;

    // -------- decompress mz ----------
        // ---- decompress run len -----
    std::vector<unsigned int> run_len_d;
    unsigned int cur = 0, inc = 0;//, vali = 0;
    int cnt = 0;
    for ( ; ; ){
        if (cnt==190)
            cnt+=0;
        unsigned int bb = *ptd++; 
        if (!hflag){
            if (bb>127){  // 1
                bb -= 128;
                inc = bb;
            } else if (bb>63){  // 1.5
                bb -= 64;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<4) | (bb1>>4)) + 128;
                hold = bb1 & 0x0F; hflag = true;
            } else if (bb>31){  // 2
                bb -= 32;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<8) | bb1) + 1152;
            } else if (bb>15){  // 2.5
                bb -= 16;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = (bb<<12) | (bb1<<4) | (bb2>>4);
                inc += 9344;
                hold = bb2 & 0x0F; hflag = true;
            } else if (bb>7){ // 3
                bb -= 8;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = (bb<<16) | (bb1<<8) | bb2;
                inc += 74880;
            } else if (bb>3){ // 3.5
                bb -= 4;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = (bb<<20) | (bb1<<12) | (bb2<<4) | (bb3>>4);
                inc += 599168;
                hold = bb3 & 0x0F; hflag = true;
            } else if (bb>1){ // 4
                bb -= 2;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = (bb<<24) | (bb1<<16) | (bb2<<8) | bb3;
                inc += 4793472;
            } else if (bb>0){ // 4.5
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                unsigned int bb4 = *ptd++; 
                inc = (bb1<<20) | (bb2<<12) | (bb3<<4) | (bb4>>4);
                inc += 38347904;
                hold = bb4 & 0x0F; hflag = true;
            } else { 
                unsigned int bb1 = *ptd++; 
                if (bb1>127){ // 5
                    bb1 -= 128;
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    inc = (bb1<<24) | (bb2<<16) | (bb3<<8) | bb4;
                    inc += 306783360; 
                } else { // 5.5
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    unsigned int bb5 = *ptd++; 
                    inc = (bb1<<28) | (bb2<<20) | (bb3<<12) | (bb4<<4) | (bb5>>4);
                    inc += 2454267008;
                    hold = bb5 & 0x0F; hflag = true;
                }
            }
        } else {
            unsigned char hbb = (hold<<4) | (bb>>4);
            if (hbb>127){  // 1
                inc = hbb - 128;
                hold = bb & 0x0F; 
            } else if (hbb>63){  // 1.5
                hold -= 4;
                hflag = false;
                inc = ((hold<<8) | bb) + 128;
            } else if (hbb>31){  // 2
                hold -= 2;
                unsigned int bb1 = *ptd++; 
                inc = ((hold<<12) | (bb<<4) | (bb1>>4)) + 1152;
                hold = bb1 & 0x0F; 
            } else if (hbb>15){  // 2.5 
                hold--;
                hflag = false;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<8) | bb1) + 9344;
            } else if (hbb>7){  // 3
                bb -= 128;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = ((bb<<12) | (bb1<<4) | (bb2>>4)) + 74880;
                hold = bb2 & 0x0F;
            } else if (hbb>3){ // 3.5
                bb -= 64;
                hflag = false;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = ((bb<<16) | (bb1<<8) | bb2) + 599168;
            } else if (hbb>1){  // 4
                bb -= 32;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = ((bb<<20) | (bb1<<12) | (bb2<<4) | (bb3>>4)) + 4793472;
                hold = bb3 & 0x0F;
            } else if (hbb>0){ // 4.5
                hflag = false; bb -= 16;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = ((bb<<24) | (bb1<<16) | (bb2<<8) | bb3) + 38347904;
            } else {  
                unsigned int bb1 = *ptd++; 
                if (bb>7){ // 5
                    bb = bb - 8;
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    inc = ((bb<<28) | (bb1<<20) | (bb2<<12) | (bb3<<4) | (bb4>>4)) + 306783360;
                    hold = bb4 & 0x0F;
                } else { // 5.5
                    hflag = false; 
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    inc = (bb1<<24) | (bb2<<16) | (bb3<<8) | bb4;
                    inc += 2454267008;
                }
            }
        }
        if (cnt>1 && inc==0)
            break;
        run_len_d.push_back(inc); cnt++; 
    }
    //int dictd_length6 = ptd - compressed;

    // -------- restore base0 -------
    cur = 0; int level = 0; 
    std::vector<unsigned int> base00; base00.reserve(base_len_intens);
    unsigned int prv_val = 0, prv_vali = 0, pos=0, del=0, filler=0;
    if (pos_d.size()==0){
        pos = base_len_intens; del=0;
    } else {
        pos=pos_d[0]; del=delta_d[0]; 
    }
    for (unsigned int ii=0; ii<run_len_d.size(); ii++){
        unsigned int vari = run_len_d[ii], codei = vari&0x03, cnti = vari>>2, vali = 0;
        if (codei==0)
            vali = ++prv_val;
        else if (codei==1)
            vali = --prv_val;
        else {
            if (ii < run_len_d.size() - 1) { //<======
                ii++;
            }
            vali = run_len_d[ii];
            prv_val = vali;
        }
        prv_vali = vali; 
        for (unsigned int jj=0; jj<cnti; jj++){
            cur++;
            if (cur>pos && level<7){
                if (level<(int)delta_d.size()){
                    pos=pos_d[++level]; del=delta_d[level]; 
                }
            }
            if (vali==0){
                base00.push_back(0);
            } else {
                filler = (vali+del)*8;
                base00.push_back(filler);
            }
        }
    }
    unsigned int base_fill_cnt = static_cast<unsigned int>(base00.size());
    for (unsigned int ii=base_fill_cnt; ii<base_len_intens; ii++){
        base00.push_back(base00[ii-1]+filler);
    }

    // ---- decompress brun len -----
    std::vector<unsigned int> brun_len_d;
    cur = 0; inc = 0; cnt = 0; 
    for ( ; ; ){
        //if (cnt==190)
        //    cnt+=0;
        unsigned int bb = *ptd++; 
        if (!hflag){
            if (bb>127){  // 1
                bb -= 128;
                inc = bb;
            } else if (bb>63){  // 1.5
                bb -= 64;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<4) | (bb1>>4)) + 128;
                hold = bb1 & 0x0F; hflag = true;
            } else if (bb>31){  // 2
                bb -= 32;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<8) | bb1) + 1152;
            } else if (bb>15){  // 2.5
                bb -= 16;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = (bb<<12) | (bb1<<4) | (bb2>>4);
                inc += 9344;
                hold = bb2 & 0x0F; hflag = true;
            } else if (bb>7){ // 3
                bb -= 8;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = (bb<<16) | (bb1<<8) | bb2;
                inc += 74880;
            } else if (bb>3){ // 3.5
                bb -= 4;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = (bb<<20) | (bb1<<12) | (bb2<<4) | (bb3>>4);
                inc += 599168;
                hold = bb3 & 0x0F; hflag = true;
            } else if (bb>1){ // 4
                bb -= 2;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = (bb<<24) | (bb1<<16) | (bb2<<8) | bb3;
                inc += 4793472;
            } else if (bb>0){ // 4.5
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                unsigned int bb4 = *ptd++; 
                inc = (bb1<<20) | (bb2<<12) | (bb3<<4) | (bb4>>4);
                inc += 38347904;
                hold = bb4 & 0x0F; hflag = true;
            } else { 
                unsigned int bb1 = *ptd++; 
                if (bb1>127){ // 5
                    bb1 -= 128;
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    inc = (bb1<<24) | (bb2<<16) | (bb3<<8) | bb4;
                    inc += 306783360; 
                } else { // 5.5
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    unsigned int bb5 = *ptd++; 
                    inc = (bb1<<28) | (bb2<<20) | (bb3<<12) | (bb4<<4) | (bb5>>4);
                    inc += 2454267008;
                    hold = bb5 & 0x0F; hflag = true;
                }
            }
        } else {
            unsigned char hbb = (hold<<4) | (bb>>4);
            if (hbb>127){  // 1
                inc = hbb - 128;
                hold = bb & 0x0F; 
            } else if (hbb>63){  // 1.5
                hold -= 4;
                hflag = false;
                inc = ((hold<<8) | bb) + 128;
            } else if (hbb>31){  // 2
                hold -= 2;
                unsigned int bb1 = *ptd++; 
                inc = ((hold<<12) | (bb<<4) | (bb1>>4)) + 1152;
                hold = bb1 & 0x0F; 
            } else if (hbb>15){  // 2.5 
                hold--;
                hflag = false;
                unsigned int bb1 = *ptd++; 
                inc = ((bb<<8) | bb1) + 9344;
            } else if (hbb>7){  // 3
                bb -= 128;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = ((bb<<12) | (bb1<<4) | (bb2>>4)) + 74880;
                hold = bb2 & 0x0F;
            } else if (hbb>3){ // 3.5
                bb -= 64;
                hflag = false;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                inc = ((bb<<16) | (bb1<<8) | bb2) + 599168;
            } else if (hbb>1){  // 4
                bb -= 32;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = ((bb<<20) | (bb1<<12) | (bb2<<4) | (bb3>>4)) + 4793472;
                hold = bb3 & 0x0F;
            } else if (hbb>0){ // 4.5
                hflag = false; bb -= 16;
                unsigned int bb1 = *ptd++; 
                unsigned int bb2 = *ptd++; 
                unsigned int bb3 = *ptd++; 
                inc = ((bb<<24) | (bb1<<16) | (bb2<<8) | bb3) + 38347904;
            } else {  
                unsigned int bb1 = *ptd++; 
                if (bb>7){ // 5
                    bb = bb - 8;
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    inc = ((bb<<28) | (bb1<<20) | (bb2<<12) | (bb3<<4) | (bb4>>4)) + 306783360;
                    hold = bb4 & 0x0F;
                } else { // 5.5
                    hflag = false; 
                    unsigned int bb2 = *ptd++; 
                    unsigned int bb3 = *ptd++; 
                    unsigned int bb4 = *ptd++; 
                    inc = (bb1<<24) | (bb2<<16) | (bb3<<8) | bb4;
                    inc += 2454267008;
                }
            }
        }
        if (cnt>1 && inc==0){
                break;
        }
        brun_len_d.push_back(inc); cnt++; 
    }
    //int dictd_length5 = ptd - compressed;

    // -------- restore bruns -------
    unsigned int stp=0;
    if (pos_d.size()==0){
        pos = base_len_intens+1; 
    } else {
        pos=pos_d[0]; stp=steps[stp_d[0]]; 
    }
    unsigned int pstp = stp, ppos = pos;
    int curi = -1; level = 0; first_run = true;
    bool skip = (brun_len_d[0] == 0), skip1 = false;
    for (unsigned int ii = 0; ii < brun_len_d.size(); ii++){        
        if (skip) {
            skip=false; skip1=true;
            continue;
        }

        unsigned int vari = brun_len_d[ii], cnti = vari & 0x03, vali = vari >> 2;
        if (cnti > 1)// && ii < brun_len_d.size() - 1)
            cnti = brun_len_d[++ii];
       // else
       //     if (cnti > 0) 
       //         break;
        curi += cnti + 1;
        
        if (curi >= (int)pos && level < steps_size-2){
            if (level < (int)stp_d.size()){//-1){
                pos = pos_d[++level];
                if (level < (int)stp_d.size()){
                    int stpi = stp_d[level];
                    if (stpi < steps_size){
                        stp = steps[stpi];
                    }
                }
            }
        }
        if (curi >= (int)base_len_intens){
            if (first_run){
                first_run = false;
                if (curi>(int)base_len_intens+1){
                    cout << "brun exceeded allowed index";
                } else {
                    cout << "--";
                }
            }
        } else {
            if (vali==0){
                curi-=3;
            } else {
                unsigned int step = stp;
                if (skip1 && ii==ppos){
                    skip1 = false; step = pstp;
                }
                //if (curi<base00.size())
                    base00[curi] = (vali - 1) * 8 + step;
            }
        }
    }

    if (file_decompress){
        if (ms_type_6){
            std::cout << "ms_type_6 not yet supported -- exiting" << endl; return false;

        } else {
            cur = mz0d;
            if (!restore_zero_peaks){ // -- no need to restore zeros
                *restored_mz = (double*)malloc((int)(base00.size() * 8 * sizeof(unsigned char)));
                if (*restored_mz == NULL){
                    if (verbose) std::cout << "decompress mz buf allocation failed" << endl; return false;
                }
                unsigned int *ptrb = (unsigned int *)*restored_mz;
                //std::vector<unsigned int> uncompressed_mz, uncompressed_intensity;
                unsigned int restored_intensi = base_intens[0]; *ptrb = restored_intensi; //uncompressed_intensity.push_back(restored_intensi);
                ptrb[1] = cur; //uncompressed_mz.push_back(cur);
                for (unsigned int ii = 1; ii < base00.size(); ii++){
                    int idx = 2 * ii;
                    unsigned int restored_intensi = base_intens[ii]; ptrb[idx] = restored_intensi; //uncompressed_intensity.push_back(restored_intensi);
                    unsigned int val = base00[ii]; cur += val; ptrb[idx + 1] = cur; //uncompressed_mz.push_back(cur);
                }

                *restored_mz_length = static_cast<int>(base00.size() * 8);
                return true;

            } else { // resoring zeros
                Reader rdr;
                std::vector<unsigned short> dv_tab; std::vector<unsigned int> mz_base; std::vector<double> mz_tab;
                rdr.buildLookupTables(dv_tab, mz_base, mz_tab);

                unsigned int *restored_nz_mz = (unsigned int*)malloc((int)(base00.size() * 2 * sizeof(unsigned int)));
                if (restored_nz_mz == NULL){
                    if (verbose) std::cout << "decompress nz_mz buf allocation failed" << endl; return false;
                }

                unsigned int *ptrb = restored_nz_mz;
                //std::vector<unsigned int> uncompressed_mz, uncompressed_intensity;
                unsigned int restored_intensi = base_intens[0]; *ptrb++ = restored_intensi; //uncompressed_intensity.push_back(restored_intensi);
                *ptrb++ = cur; //uncompressed_mz.push_back(cur);
                for (unsigned int ii = 1; ii < base00.size(); ii++){
                    int idx = 2 * ii;
                    unsigned int restored_intensi = base_intens[ii]; *ptrb++ = restored_intensi; //uncompressed_intensity.push_back(restored_intensi);
                    unsigned int val = base00[ii]; cur += val; *ptrb++ = cur; //uncompressed_mz.push_back(cur);
                }

                std::vector<unsigned int> restored_zeros_mz;
                rdr.restoreZeros(restored_nz_mz, static_cast<int>(base00.size()), dv_tab, mz_base, mz_tab, restored_zeros_mz);
                free_safe(restored_nz_mz);

                const size_t sizeB = restored_zeros_mz.size()*sizeof(unsigned int);
                *restored_mz = (double*)malloc(sizeB);
                if (*restored_mz == NULL){
                    cout << "ucomp buf alloc failed" << endl; return false;
                }

                unsigned int *ptri = (unsigned int *)*restored_mz;
                for (size_t ii = 0; ii < restored_zeros_mz.size(); ii++) {
                    ptri[ii] = restored_zeros_mz[ii];
                }
                *restored_mz_length = static_cast<int>(sizeB);
                return true;
            }
        }
    }

    // -------- no need to restore zeros: convert to double mz output -----------
    cur = mz0d; 
    if (!restore_zero_peaks){ // -- no need to restore zeros
        double mzv = decodeAndCalibrateMzType1(mz0d, calib_coefi, cal_mod_coef); (*restored_mz)[0] = mzv;
        for (unsigned int ii=1; ii<base00.size(); ii++){
            unsigned int val = base00[ii]; cur += val;
            mzv = decodeAndCalibrateMzType1(cur, calib_coefi, cal_mod_coef); (*restored_mz)[ii] = mzv;
        }
        for (unsigned int ii=0; ii<base00.size(); ii++){
            unsigned int restored_intensi = base_intens[ii]; // decodeIntensityType1(base_intens[ii]);
            (*restored_intensity)[ii] = (float)restored_intensi;
        }

        *restored_mz_length = static_cast<int>(base00.size());
        return true;
    }
    // restore zeros
    // -------- convert to double mz -----------
    std::vector<double> dbl_mz, delta_mz; cur=mz0d; //std::vector<unsigned int> intense; //base_intens[]
    double prev_mz = decodeAndCalibrateMzType1(mz0d, calib_coefi, cal_mod_coef); dbl_mz.push_back(prev_mz);
    unsigned int lenx = static_cast<unsigned int>(base00.size()); if (base00[lenx-1]==0) lenx--;
    for (unsigned int ii=1; ii<lenx; ii++){ //base00.size()
        unsigned int val = base00[ii]; cur += val;
        //if (ii>108974)
        //    ii+=0;
        double mzv = decodeAndCalibrateMzType1(cur, calib_coefi, cal_mod_coef); dbl_mz.push_back(mzv);
        double del = mzv-prev_mz;
        if (del<0)
            ii+=0;
        delta_mz.push_back(del);
        prev_mz = mzv;
    }

    // restore zeros
    std::vector<double> restore_dbl_mzs; std::vector<unsigned int> restore_intens;
    if (!old_pred_style){
        double mz0 = dbl_mz[0], mz2 = mz0*mz0; unsigned int intensi = base_intens[0];
        double aa = data[0], bb = data[1], cc = data[2], dd = data[3];
        double prev_mzi = mz0, previous_delta_mzi = aa + bb*mz0 + cc*mz2 + dd*mz0*mz2;
        restore_intens.push_back(0); restore_dbl_mzs.push_back(mz0 - previous_delta_mzi);
        restore_intens.push_back(intensi); restore_dbl_mzs.push_back(mz0);
        for (unsigned int ii = 1; ii < dbl_mz.size(); ii++){
            double mzi = dbl_mz[ii], mzi2 = mzi*mzi; unsigned int intensi = base_intens[ii];
            double delta_mzi = aa + bb*mzi + cc*mzi2 + dd*mzi*mzi2;
            double delmz = mzi - prev_mzi;
            if (delmz > 2.5 * delta_mzi){
                restore_intens.push_back(0); restore_dbl_mzs.push_back(prev_mzi + previous_delta_mzi);
                restore_intens.push_back(0); restore_dbl_mzs.push_back(mzi - delta_mzi);
            }
            else if (delmz > 1.5 * delta_mzi){
                restore_intens.push_back(0); restore_dbl_mzs.push_back((prev_mzi + mzi) / 2);
            }
            restore_intens.push_back(intensi); restore_dbl_mzs.push_back(mzi);
            prev_mzi = mzi; previous_delta_mzi = delta_mzi;
        }
        restore_intens.push_back(0); restore_dbl_mzs.push_back(prev_mzi + previous_delta_mzi);
        free(data);
        dbl_mz.clear(); base_intens.clear();

    } else {
        // find 2-level minimum
        int idx = -1; double min0=99999.0, min1=min0;
        const double mz_bounds[] = {32., 64., 128., 256., 512., 1024., 2048., 4096., 8192., 16384.};
        std::vector<double> mz_mins; mz_mins.assign(20, 99999.0);    
        for (int ii=0; ii<(int)delta_mz.size(); ii++){
            double mzi = dbl_mz[ii+1];
            if (ii==0){
                while (mzi>=mz_bounds[idx])
                    idx++;
            }
            if (mzi>=mz_bounds[idx]){
                if (idx<9){
                    if (idx>=0){            
                        mz_mins[2*idx]=min0; mz_mins[2*idx+1]=min1; 
                    }
                    while (mzi>=mz_bounds[idx])
                        idx++;
                }
                min0=99999.0, min1=min0;
            }

            double deli = delta_mz[ii];    
            if (deli<0)
                ii+=0;
            if (deli<min0){
                min1=min0; min0=deli;
            } else {
                if (deli<min1 && deli>min0)
                    min1=deli;
            }
        }
        if (idx<0) idx=0;
        if (2*idx+1>(int)mz_mins.size()){
            cout << "exceed mz_mins size"; 
        }
        if (idx>9)
            idx=9;
        mz_mins[2*idx]=min0; mz_mins[2*idx+1]=min1;

        idx = 0;

        double* vect = (double *)calloc(11, sizeof(double));
        double* sx = &vect[0], *sy = &vect[7]; int pcnt = 0; 
        for (int ii=0; ii<(int)delta_mz.size(); ii++){
            double mzi = dbl_mz[ii+1];
            if (mzi>=mz_bounds[idx]){
                while (mzi>=mz_bounds[idx])
                    idx++;
                int idx2 = idx+1;
                if (2*idx2+1<(int)mz_mins.size()){
                    min0=mz_mins[2*idx2], min1=mz_mins[2*idx2+1];
                }
            }

            double deli = delta_mz[ii];
            if (deli<=min1){    
                double xpi = 1.0;
                sy[0] += deli; pcnt++;
                for (int jj=1; jj<7; jj++){
                    xpi *= mzi; sx[jj] += xpi; 
                    if (jj<4) sy[jj] += xpi*deli;
                }
            }
        }
        std::vector<double> mzmn, mzmz; double dmin1 = 9E99;
        for (ptrdiff_t ii = delta_mz.size() - 1; ii >= 0; ii--){
            double vali = delta_mz[ii];
            if (vali <= dmin1 + 2E-9) {
                dmin1 = vali; mzmn.push_back(vali); mzmz.push_back(dbl_mz[ii]);
            }
        }
        int lmin = 1; dmin1 = mzmn[0];
        for (; lmin < (int)mzmn.size(); lmin++){
            double vali = mzmn[lmin];
            if (abs(vali-dmin1)<1E-10)
                break;
            dmin1 = vali;
        }

        for (int ii = (int)mzmn.size() - 1; ii >= lmin; ii--){
            double deli = mzmn[ii], mzi = mzmz[ii];
            double xpi = 1.0;
            sy[0] += deli; pcnt++;
            for (int jj = 1; jj<7; jj++){
                xpi *= mzi; sx[jj] += xpi;
                if (jj<4) sy[jj] += xpi*deli;
            }
        }
        sx[0] = pcnt;
        Predictor* pred = new Predictor(3);
        pred->initialize(vect);
        double* data = pred->predict();

        double aa=*(data+3), bb=*(data+2), cc=*(data+1), dd=*data;
        delete pred; 
        free(vect); 

    
        // -------- restore zeros -----------
        restore_dbl_mzs.reserve(dbl_mz.size()); restore_intens.reserve(dbl_mz.size());
        double mzi = dbl_mz[0], mz2 = mzi*mzi; int zcnt1=0, zcnt2=0;
        double val0 = aa*mzi*mz2 + bb*mz2 + cc*mzi + dd; 
        if (val0 <= 0.0) val0 = DEFAULT_ZERO_GAP; val0 = min(val0, MAX_ZERO_GAP);
        double prev_mzi = mzi, prev_val = val0; restore_dbl_mzs.push_back(mzi - val0); restore_intens.push_back(0);
        restore_dbl_mzs.push_back(mzi); restore_intens.push_back(base_intens[0]);
    
        for (unsigned int ii=1; ii<dbl_mz.size(); ii++){ 
            double mzp = dbl_mz[ii], mp2 = mzp*mzp; val0 = aa*mzp*mp2 + bb*mp2 + cc*mzp + dd; 
            if (val0 <= 0.0) val0 = DEFAULT_ZERO_GAP; val0 = min(val0, MAX_ZERO_GAP);
            double gap = mzp-prev_mzi, sval=(prev_val+val0)/2;
            if (gap>sval*2.5){
                restore_dbl_mzs.push_back(prev_mzi + prev_val); restore_intens.push_back(0);
                restore_dbl_mzs.push_back(mzp - val0); restore_intens.push_back(0); zcnt1++;
            } else if (gap>sval*1.5){
                double midpt = (prev_mzi+mzp)/2; zcnt2++;
                restore_dbl_mzs.push_back(midpt); restore_intens.push_back(0);
            }
            restore_dbl_mzs.push_back(mzp); restore_intens.push_back(base_intens[ii]); prev_mzi = mzp; prev_val = val0;
        }

    }

    *restored_intensity = (float*)malloc(restore_intens.size()*sizeof(float));
    if (*restored_intensity == NULL){
        if (verbose) std::cout << "decomp intensity buf allocation failed" << endl; return false;
    }

    *restored_mz = (double*)malloc(restore_dbl_mzs.size()*sizeof(double));
    if (*restored_mz == NULL){
        cout << "decomp mz buf alloc failed" << endl; return false;
    }

    for (unsigned int ii = 0; ii<restore_dbl_mzs.size(); ii++)
        (*restored_mz)[ii] = restore_dbl_mzs[ii];
    for (unsigned int ii = 0; ii<restore_intens.size(); ii++)
        (*restored_intensity)[ii] = (float)restore_intens[ii];

    *restored_mz_length = static_cast<int>(restore_dbl_mzs.size());
    return true;

/*
    mzi = dbl_mz[dbl_mz.size()-1], mz2 = mzi*mzi; val0 = aa*mzi*mz2+bb*mz2+cc*mzi+dd; 
    if (val0 <= 0.0) val0 = DEFAULT_ZERO_GAP; val0 = min(val0, MAX_ZERO_GAP);
    dbl_mz_restore.push_back(mzi+val0); intens_restore.push_back(0);

    double prev_dbl_mz = dbl_mz_restore[0]; std::vector<double> dbl_mz_final; dbl_mz_final.push_back(prev_dbl_mz);
    unsigned int prev_intens = intens_restore[0]; std::vector<unsigned int> intens_final; intens_final.push_back(prev_intens);
    for (unsigned int ii = 1; ii < dbl_mz_restore.size(); ii++){
        double mzi = dbl_mz_restore[ii], mz_delta = mzi - prev_dbl_mz; unsigned int intensi = intens_restore[ii];
        if (intensi>0 && prev_intens > 0 && mz_delta >= MAX_ZERO_GAP){
            dbl_mz_final.push_back(prev_dbl_mz + DEFAULT_ZERO_GAP); intens_final.push_back(0);
            dbl_mz_final.push_back(mzi - DEFAULT_ZERO_GAP); intens_final.push_back(0);
        }
        dbl_mz_final.push_back(mzi); intens_final.push_back(intensi);
        prev_dbl_mz = mzi; prev_intens = intensi;
    }

    *restored_mz = (double*)malloc((int)(dbl_mz_final.size()*sizeof(double)));
    if (*restored_mz == NULL){
        if (verbose) std::cout << "decompress mz buf allocation failed" << endl; return false;
    }

    *restored_intensity = (float*)malloc((int)(dbl_mz_final.size()*sizeof(float)));
    if (*restored_intensity == NULL){
        if (verbose) std::cout << "decompress intensity buf allocation failed" << endl; return false;
    }

    for (unsigned int ii = 0; ii<dbl_mz_final.size(); ii++)
        (*restored_mz)[ii] = dbl_mz_final[ii];
    for (unsigned int ii = 0; ii<intens_final.size(); ii++){
        unsigned int restored_intensi = intens_final[ii];
        (*restored_intensity)[ii] = (float)restored_intensi;
    }

    *restored_mz_length = dbl_mz_final.size();
    return true;*/
}

/*
// decode and calibrate mz
double PicoLocalDecompress::decodeAndCalibrateMzType1_old(const unsigned int mz, const std::vector<double> &calib_coefi) const
{
    if (mz==0)
        return 0.0;
    double val; 
    if (mz<0x34000000UL) {
        val = 32.0;  // clip lower limit at 32.0
    } else if (mz<0x3C000000UL){
        val = mz/2097152.0 - 384.0;//r64;  32-64 for 0x34-0x38
    } else if (mz<0x44000000UL){
        val = mz/1048576.0 - 896.0;//r128;  64-128 for 0x3C-0x40
    } else if (mz<0x4C000000UL){
        val = mz/524288.0 - 2048.0;//r256;  128-256 for 0x44-0x48
    } else if (mz<0x54000000UL){
        val = mz/262144.0 - 4608.0;//r512;  256-512 for 0x4C-0x50
    } else if (mz<0x5C000000UL){
        val = mz/131072.0 - 10240.0;//r1024;  512-1024 for 0x54-0x58
    } else if (mz<0x64000000UL){
        val = mz/65536.0 - 22528.0;//r2048;  1024-2048 for 0x5C-0x60
    } else if (mz<0x6C000000UL){
        val = mz/32768.0 - 49152.0;//r4096;  2048-4096 for 0x64-0x68
    } else if (mz<0x74000000UL){
        val = mz/16384.0 - 106496.0;//r8192;  4096-8192
    } else {
        val = 8192.0;  // clip upper limit at 8192.0.0
    }
    if (calib_coefi.size() <= 0)
        return val;

    double oval = val;
    if (calib_coefi[1] < 0){ // for SQD
        double val2 = val*val; 
        oval = calib_coefi[0] - calib_coefi[1] * val + calib_coefi[2] * val2 + calib_coefi[3] * val2*val + calib_coefi[4] * val2*val2;// +calib_coefi[5] * val2*val2*val;
    } else {
        double vsq = sqrt(val), val2 = val*val;
        oval = calib_coefi[0]+calib_coefi[1]*vsq+calib_coefi[2]*val+calib_coefi[3]*vsq*val+calib_coefi[4]*val2+calib_coefi[5]*val2*vsq;
        oval *= oval;
    }
    return oval;
}
*/

// decode and calibrate mz
double PicoLocalDecompress::decodeAndCalibrateMzType1(const unsigned int mz, const std::vector<double> &calib_coefi, const WatersCalibration::Coefficents *cal_mod_coef) const
{
    if (mz==0)
        return 0.0;
    double val; 
    if (mz<0x34000000UL) {
        val = 32.0;  // clip lower limit at 32.0
    } else if (mz<0x3C000000UL){
        val = mz/2097152.0 - 384.0;//r64;  32-64 for 0x34-0x38
    } else if (mz<0x44000000UL){
        val = mz/1048576.0 - 896.0;//r128;  64-128 for 0x3C-0x40
    } else if (mz<0x4C000000UL){
        val = mz/524288.0 - 2048.0;//r256;  128-256 for 0x44-0x48
    } else if (mz<0x54000000UL){
        val = mz/262144.0 - 4608.0;//r512;  256-512 for 0x4C-0x50
    } else if (mz<0x5C000000UL){
        val = mz/131072.0 - 10240.0;//r1024;  512-1024 for 0x54-0x58
    } else if (mz<0x64000000UL){
        val = mz/65536.0 - 22528.0;//r2048;  1024-2048 for 0x5C-0x60
    } else if (mz<0x6C000000UL){
        val = mz/32768.0 - 49152.0;//r4096;  2048-4096 for 0x64-0x68
    } else if (mz<0x74000000UL){
        val = mz/16384.0 - 106496.0;//r8192;  4096-8192
    } else if (mz<0x7C000000UL){
        val = mz/8192.0 - 229375.0;//193;  8193-16384
    } else if (mz<0x84000000UL){
        val = mz/4096.0 - 491520.0;//193;  16385-32768
    } else {
        val = 32768.0;  // clip upper limit at 32768
    }
    if (calib_coefi.size() <= 0)
        return val;

    double oval = val;
    if (calib_coefi[1] < 0){ // for SQD
        double val2 = val*val; 
        oval = calib_coefi[0] - calib_coefi[1] * val + calib_coefi[2] * val2 + calib_coefi[3] * val2*val + calib_coefi[4] * val2*val2;// +calib_coefi[5] * val2*val2*val;
    } else {
        double vsq = sqrt(val), val2 = val*val;
        oval = calib_coefi[0]+calib_coefi[1]*vsq+calib_coefi[2]*val+calib_coefi[3]*vsq*val+calib_coefi[4]*val2+calib_coefi[5]*val2*vsq;
        oval *= oval;
    }
    if (cal_mod_coef != NULL){
        if ((*cal_mod_coef).type != WatersCalibration::CoefficentsType_None){ // cal_mod
            double mzi = oval;
            if ((*cal_mod_coef).type == WatersCalibration::CoefficentsType_T0){ // for SQD
                oval = 0; double val2 = 1;
                for (unsigned int ii = 0; ii < (*cal_mod_coef).coeffients.size(); ii++){
                    oval += (*cal_mod_coef).coeffients[ii] * val2;
                    val2 *= mzi;
                }
                //oval = calib_coefi[0] + calib_coefi[1] * mzi + calib_coefi[2] * val2 + calib_coefi[3] * val2*mzi + calib_coefi[4] * val2*val2;// +calib_coefi[5] * val2*val2*mzi;
            }
            else {
                oval = 0; double vsq = sqrt(mzi), val2 = 1;
                for (unsigned int ii = 0; ii < (*cal_mod_coef).coeffients.size(); ii++){
                    oval += (*cal_mod_coef).coeffients[ii] * val2;
                    val2 *= vsq;
                }
                //oval = calib_coefi[0] + calib_coefi[1] * vsq + calib_coefi[2] * mzi + calib_coefi[3] * vsq*mzi + calib_coefi[4] * mz4 + calib_coefi[5] * mz4*vsq;
                oval *= oval;
            }
        }
    }
    return oval;
}

// parse calibration string
bool PicoLocalDecompress::parseCalibrationString(const string &coeff_str, std::vector<double> &coeffs)
{
    if (coeff_str.length()<10)
        return false;
    if (coeff_str[0]!='<' || coeff_str[1]!='T' || coeff_str[3]!='>') 
        return false;
    char ch = coeff_str[2];
    if (coeff_str[coeff_str.length()-5]!='<' || coeff_str[coeff_str.length()-4]!='/' || coeff_str[coeff_str.length()-3]!='T'  || coeff_str[coeff_str.length()-2]!=ch || coeff_str[coeff_str.length()-1]!='>' )
        return false;
    //string tag = "T"; tag.push_back(ch);
    string data = coeff_str.substr(4, coeff_str.length()); data = data.substr(0, data.length()-5);
    std::vector<string> splits;
    boost::split(splits, data, boost::is_any_of(","));
    for (unsigned int ii=0; ii<splits.size(); ii++){
        string str = splits[ii];
        coeffs.push_back(atof(str.c_str()));
    }
    return true;
}

// get compression info type
bool PicoLocalDecompress::getCompressionInfoType(const wstring &input_compressed_db_filename, string &compression_type)
{
    Sqlite* sql_in = new Sqlite(input_compressed_db_filename);
    if(!sql_in->openDatabase()){
        wcerr << "can't open output sqlite database = " << input_compressed_db_filename << endl;
        delete sql_in; return false;
    } 
    bool compression_info = sql_in->getPropertyValueFromCompressionInfo("Type", compression_type);
    delete sql_in;
    return compression_info;
}

int PicoLocalDecompress::writeFile(const std::vector<int> &hop_del, const wstring &filename)
{
    ofstream myfile;
    myfile.open (filename); 
    if (myfile.good()){
        for (unsigned int ii = 0; ii < hop_del.size(); ii++)
            myfile << hop_del[ii] << endl;
    }
    myfile.close();
    return 0;
}

bool PicoLocalDecompress::test_decompress_pico_byspec(const wstring &input_compressed_db_filename, const string &output_filename)
{
    Sqlite* sql_in = new Sqlite(input_compressed_db_filename);
    if (!sql_in->openDatabase()){
        wcerr << "can't open output sqlite database = " << input_compressed_db_filename << endl;
        delete sql_in; return false;
    }

    WatersCalibration wtrs_cal;
    if (!wtrs_cal.readFromSqlite(sql_in)){
        wcerr << "can't read calibration modification data from sqlite" << endl;
        sql_in->closeDatabase(); delete sql_in; return false;
    }

    int scan_count = sql_in->getPeakBlobCount();
    if (scan_count<0){
        std::cout << "can't read peak blob count from sqlite database" << endl;
        sql_in->closeDatabase(); delete sql_in; return false;
    }
    if (verbose) std::cout << "\ncompress:   spectrum contains " << scan_count << " scans" << endl;

    WatersCalibration::Coefficents cal_mod_coef; int cal_mod_func = 1, next_cal_mod_scan = 0;
    unsigned long long original_size = 0, compressed_size = 0;
    int scan = 0;
    sqlite3_stmt *stmt; bool success = false;
    sqlite3* db_in = sql_in->getDatabase();
    sqlite3_exec(db_in, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "Select PeaksMz FROM Peaks";// WHERE Id = 2";// 737";
    if (sqlite3_prepare_v2(db_in, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                int compressed_length = sqlite3_column_bytes(stmt, 0);
                //int pk_id = sqlite3_column_int(stmt, 0);
                unsigned char* compressed = (unsigned char*)sqlite3_column_blob(stmt, 0);

                if (scan == next_cal_mod_scan - 1){
                    std::vector<int> max_scan_id = wtrs_cal.getMaxScanId();
                    if (cal_mod_func < (int)max_scan_id.size())
                        next_cal_mod_scan = max_scan_id[cal_mod_func - 1];
                    cal_mod_coef = wtrs_cal.calibrationModificationCoefficients(cal_mod_func++);
                }
/*                sqlite3_stmt *stmt2; bool success2 = false;
                ostringstream cmd2; cmd2 << "SELECT NativeId FROM Spectra WHERE ID='" << pk_id << "'"; //<=============
                if (sqlite3_prepare_v2(sql_in->getDatabase(), cmd2.str().c_str(), -1, &stmt2, NULL) == SQLITE_OK){
                    int rc2 = sqlite3_step(stmt2);
                    if (rc2 == SQLITE_ROW || rc2 == SQLITE_DONE){
                        char* text = (char*)sqlite3_column_text(stmt2, 0);
                        if (text != NULL){
                            string str(text);
                            const char *sptr = strstr(str.c_str(), "function=");
                            if (sptr != NULL){
                                int function = atoi(sptr + 9);
                                if (function != cal_mod_func){
                                    cal_mod_coef = wtrs_cal.calibrationModificationCoefficients(function);
                                    cal_mod_func = function;
                                }
                            }
                        }
                        success2 = true;
                    }
                }
                sqlite3_finalize(stmt2);
                if (!success2){
                    if (verbose) cout << "can't read calibration modification data from sqlite -- " << string(sqlite3_errmsg(db_in)) << endl;
                    sqlite3_finalize(stmt);
                    sql_in->closeDatabase(); delete sql_in; return false;
                }*/
                double* restored_mz = 0; float* restored_intensity = 0; int restored_mz_length = 0;

                auto start = chrono::system_clock::now();
                if (!decompressRawType1_N(compressed, &cal_mod_coef, &restored_mz, &restored_intensity, &restored_mz_length, true, false)){
                    free_safe(compressed); sql_in->closeDatabase(); delete sql_in; return false;
                }
                auto duration = chrono::system_clock::now() - start;
                auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
                if (!compressed){
                    if (verbose) std::cout << "null compressed data" << endl; sql_in->closeDatabase(); delete sql_in; return false;
                }

                compressed_size += compressed_length; original_size += restored_mz_length * 12; // 8 bytes mz + 4 bytes intensity 
                std::cout << "   scan: " << scan << ", compress=" << compressed_length << ", orig=" << restored_mz_length * 12 << " bytes (cr=" << setprecision(5) << (float)restored_mz_length * 12 / compressed_length << setprecision(0) << ") [" << millis << "]" << endl;
                scan++; success = true;
                //if (scan == 10) //<================== temp
                //    break;

            }
            else if (rc == SQLITE_DONE) {
                success = true; break;
            }
            else {
                if (verbose) cout << "rc=" << rc << endl; break;
            }
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db_in, "COMMIT;", NULL, NULL, NULL);
    std::cout << "done! overall compression=" << compressed_size << " orig=" << original_size << " bytes (cr=" << setprecision(5) << (float)original_size / compressed_size << setprecision(0) << ")" << endl;
    sql_in->closeDatabase(); delete sql_in;
    return success;

}


_PICO_END

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
