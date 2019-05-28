// ********************************************************************************************
// PicoLocalCompress.cpp : The main compression module
//
//      @Author: D.Kletter    (c) Copyright 2014, PMI Inc.
// 
// This module reads in an uncompressed byspec2 file and produces a highly compressed byspec2 file.
// The algorithm uses a proprietary run length hop coding that leverages the sparse data nature
// of the underlying data.
//
// The algorithm can be applied to an entire byspec file or one spectra at a time:
// call PicoLocalCompress::compress(string input_db_filename, string out_compressed_db_filename) to 
// compress the entire file:
//
//      @Arguments:  <input_db_filename>  the full path to the input byspec2 file to compress.
//                   <out_compressed_db_filename>  full path to the compressed output byspec2, where
//                    the result is written
//
// Alternatively, call:
// PicoLocalCompress::compressSpectra(Sqlite* sql, const int scan, unsigned char** compressed, unsigned int* length)
// to compress one scanline at a time. In order to support high performance, the input file is opened only
// once in the beginning, and an sqlite pointer is passed for subsequent spectra, thereby avoiding the extra
// overhead associated with any subsequent closing and re-opeining the input file for each scan. The caller
// is responsible for freeing the blob memory after it has been written or copied.
//
//      @Arguments:  <sql> the sqlite pointer to an open input file
//                   <scan> the current spectra to compress 
//                   <**compressed> the resulting compressed blob of the entire scan, allocated by PicoLocalCompress 
//                   <*length> the length of the compresed blob in bytes 
//
//      @Returns: true if successful, false otherwise
//
// *******************************************************************************************

#include <ios>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <vector>
#include <algorithm>
//#include <assert.h> 
#include "Sqlite.h"
#include "PicoLocalCompress.h"
#include "PicoLocalDecompress.h"
#include "PicoUtil.h"
#include "Reader.h"
#include "Centroid.h"
#include "PicoCommon.h"


//#define TEST

#define COMP_VER 1
#define MIN_CENTROIDED_PEAKS 10
#define MIN_CENTROIDED_PEAK_MZ_SPAN 2.0
#define MIN_WATERS_PEAK_MZ_SPAN 10000000U

using namespace std;

_PICO_BEGIN

inline bool peak_sort_by_mz (const Peak &ci, const Peak &cj)
{ 
    if (ci.mz == cj.mz) return (ci.intensity > cj.intensity); 
    return (ci.mz < cj.mz); 
}

inline bool sort_by_count (const CntRecord &ci, const CntRecord &cj)
{ 
    if (ci.count == cj.count) return (ci.value < cj.value); 
    return (ci.count > cj.count); 
}

inline bool fpeak_sort_by_mz (const PeakF &ci, const PeakF &cj)
{ 
    if (ci.mz == cj.mz) return (ci.intensity > cj.intensity); 
    return (ci.mz < cj.mz); 
}


inline bool sort_by_count_D (const CntRecordD &ci, const CntRecordD &cj)
{ 
    if (ci.count == cj.count) return (ci.value < cj.value); 
    return (ci.count > cj.count); 
}

inline string commas(unsigned long long n)
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

PicoLocalCompress::PicoLocalCompress(bool verbose1)
{
    mz_len = 0;
    LSB_FACTOR = 4;
    min_mz = 9999999.0;
    max_mz = 0.0;
    max_intensity = 0;
    verbose = verbose1;

    rd_mz_millis = 0;
    rd_intens_millis = 0;
    compress_millis = 0;
}

PicoLocalCompress::PicoLocalCompress()
{
    mz_len = 0;
    LSB_FACTOR = 4;
    min_mz = 9999999.0;
    max_mz = 0.0;
    max_intensity = 0;
    verbose = false;

    rd_mz_millis = 0;
    rd_intens_millis = 0;
    compress_millis = 0;
}

PicoLocalCompress::~PicoLocalCompress()
{    

}

// compress file
bool PicoLocalCompress::compress(const std::wstring &input_db_filename, const std::wstring &out_compressed_db_filename,
                                 CompressType type, int single_scan)
{
    if (type==Type_Bruker1){
        return compressType0(input_db_filename, out_compressed_db_filename);

    } else if (type==Type_Bruker2){
        return compressType02(input_db_filename, out_compressed_db_filename);

    } else if (type==Type_Waters1){
        return compressType1(input_db_filename, out_compressed_db_filename, single_scan);

    } else if (type==Type_Centroided1){
        return compressCentroided(input_db_filename, out_compressed_db_filename, single_scan);

    } else if (type==Type_ABSciex1){
        return compressType4(input_db_filename, out_compressed_db_filename, single_scan);

    } else {
        cout << "unsupported decompression type" << endl;
        return false;
    }
}


// compress type 0 file
bool PicoLocalCompress::compressType0(const std::wstring &input_db_filename, const std::wstring &out_compressed_db_filename)
{
    wstring out_filename = out_compressed_db_filename;
    if (out_filename.length() == 0){
        out_filename = input_db_filename + L"_compressed.byspec2";
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
    if (!sql_out->createCopyByspecTablesToSqlite(input_db_filename, true)){
        wcerr << "can't create table in sqlite database = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }

    Sqlite* sql_in = new Sqlite(input_db_filename);
    if(!sql_in->openDatabase()){
        wcerr << "can't open output sqlite database = " << out_filename << endl;
        delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 

    int scan_count = sql_in->getPeakBlobCount(); 
    if (scan_count<0){
        std::cout << "can't read peak blob count from sqlite database" << endl; 
        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if (verbose) std::cout << "\ncompress:   spectrum contains " << scan_count << " scans" << endl;

    Predictor* pred = new Predictor(3);    
    unsigned long long original_size = 0, compressed_size = 0; //unsigned int max_cnt = 0;
    int scan = 0; unsigned long long intensity_sum = 0;
    double* vect = (double *)malloc(11*sizeof(double));
    if (!vect){
        if (verbose) std::cout << "vect malloc failed " << endl; delete pred; sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    }
    double* sx = &vect[0], *sy = &vect[7];
    std::vector<unsigned int> intensity, intensity_index; 
    sqlite3_stmt *stmt; bool success = false;
    sqlite3* db_in = sql_in->getDatabase(), *db_out = sql_out->getDatabase();
    sqlite3_exec(db_in, "BEGIN;", NULL, NULL, NULL);
    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    ostringstream cmd; cmd << "Select PeaksMz,PeaksIntensity FROM Peaks";
    ostringstream cmd1; cmd1 << "INSERT into Peaks(PeaksMz) values (?)"; 
    if (sqlite3_prepare_v2(db_in, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                memset((unsigned char *)vect, 0, 11*sizeof(double));  
                mz_len = sqlite3_column_bytes(stmt, 0)/8; sx[0]=mz_len;
                double* mz_blob = (double*)sqlite3_column_blob(stmt, 0);
                //writeMzsToFile(mz_blob, mz_len, "c:\\data\\mzs.txt");

                if (mz_blob[0]<min_mz) min_mz=mz_blob[0];
                for (int ii=0; ii<mz_len; ii++){
                    double yi = mz_blob[ii], xpi = 1; sy[0] += yi;
                    for (int jj=1; jj<7; jj++){
                        xpi *= ii; sx[jj] += xpi; 
                        if (jj<4) sy[jj] += xpi*yi;
                    }
                }
                if (mz_blob[mz_len-1]>max_mz) max_mz=mz_blob[mz_len-1]; 

                intensity_index.clear(); intensity.clear();
                int intens_size = sqlite3_column_bytes(stmt, 1)/4; 
                float* intens_blob = (float*)sqlite3_column_blob(stmt, 1);
                for (int ii=0; ii<intens_size; ii++){
                    float yi = intens_blob[ii];
                    if (yi>0.0){
                        int val = (int)yi/LSB_FACTOR; if (val>max_intensity) max_intensity = val; intensity_sum += val;
                        intensity.push_back(val); intensity_index.push_back(ii); 
                    }
                }

                auto start = chrono::system_clock::now();
                createSpectraHash(intensity);
                createHopHash(intensity_index);
                //writeFile("c://data//file_out.txt");

                const size_t size = 3+32+3*4+hop_recs.size()*4+intens_recs.size()*4+hop_dels.size()*4; // not hvals
                unsigned char* compressed = (unsigned char*)malloc(size*sizeof(unsigned char)); 
                if (compressed==NULL){
                    cout << "compressed buf allocation failed" << endl; sql_in->closeDatabase(); free_safe(vect); delete pred; delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                start = chrono::system_clock::now();
                pred->initialize(vect);
                double* data = pred->predict();
                if (!data){
                    cout << "null predict data" << endl; sql_in->closeDatabase(); free_safe(vect); delete pred; delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }
                unsigned char* ptr = createLocalDictionary(compressed, mz_len, data);
                free_safe(data); 

                unsigned char* ptr2 = generateIndices(intensity, ptr);
                auto duration = chrono::system_clock::now() - start;
                auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

                const int length = static_cast<int>(ptr2 - compressed);

                sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                if ((rc1=sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                    sqlite3_bind_blob(stmt1, 1, compressed, length, SQLITE_STATIC); 
                    rc1 = sqlite3_step(stmt1);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt1);
                free_safe(compressed);
                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); free_safe(vect); delete pred;
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                ostringstream cmd2; cmd2 << "INSERT into TIC(IntensitySum) values ('" << intensity_sum << "')"; 
                sqlite3_stmt *stmt2; success1 = false;  
                if ((rc1=sqlite3_prepare_v2(db_out, cmd2.str().c_str(), -1, &stmt2, NULL)) == SQLITE_OK){
                    rc1 = sqlite3_step(stmt2);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt2); 
                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write TIC data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); free_safe(vect); delete pred;
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                compressed_size += length; original_size += mz_len*12; // 8 bytes mz + 4 bytes intensity 
                if (verbose) std::cout << "   scan: " << scan << ", compress=" << length << ", orig=" << mz_len*12 << " bytes (cr=" << setprecision(5) << (float)mz_len*12/length << setprecision(0) << ") [" << millis << "]" << endl;

                //cout << "scan=" << scan << ", max=" << max_intensity << ", max_cnt=" << max_cnt << endl;
                scan++; 
                //if (scan == 10){ //<================== temp
                //    success = true; break;
                //}

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
    free_safe(vect); delete pred;
    if (verbose) std::cout << "done! overall compression=" << commas(compressed_size).c_str() << " orig=" << commas(original_size).c_str() << " bytes (cr=" << setprecision(5) << (float)original_size/compressed_size << setprecision(0) << ")" << endl;
    //if (!success)
    //    cout << "can't read mz or intensity blobs from sqlite -- " << string(sqlite3_errmsg(db_in)) << endl; 

    //if (!sql_out->addPropertyValueToPeaksDictionary("Flavor", "B1.0.0"))
    //    success = false;

    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; 
    return success;
}

// compress type 02 file
bool PicoLocalCompress::compressType02(const std::wstring &input_db_filename, const std::wstring &out_compressed_db_filename)
{
    wstring out_filename = out_compressed_db_filename;
    if (out_filename.length() == 0){
        out_filename = input_db_filename + L"_compressed.byspec2";
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
    if (!sql_out->createCopyByspecTablesToSqlite(input_db_filename.c_str(), true)){
        wcerr << "can't create table in sqlite database = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }

    Sqlite* sql_in = new Sqlite(input_db_filename);
    if(!sql_in->openDatabase()){
        wcerr << "can't open output sqlite database = " << out_filename << endl;
        delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 

    int scan_count = sql_in->getPeakBlobCount(); 
    if (scan_count<0){
        std::cout << "can't read peak blob count from sqlite database" << endl; 
        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if (verbose) std::cout << "\ncompress:   spectrum contains " << scan_count << " scans" << endl;

    unsigned long long original_size = 0, compressed_size = 0; //unsigned int max_cnt = 0;
    int scan = 0; //unsigned long long intensity_sum = 0;
    sqlite3_stmt *stmt; bool success = false;
    sqlite3* db_in = sql_in->getDatabase(), *db_out = sql_out->getDatabase();
    sqlite3_exec(db_in, "BEGIN;", NULL, NULL, NULL);
    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    ostringstream cmd; cmd << "Select PeaksMz,PeaksIntensity FROM Peaks";
    ostringstream cmd1; cmd1 << "INSERT into Peaks(PeaksMz) values (?)"; 
    if (sqlite3_prepare_v2(db_in, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                mz_len = sqlite3_column_bytes(stmt, 0)/8; //sx[0]=mz_len;
                double* mz_blob = (double*)sqlite3_column_blob(stmt, 0);
                //writeMzsToFile(mz_blob, mz_len, "c:\\data\\mzs.txt");

                //int intens_size = sqlite3_column_bytes(stmt, 1)/4;
                float* intens_blob = (float*)sqlite3_column_blob(stmt, 1);

                unsigned char* compressed = 0; unsigned int compressed_length = 0; unsigned long long intensity_sum = 0;
                auto start = chrono::system_clock::now();
                if (!compressRawType02(mz_blob, intens_blob, mz_len, &compressed, &compressed_length, &intensity_sum)){
                    free_safe(compressed); sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }
                auto duration = chrono::system_clock::now() - start;
                auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
                if (!compressed){
                    if (verbose) std::cout << "null compressed data" << endl; sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                if ((rc1=sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                    sqlite3_bind_blob(stmt1, 1, compressed, compressed_length, SQLITE_STATIC); 
                    rc1 = sqlite3_step(stmt1);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt1);
                free_safe(compressed);
                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                ostringstream cmd2; cmd2 << "INSERT into TIC(IntensitySum) values ('" << intensity_sum << "')"; 
                sqlite3_stmt *stmt2; success1 = false;  
                if ((rc1=sqlite3_prepare_v2(db_out, cmd2.str().c_str(), -1, &stmt2, NULL)) == SQLITE_OK){
                    rc1 = sqlite3_step(stmt2);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt2); 
                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write TIC data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                compressed_size += compressed_length; original_size += mz_len*12; // 8 bytes mz + 4 bytes intensity 
                if (verbose) std::cout << "   scan: " << scan << ", compress=" << compressed_length << ", orig=" << mz_len*12 << " bytes (cr=" << setprecision(5) << (float)mz_len*12/compressed_length << setprecision(0) << ") [" << millis << "]" << endl;
                scan++; success = true; 
                //if (scan == 10) //<================== temp
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
    if (verbose) std::cout << "done! overall compression=" << commas(compressed_size).c_str() << " orig=" << commas(original_size).c_str() << " bytes (cr=" << setprecision(5) << (float)original_size/compressed_size << setprecision(0) << ")" << endl;
    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; 
    return success;
}


// compress centroided type file
bool PicoLocalCompress::compressCentroided(const std::wstring &input_db_filename,
                                           const std::wstring &out_compressed_db_filename, int single_scan)
{
    wstring out_filename = out_compressed_db_filename;
    if (out_filename.length() == 0){
        out_filename = input_db_filename + L"_compressed.byspec2";
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
    if (!sql_out->createCopyByspecTablesToSqlite(input_db_filename.c_str(), true)){
        wcerr << "can't create table in sqlite database = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }

    Sqlite* sql_in = new Sqlite(input_db_filename);
    if(!sql_in->openDatabase()){
        wcerr << "can't open output sqlite database = " << out_filename << endl;
        delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 

    int scan_count = sql_in->getPeakBlobCount(); 
    if (scan_count<0){
        std::cout << "can't read peak blob count from sqlite database" << endl; 
        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if (verbose) std::cout << "\ncompress:   spectrum contains " << scan_count << " scans" << endl;

    unsigned long long original_size = 0, compressed_size = 0; //unsigned int max_cnt = 0;
    int scan = 0; //unsigned long long intensity_sum = 0;
    sqlite3_stmt *stmt; bool success = false;
    sqlite3* db_in = sql_in->getDatabase(), *db_out = sql_out->getDatabase();
    sqlite3_exec(db_in, "BEGIN;", NULL, NULL, NULL);
    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    ostringstream cmd; 
    if (single_scan>=0){
        scan = single_scan;
        cmd << "Select PeaksMz,PeaksIntensity FROM Peaks WHERE Id='" << single_scan << "'"; //<==================
    } else
        cmd << "Select PeaksMz,PeaksIntensity FROM Peaks";
    ostringstream cmd1; cmd1 << "INSERT into Peaks(PeaksMz,PeaksIntensity) values (?,?)"; 
    if (sqlite3_prepare_v2(db_in, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                mz_len = sqlite3_column_bytes(stmt, 0)/8; 
                double* mz_blob = (double*)sqlite3_column_blob(stmt, 0);
                
                //int intens_size = sqlite3_column_bytes(stmt, 1)/4; 
                float* intens_blob = (float*)sqlite3_column_blob(stmt, 1);

                unsigned char* compressed_mz = 0, *compressed_intensity = 0; 
                unsigned int compressed_mz_length = 0, compressed_intensity_length = 0; unsigned long long intensity_sum = 0;
                auto start = chrono::system_clock::now();
                if (!compressRawTypeCentroided1(mz_blob, intens_blob, mz_len, &compressed_mz, &compressed_mz_length, &compressed_intensity, &compressed_intensity_length, &intensity_sum, true)){
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }
                auto duration = chrono::system_clock::now() - start;
                auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

                sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                if ((rc1=sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                    sqlite3_bind_blob(stmt1, 1, compressed_mz, compressed_mz_length, SQLITE_STATIC); 
                    sqlite3_bind_blob(stmt1, 2, compressed_intensity, compressed_intensity_length, SQLITE_STATIC); 
                    rc1 = sqlite3_step(stmt1);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt1);
                free_safe(compressed_mz); free_safe(compressed_intensity);
                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                unsigned int combined_length = compressed_mz_length + compressed_intensity_length;
                compressed_size += combined_length; original_size += mz_len*12; // 8 bytes mz + 4 bytes intensity 
                if (verbose) std::cout << "   scan: " << scan << ", compress=" << combined_length << ", orig=" << mz_len*12 << " bytes (cr=" << setprecision(5) << (float)mz_len*12/combined_length << setprecision(0) << ") [" << millis << "]" << endl;
                scan++; success = true; 

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
    if (verbose) std::cout << "done! overall compression=" << commas(compressed_size).c_str() << " orig=" << commas(original_size).c_str() << " bytes (cr=" << setprecision(5) << (float)original_size/compressed_size << setprecision(0) << ")" << endl;
    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; 
    return success;
}

// compress type 1 file
bool PicoLocalCompress::compressType1(const std::wstring &input_db_filename,
                                      const std::wstring &out_compressed_db_filename, int single_scan)
{
    wstring out_filename = out_compressed_db_filename;
    if (out_filename.length() == 0){
        out_filename = input_db_filename + L"_compressed.byspec2";
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
    if (!sql_out->createCopyByspecTablesToSqlite(input_db_filename.c_str(), true)){
        wcerr << "can't create table in sqlite database = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }

    Sqlite* sql_in = new Sqlite(input_db_filename);
    if(!sql_in->openDatabase()){
        wcerr << "can't open output sqlite database = " << out_filename << endl;
        delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 

    int scan_count = sql_in->getPeakBlobCount(); 
    if (scan_count<0){
        std::cout << "can't read peak blob count from sqlite database" << endl; 
        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if (verbose) std::cout << "\ncompress:   spectrum contains " << scan_count << " scans" << endl;

    unsigned long long original_size = 0, compressed_size = 0; //unsigned int max_cnt = 0;
    int scan = 0; unsigned long long intensity_sum = 0;
    sqlite3_stmt *stmt; bool success = false;
    sqlite3* db_in = sql_in->getDatabase(), *db_out = sql_out->getDatabase();
    sqlite3_exec(db_in, "BEGIN;", NULL, NULL, NULL); sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    ostringstream cmd; 
    if (single_scan>=0){
        scan = single_scan;
        cmd << "Select PeaksMz,PeaksIntensity FROM Peaks WHERE Id='" << single_scan << "'"; //<==================
    } else {
        cmd << "Select PeaksMz,PeaksIntensity FROM Peaks";
    }
    ostringstream cmd1; cmd1 << "INSERT into Peaks(PeaksMz,PeaksIntensity) values (?,?)";
    if (sqlite3_prepare_v2(db_in, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                mz_len = sqlite3_column_bytes(stmt, 0)/8;  
                if (mz_len<=2){
                    success = true; break;  // skip UV data
                }
                double* mz_blob = (double*)sqlite3_column_blob(stmt, 0);
                //writeMzsToFile(mz_blob, mz_len, "c:\\data\\mzs.txt");

                unsigned char* compressed_mz = (unsigned char*)malloc(mz_len*sizeof(double)); 
                if (compressed_mz == NULL){
                    cout << "compressed m/z buf allocation failed" << endl; sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                unsigned int compressed_mz_length = 0; 
                auto start = chrono::system_clock::now();
                if (!compressMzType1(mz_blob, mz_len, compressed_mz, &compressed_mz_length)){
                    free_safe(compressed_mz); sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                } 

                //int intens_size = sqlite3_column_bytes(stmt, 1)/4;
                float* intens_blob = (float*)sqlite3_column_blob(stmt, 1); 
                
                unsigned char* compressed_intensity = 0; unsigned int compressed_intensity_length = 0; 
                if (!compressIntensityType1(intens_blob, mz_len, &compressed_intensity, &compressed_intensity_length, &intensity_sum)){
                    free_safe(compressed_mz); sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }
                auto duration = chrono::system_clock::now() - start;
                auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

                //int compressed_intens_length = ptr2 - compressed_intens;  
                int combined_length = compressed_mz_length + compressed_intensity_length; //compressed_intens_length;
                compressed_size += combined_length; original_size += mz_len*12; // 8 bytes mz + 4 bytes intensity 
            
                sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                if ((rc1=sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                    sqlite3_bind_blob(stmt1, 1, (unsigned char*)compressed_mz, compressed_mz_length, SQLITE_STATIC); 
                    sqlite3_bind_blob(stmt1, 2, (unsigned char*)compressed_intensity, compressed_intensity_length, SQLITE_STATIC); 
                    rc1 = sqlite3_step(stmt1);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt1); 
                free_safe(compressed_intensity); free_safe(compressed_mz);

                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                ostringstream cmd2; cmd2 << "INSERT into TIC(IntensitySum) values ('" << intensity_sum << "')"; 
                sqlite3_stmt *stmt2; success1 = false;  
                if ((rc1=sqlite3_prepare_v2(db_out, cmd2.str().c_str(), -1, &stmt2, NULL)) == SQLITE_OK){
                    rc1 = sqlite3_step(stmt2);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt2); 
                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write TIC data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                if (verbose) std::cout << dec << "   scan: " << scan << ", compress=" << combined_length << ", orig=" << mz_len*12 << " bytes (cr=" << setprecision(5) << (float)mz_len*12/combined_length << setprecision(0) << ") [" << millis << "]" << endl;                                    
                //cout << "scan=" << scan << ", max=" << max_intensity << ", max_cnt=" << max_cnt << endl;
                scan++; success = true;
                //if (scan == 3) //<================== temp
                //     break;

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
    if (verbose) std::cout << "done! overall compression=" << commas(compressed_size).c_str() << " orig=" << commas(original_size).c_str() << " bytes (cr=" << setprecision(5) << (float)original_size/compressed_size << setprecision(0) << ")" << endl;
    //if (!success)
    //    cout << "can't read mz or intensity blobs from sqlite -- " << string(sqlite3_errmsg(db_in)) << endl; 

    //if (!sql_out->addPropertyValueToPeaksDictionary("Flavor", "B1.0.0"))
    //    success = false;

    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; 
    return success;
}


// compress type 4 file ABSciex
bool PicoLocalCompress::compressType4(const wstring &input_db_filename, const wstring &out_compressed_db_filename, int single_scan)
{
    wstring out_filename = out_compressed_db_filename;
    if (out_filename.length() == 0){
        out_filename = input_db_filename + L"_compressed.byspec2";
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
    if (!sql_out->createCopyByspecTablesToSqlite(input_db_filename.c_str(), true)){
        wcerr << "can't create table in sqlite database = " << out_filename << endl;
        sql_out->closeDatabase(); delete sql_out; return false;
    }

    Sqlite* sql_in = new Sqlite(input_db_filename);
    if(!sql_in->openDatabase()){
        wcerr << "can't open output sqlite database = " << out_filename << endl;
        delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 

    int scan_count = sql_in->getPeakBlobCount(); 
    if (scan_count<0){
        std::cout << "can't read peak blob count from sqlite database" << endl; 
        sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
    } 
    if (verbose) std::cout << "\ncompress:   spectrum contains " << scan_count << " scans" << endl;

    unsigned long long original_size = 0, compressed_size = 0; //unsigned int max_cnt = 0;
    int scan = 0; //unsigned long long intensity_sum = 0;
    sqlite3_stmt *stmt; bool success = false;
    sqlite3* db_in = sql_in->getDatabase(), *db_out = sql_out->getDatabase();
    sqlite3_exec(db_in, "BEGIN;", NULL, NULL, NULL);
    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    ostringstream cmd; 
    if (single_scan>=0){
        scan = single_scan;
        cmd << "Select PeaksMz,PeaksIntensity FROM Peaks WHERE Id='" << single_scan << "'"; //<==================
    } else
        cmd << "Select PeaksMz,PeaksIntensity FROM Peaks";
    ostringstream cmd1; cmd1 << "INSERT into Peaks(PeaksMz) values (?)";
    if (sqlite3_prepare_v2(db_in, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                mz_len = sqlite3_column_bytes(stmt, 0)/8;  
                double* mz_blob = (double*)sqlite3_column_blob(stmt, 0);
                //int intens_size = sqlite3_column_bytes(stmt, 1)/4;
                float* intens_blob = (float*)sqlite3_column_blob(stmt, 1); 
                //writeMzsToFile(mz_blob, mz_len, "c:\\data\\mzs.txt");

                unsigned char* compressed = (unsigned char*)malloc((mz_len+mz_len/2)*sizeof(double)); 
                if (compressed == NULL){
                    cout << "compressed m/z buf allocation failed" << endl; 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }

                unsigned int compressed_length = 0; unsigned long long intensity_sum = 0;
                auto start = chrono::system_clock::now();
                if (!compressRawType4(mz_blob, intens_blob, mz_len, compressed, &compressed_length, &intensity_sum)){
                    free_safe(compressed); sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }
                auto duration = chrono::system_clock::now() - start;
                auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

                compressed_size += compressed_length; original_size += mz_len*12; // 8 bytes mz + 4 bytes intensity 
            
                sqlite3_stmt *stmt1; bool success1 = false; int rc1;
                if ((rc1=sqlite3_prepare_v2(db_out, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
                    sqlite3_bind_blob(stmt1, 1, (unsigned char*)compressed, compressed_length, SQLITE_STATIC); 
                    rc1 = sqlite3_step(stmt1);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt1); 
                free_safe(compressed);

                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }
/*
                ostringstream cmd2; cmd2 << "INSERT into TIC(IntensitySum) values ('" << intensity_sum << "')"; 
                sqlite3_stmt *stmt2; success1 = false;  
                if ((rc1=sqlite3_prepare_v2(db_out, cmd2.str().c_str(), -1, &stmt2, NULL)) == SQLITE_OK){
                    rc1 = sqlite3_step(stmt2);
                    if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
                        success1 = true;
                    } 
                }
                sqlite3_finalize(stmt2); 
                if (!success1){
                    if (verbose) cout << "rc1=" << rc1 << ": can't write TIC data to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
                    sqlite3_finalize(stmt); 
                    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; return false;
                }
*/
                if (verbose) std::cout << dec << "   scan: " << scan << ", compress=" << compressed_length << ", orig=" << mz_len*12 << " bytes (cr=" << setprecision(5) << (float)mz_len*12/compressed_length << setprecision(0) << ") [" << millis << "]" << endl;                                    
                //cout << "scan=" << scan << ", max=" << max_intensity << ", max_cnt=" << max_cnt << endl;
                scan++; success = true;
                //if (scan == 3) // temp
                //     break;

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
    if (verbose) std::cout << "done! overall compression=" << commas(compressed_size).c_str() << " orig=" << commas(original_size).c_str() << " bytes (cr=" << setprecision(5) << (float)original_size/compressed_size << setprecision(0) << ")" << endl;

    sql_in->closeDatabase(); delete sql_in; sql_out->closeDatabase(); delete sql_out; 
    return success;
}

// initialize a raw to sqlite compression
// this opens an output sqlite file, clears the peaks table, and prepares for writing compressed data
// all metadata information is copied from a referenced input sqlite file
bool PicoLocalCompress::initialize_raw2sqlite(const std::wstring &input_db_filename,
                                              const std::wstring &out_compressed_db_filename, Sqlite** sql_out)
{
    wstring out_filename = out_compressed_db_filename;
    if (out_filename.length() == 0){
        out_filename = input_db_filename + L"_compressed.byspec2";
    }

    *sql_out = new Sqlite(out_filename);
    if(!(*sql_out)->openDatabase()){
        wcerr << "can't open output sqlite database = " << out_filename << endl;
        delete sql_out; return false;
    } 
    if(!(*sql_out)->setMode()){
        wcerr << "can't set output sqlite mode = " << out_filename << endl;
        (*sql_out)->closeDatabase(); delete *sql_out; return false;
    } 
    if(!(*sql_out)->createNewCompressedByspecFile()){
        wcerr << "can't create compressed sqlite file = " << out_filename << endl;
        (*sql_out)->closeDatabase(); delete *sql_out; return false;
    }
/*    if (!(*sql_out)->createCopyByspecTablesToSqlite(input_db_filename.c_str())){
        cerr << "can't create table in sqlite database = " << string(out_filename) << endl; 
        (*sql_out)->closeDatabase(); delete *sql_out; return false;
    }*/
    mz_len = 0;
    return true;
}

// add spectra to compressed sqlite file
bool PicoLocalCompress::add_spectra2sqlite(int peaksId, const double* uncompressed_mz, const double* uncompressed_intensity, int uncompressed_mz_length, Sqlite* sql_out)
{
    //unsigned long long original_size = 0, compressed_size = 0;
    //int scan = 0;
    unsigned long long intensity_sum = 0;
    double* vect = (double *)calloc(11, sizeof(double)); 
    if (!vect){
        cerr << "vect allocation failed" << endl; return false;
    }
    double* sx = &vect[0], *sy = &vect[7];

    auto start = chrono::system_clock::now();
    sx[0] = uncompressed_mz_length;
    for (int ii=0; ii<uncompressed_mz_length; ii++){
        double yi = ((double*)uncompressed_mz)[ii], xpi = 1; sy[0] += yi;
        if (ii==0)
            if (yi<min_mz) min_mz=yi;
        for (int jj=1; jj<7; jj++){
            xpi *= ii; sx[jj] += xpi; 
            if (jj<4) sy[jj] += xpi*yi;
        }
        if (ii==uncompressed_mz_length-1)
            if (yi>max_mz) max_mz=yi;
    }

    std::vector<unsigned int> intensity_index, intensity;
    for (int ii=0; ii<uncompressed_mz_length; ii++){
        double yi = ((double*)uncompressed_intensity)[ii];
        if (yi>0.0){
            int val = (int)yi/LSB_FACTOR; if (val>max_intensity) max_intensity = val; intensity_sum += val;
            intensity.push_back(val); intensity_index.push_back(ii); 
        }
    }

    createSpectraHash(intensity);
    createHopHash(intensity_index);
    //writeFile("c://data//file_out.txt");

    const size_t size = 3+32+3*4+hop_recs.size()*4+intens_recs.size()*4+hop_dels.size()*4; 
    unsigned char* compressed = (unsigned char*)malloc(size*sizeof(unsigned char)); 
    if (compressed == NULL){
        if (verbose) cout << "malloc out of memory failure" << endl; free_safe(vect); return false;
    }

    Predictor* pred = new Predictor(3);
    pred->initialize(vect);
    double* data = pred->predict();
    if (!data){
        if (verbose) cout << "null vect predict" << endl; free_safe(compressed); delete pred; free_safe(vect); return false;
    }
    unsigned char* ptr = createLocalDictionary(compressed, uncompressed_mz_length, data);
    free_safe(data); free_safe(vect); delete pred;

    unsigned char* ptr2 = generateIndices(intensity, ptr);
    auto duration = chrono::system_clock::now() - start;
    auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

    ptrdiff_t compressed_length = ptr2 - compressed; 

    sqlite3* db_out = sql_out->getDatabase();
    sqlite3_exec(db_out, "BEGIN;", NULL, NULL, NULL); 
    ostringstream cmd; cmd << "INSERT into Peaks(Id, PeaksMz) values (?,?)"; 
    sqlite3_stmt *stmt; bool success1 = false; int rc1;
    if ((rc1=sqlite3_prepare_v2(db_out, cmd.str().c_str(), -1, &stmt, NULL)) == SQLITE_OK){
        sqlite3_bind_int(stmt, 1, peaksId);
        sqlite3_bind_blob(stmt, 2, compressed, static_cast<int>(compressed_length), SQLITE_STATIC); 
        rc1 = sqlite3_step(stmt);
        if (rc1 == SQLITE_ROW || rc1 == SQLITE_DONE){
            success1 = true;
        }
    }
    sqlite3_finalize(stmt);
    free_safe(compressed);
    if (!success1){
        if (verbose) cout << "rc1=" << rc1 << ": can't write compressed peaks to sqlite -- " << string(sqlite3_errmsg(db_out)) << endl; 
    }
    if (verbose) std::cout << "   scan: " << mz_len++ << ", compress=" << compressed_length << ", orig=" << uncompressed_mz_length*16 << " bytes (cr=" << setprecision(5) << (float)uncompressed_mz_length*16/compressed_length << setprecision(0) << ") [" << millis << "]" << endl;
    return success1;
}

// close the output sqlite file
void close_raw2sqlite(Sqlite* sql_out)
{
    sql_out->closeDatabase(); delete sql_out; 
}

// compressor raw type 0 binary data
bool PicoLocalCompress::compressRawType0(const double* uncompressed_mz, const double* uncompressed_intensity, int uncompressed_mz_length, unsigned char** compressed,
                unsigned int* compressed_length, unsigned long long* intensity_sum)
{
    //unsigned long long original_size = 0, compressed_size = 0;
    //int scan = 0;
    unsigned long long intensity_sum_i = 0;
    double* vect = (double *)calloc(11, sizeof(double)); 
    if (!vect){
        cerr << "vect allocation failed" << endl; return false;
    }
    double* sx = &vect[0], *sy = &vect[7];

    auto start = chrono::system_clock::now();
    sx[0] = uncompressed_mz_length;
    for (int ii=0; ii<uncompressed_mz_length; ii++){
        double yi = ((double*)uncompressed_mz)[ii], xpi = 1; sy[0] += yi;
        if (ii==0)
            if (yi<min_mz) min_mz=yi;
        for (int jj=1; jj<7; jj++){
            xpi *= ii; sx[jj] += xpi; 
            if (jj<4) sy[jj] += xpi*yi;
        }
        if (ii==uncompressed_mz_length-1)
            if (yi>max_mz) max_mz=yi;
    }

    std::vector<unsigned int> intensity_index, intensity;
    intensity_index.reserve(uncompressed_mz_length);
    intensity.reserve(uncompressed_mz_length);
    for (int ii=0; ii<uncompressed_mz_length; ii++){
        double yi = ((double*)uncompressed_intensity)[ii];
        if (yi>0.0){
            unsigned int val = (unsigned int)yi/LSB_FACTOR; if (val>max_intensity) max_intensity = val; intensity_sum_i += val;
            intensity.push_back(val); intensity_index.push_back(ii); 
        }
    }
    *intensity_sum = intensity_sum_i;

    createSpectraHash(intensity);
    createHopHash(intensity_index);
    //writeFile("c://data//file_out.txt");

    const size_t size = 3+32+3*4+hop_recs.size()*4+intens_recs.size()*4+hop_dels.size()*4; 
    *compressed = (unsigned char*)malloc(size*sizeof(unsigned char)); 
    if (*compressed == NULL){
        if (verbose) cout << "out of memory malloc failure" << endl; free_safe(vect); return false;
    }

    Predictor* pred = new Predictor(3);
    pred->initialize(vect);
    double* data = pred->predict();
    if (!data){
        if (verbose) cout << "null vect predict" << endl; free_safe(*compressed); delete pred; free_safe(vect); return false;
    }
    unsigned char* ptr = createLocalDictionary(*compressed, uncompressed_mz_length, data);
    free_safe(data); free_safe(vect); delete pred;

    unsigned char* ptr2 = generateIndices(intensity, ptr);
    auto duration = chrono::system_clock::now() - start;
    auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

    *compressed_length = static_cast<unsigned int>(ptr2 - *compressed); 
    free_safe(*compressed); //<===========
    if (verbose) std::cout << "   scan: " << mz_len++ << ", compress=" << *compressed_length << ", orig=" << uncompressed_mz_length*16 << " bytes (cr=" << setprecision(5) << (float)uncompressed_mz_length*16/(*compressed_length) << setprecision(0) << ") [" << millis << "]" << endl;

    return true;
}

// compressor raw type 1 binary data
bool PicoLocalCompress::compressRawType1(const double* uncompressed_mz, const float* uncompressed_intensity, const int uncompressed_mz_length, unsigned char** compressed_mz, unsigned int* compressed_mz_length,
        unsigned char** compressed_intensity, unsigned int* compressed_intensity_length, unsigned long long* intensity_sum)
{
    //unsigned long long original_size = 0, compressed_size = 0;
    //int scan = 0;
    *compressed_mz_length = 0; 
    *compressed_mz = (unsigned char*)malloc(uncompressed_mz_length*sizeof(double)); 
    if (*compressed_mz == NULL){
        cout << "compressed m/z buf allocation failed" << endl; return false;
    }

    auto start = chrono::system_clock::now();
    if (!compressMzType1(uncompressed_mz, uncompressed_mz_length, *compressed_mz, compressed_mz_length)){
        return false;
    }

    *compressed_intensity_length = 0; 
    if (!compressIntensityType1(uncompressed_intensity, uncompressed_mz_length, compressed_intensity, compressed_intensity_length, intensity_sum)){
        return false;
    }
    auto duration = chrono::system_clock::now() - start;
    auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

    //unsigned int combined_length = *compressed_mz_length + *compressed_intensity_length;
    if (verbose) std::cout << "   scan: " << mz_len++ << ", compress=" << *compressed_mz_length << ", orig=" << uncompressed_mz_length*16 << " bytes (cr=" << setprecision(5) << (float)uncompressed_mz_length*16/(*compressed_mz_length) << setprecision(0) << ") [" << millis << "]" << endl;

    return true;
}

// compressor raw type 1 binary data unsigned integer data
bool PicoLocalCompress::compressRawType1_I(const double* uncompressed_mz, const unsigned int* uncompressed_intensity, int uncompressed_mz_length, unsigned char** compressed_mz, unsigned int* compressed_mz_length,
        unsigned char** compressed_intensity, unsigned int* compressed_intensity_length, unsigned long long* intensity_sum)
{
    //unsigned long long original_size = 0, compressed_size = 0;
    //int scan = 0;
    *compressed_mz_length = 0; 
    *compressed_mz = (unsigned char*)malloc(uncompressed_mz_length*sizeof(double)); 
    if (*compressed_mz == NULL){
        cout << "compressed m/z buf allocation failed" << endl; return false;
    }

    auto start = chrono::system_clock::now();
    if (!compressMzType1(uncompressed_mz, uncompressed_mz_length, *compressed_mz, compressed_mz_length)){
        return false;
    }

    *compressed_intensity_length = 0; 
    if (!compressIntensityType1_I(uncompressed_intensity, uncompressed_mz_length, compressed_intensity, compressed_intensity_length, intensity_sum)){
        return false;
    }
    auto duration = chrono::system_clock::now() - start;
    auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
    compress_millis = millis;

    //unsigned int combined_length = *compressed_mz_length + *compressed_intensity_length;
    if (verbose) std::cout << "   scan: " << mz_len++ << ", compress=" << *compressed_mz_length << ", orig=" << uncompressed_mz_length*16 << " bytes (cr=" << setprecision(5) << (float)uncompressed_mz_length*16/(*compressed_mz_length) << setprecision(0) << ") [" << millis << "]" << endl;

    return true;
}


// compress one type 0 spectra
bool PicoLocalCompress::compressSpectraType0(Sqlite* sql, int scan, unsigned char** compressed, unsigned int* length)
{
    int max_intensity1 = 0; double* data = 0;
    std::vector<unsigned int> intensity_index, intensity;
    if (!readSpectra(sql, scan, &data, intensity_index, intensity, &max_intensity1)){
        free_safe(data); return false;
    }
    if (!data){
        if (verbose) std::cout << "null data" << endl;
        return false;
    }

    auto start = chrono::system_clock::now();

    max_intensity = max_intensity1;
    createSpectraHash(intensity);
    createHopHash(intensity_index);
    //writeFile("c://data//file_out.txt");

    const size_t size = 3+32+3*4+hop_recs.size()*4+intens_recs.size()*4+hop_dels.size()*4; // not hvals
    *compressed = (unsigned char*)malloc(size*sizeof(unsigned char)); 
    if (*compressed == NULL){
        cout << "compressed m/z buf allocation failed" << endl; free_safe(data); return false;
    }

    unsigned char* ptr = createLocalDictionary(*compressed, mz_len, data);
    free_safe(data); 

    start = chrono::system_clock::now();
    unsigned char* ptr2 = generateIndices(intensity, ptr);
    auto duration = chrono::system_clock::now() - start;
    auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
    compress_millis = millis;

    *length = static_cast<unsigned int>(ptr2 - *compressed);
    return true;
}

bool PicoLocalCompress::readSpectra(Sqlite* sql, int scan, double** data, std::vector<unsigned int> &intensity_index,
                                    std::vector<unsigned int> &intensity, int* max_intensity1)
{
    LSB_FACTOR = sql->getLsbFactor();

    auto start = chrono::system_clock::now();
    if (!sql->getPredictedPeakMzData(scan, data, &mz_len)){
        return false; 
    }
    auto duration = chrono::system_clock::now() - start;
    auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
    rd_mz_millis = millis;

    start = chrono::system_clock::now();
    if (!sql->getReducedPeakIntensityData(scan, intensity_index, intensity, max_intensity1)){
        return false; 
    }
    duration = chrono::system_clock::now() - start;
    millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
    rd_intens_millis = millis;

    return true;
}

void PicoLocalCompress::createSpectraHash(const std::vector<unsigned int> &intensity)
{
    std::vector<unsigned int> hcnts; hmap.clear(); hvals.clear();
    for (unsigned int ii=0; ii<intensity.size(); ii++){
        unsigned int val = intensity[ii], id;
        map<unsigned int, unsigned int>::iterator itr = hmap.find(val);
        if (itr == hmap.end()) {
            id = static_cast<unsigned int>(hmap.size());
            hmap.insert(
                std::pair<unsigned int, unsigned int>(val, static_cast<unsigned int>(hmap.size())));
            hcnts.push_back(1);
            hvals.push_back(val);
        } else {
            id = itr->second;
            hcnts[id] += 1;
        }
    }
    intens_recs.clear();
    intens_recs.reserve(hcnts.size());
    for (unsigned int ii=0; ii<hcnts.size(); ii++){
        intens_recs.push_back(CntRecord(hcnts[ii], hvals[ii], ii));
    }
    sort(intens_recs.begin(), intens_recs.end(), sort_by_count);
}

void PicoLocalCompress::createHopHash(const std::vector<unsigned int> &intensity_index)
{
    unsigned int prev_hop = 0, hop_max = 0; hop_map.clear(); hop_dels.clear();
    std::vector<unsigned int> hop_vals, hop_cnts;
    for (unsigned int ii=0; ii<intensity_index.size(); ii++){
        unsigned int val = intensity_index[ii], del = val - prev_hop, id; hop_dels.push_back(del); if (del>hop_max) hop_max=del;
        prev_hop = val; 
        map<unsigned int, unsigned int>::iterator itr = hop_map.find(del);
        if (itr == hop_map.end()) {
            id = static_cast<unsigned int>(hop_map.size());
            hop_map.insert(std::pair<unsigned int, unsigned int>(
                del, static_cast<unsigned int>(hop_map.size())));
            hop_cnts.push_back(1);
            hop_vals.push_back(del);
        } else {
            id = itr->second;
            hop_cnts[id] += 1;
        }
    }
    hop_recs.clear();
    for (unsigned int ii=0; ii<hop_cnts.size(); ii++){
        hop_recs.push_back(CntRecord(hop_cnts[ii], hop_vals[ii], ii));
    }
    sort(hop_recs.begin(), hop_recs.end(), sort_by_count);
}

void PicoLocalCompress::createCombinedSpectraHash(const std::vector<unsigned int> &intensity)
{
    std::vector<unsigned int> hcnts; hmap.clear(); hvals.clear();
    for (unsigned int ii=0; ii<intensity.size(); ii++){
        unsigned int val = intensity[ii]; unsigned int id;
        map<unsigned int, unsigned int>::iterator itr = hmap.find(val);
        if (itr == hmap.end()) {
            id = static_cast<unsigned int>(hmap.size());
            hmap.insert(
                std::pair<unsigned int, unsigned int>(val, static_cast<unsigned int>(hmap.size())));
            hcnts.push_back(1);
            hvals.push_back(val);
        } else {
            id = itr->second;
            hcnts[id] += 1;
        }
    }
    intens_recs.clear();
    intens_recs.reserve(hcnts.size());
    for (unsigned int ii=0; ii<hcnts.size(); ii++){
        intens_recs.push_back(CntRecord(hcnts[ii], hvals[ii], ii));
    }
    sort(intens_recs.begin(), intens_recs.end(), sort_by_count);
}

unsigned char* PicoLocalCompress::createLocalDictionary(unsigned char* compressed, const unsigned int mz_len1, const double* data)
{
    unsigned char* ptr = compressed;
    // ---- generate mz rlc table ------
    *(unsigned int*)ptr = mz_len1; ptr+=4; //.putInt(mzs.length);
    for (int ii=0; ii<4; ii++){
        *(double*)ptr = data[ii]; ptr+=8; // bidm.putDouble(best[ii]);
    }
    // ---- generate hop table ------
    *(unsigned short*)ptr = (short)hop_recs.size(); ptr+=2; //bidm.putShort((short)hop_recs.size()); //65535
    for (unsigned int ii=0; ii<hop_recs.size(); ii++){
        CntRecord cr = hop_recs[ii];
        *(unsigned short*)ptr = (short)cr.idx; ptr+=2; //bidm.putShort((short)(int)cr.idx); bidm.putShort((short)cr.value); 
#ifdef TEST 
        if (cr.value>65535)
            ptr+=0;
#endif
        *(unsigned short*)ptr = (short)cr.value; ptr+=2;
    }                
    // ---- generate intensity table ------
    *(unsigned char*)ptr = (unsigned char)LSB_FACTOR; ptr++; 
    *(unsigned int*)ptr = static_cast<unsigned int>(intens_recs.size()); ptr+=4; //bidm.putInt(intens_recs.size()); 
    for (unsigned int ii=0; ii<intens_recs.size(); ii++){
        CntRecord cr = intens_recs[ii];
        *(unsigned short*)ptr = (short)cr.idx; ptr+=2; //bidm.putShort((short)(int)cr.idx); bidm.putShort((short)(int)(cr.idx/4)); //bidm.putInt(cr.value); 
#ifdef TEST 
        if (cr.value>65535)
            ptr+=0;            // 69266
#endif
        int val = cr.value;
        if (val<=32767){
            *(unsigned short*)ptr = (unsigned short)(val|0x8000); ptr+=2; 
        } else {
            *(unsigned short*)ptr = (unsigned short)(val>>8); ptr+=2; 
            *(unsigned char*)ptr = (unsigned char)(val&0xFF); ptr++; 
        }
        //*(unsigned short*)ptr = (short)cr.value; ptr+=2; //(short)cr.idx/4; ptr+=2; //?? <=============== ??
    }
    hop_recs.clear(); intens_recs.clear();
    std::vector<CntRecord>().swap(hop_recs);
    std::vector<CntRecord>().swap(intens_recs);
    return ptr;
}

unsigned char* PicoLocalCompress::createLocalDictionaryType02(unsigned char* compressed, const unsigned int mz_len1, const double* data)
{
    unsigned char* ptr = compressed;
    // ---- generate mz rlc table ------
    *(unsigned int*)ptr = mz_len1; ptr+=4; //.putInt(mzs.length);
    for (int ii=0; ii<4; ii++){
        *(double*)ptr = data[ii]; ptr+=8; // bidm.putDouble(best[ii]);
    }
                
    // ---- generate intensity table ------
    *(unsigned char*)ptr = (unsigned char)LSB_FACTOR; ptr++; 
    *(unsigned int*)ptr = (unsigned int)intens_recs.size(); ptr+=4; 
    for (unsigned int ii=0; ii<intens_recs.size(); ii++){
#ifdef TEST 
        if (intens_recs[ii].value>65535)
            ptr+=0;            // 69266
#endif
        int val = intens_recs[ii].value;
        if (val<=32767){
            *(unsigned short*)ptr = (unsigned short)(val|0x8000); ptr+=2; 
        } else {
            *(unsigned short*)ptr = (unsigned short)(val>>8); ptr+=2; 
            *(unsigned char*)ptr = (unsigned char)(val&0xFF); ptr++; 
        }
    }
    return ptr;
}


unsigned char* PicoLocalCompress::generateIndices(const std::vector<unsigned int> &intensity, unsigned char* ptr2)
{
    // ---- generate indices table ------
    unsigned char* ptr = ptr2;
    *(unsigned int*)ptr = static_cast<unsigned int>(hop_dels.size()); ptr+=4; //bidm.putInt(intens3.size()); int bcnt=0;
    for (unsigned int ii=0; ii<hop_dels.size(); ii++){
        unsigned int val = hop_map[hop_dels[ii]]; //int val = hop_hmap.get(del3[ii]);
#ifdef TEST 
        if (val>65535)
            ptr+=0;            // 
#endif
        if (val<127){
            *(unsigned char*)ptr = (unsigned char)(val|0x80); ptr++; //bidm.put((byte)(val|0x80)); bcnt++;
        } else {
            *(unsigned char*)ptr = (unsigned char)((val>>8)&0xFF); ptr++; //msb
            *(unsigned char*)ptr = (unsigned char)(val&0xFF); ptr++; //lsb
            //*(unsigned short*)ptr = (unsigned short)((short)val); ptr+=2; //bidm.putShort((short)val); bcnt+=2;
        }
        int val1 = hmap[intensity[ii]]; //hmap[hvals[ii]]; 
#ifdef TEST 
        if (val1>65535)
            ptr+=0;            // 
#endif
        if (val1<127){
            *(unsigned char*)ptr = (unsigned char)(val1|0x80); ptr++; //bidm.put((byte)(val1|0x80)); bcnt++;
        } else {
            *(unsigned char*)ptr = (unsigned char)((val1>>8)&0xFF); ptr++; //msb
            *(unsigned char*)ptr = (unsigned char)(val1&0xFF); ptr++; //lsb
            //*(unsigned short*)ptr = (unsigned short)((short)val1); ptr+=2; //bidm.putShort((short)val1); bcnt+=2;
        }
    }
    return ptr;
}

// compress type 02
bool PicoLocalCompress::compressRawType02(const double* mz_blob, const float* intens_blob, int mz_len1, unsigned char** compressed, unsigned int* compressed_length, unsigned long long* intensity_sum)
{
    //int scan = 0;
    unsigned long long intensity_sum_i = 0;
    double* vect = (double *)calloc(11, sizeof(double));
    if (!vect){
        cerr << "vect allocation failed" << endl; return false;
    }
    double* sx = &vect[0], *sy = &vect[7]; sx[0] = mz_len1;

    auto start = chrono::system_clock::now();
    if (mz_blob[0]<min_mz) min_mz=mz_blob[0];
    for (int ii=0; ii<mz_len1; ii++){
        double yi = mz_blob[ii], xpi = 1; sy[0] += yi;
        for (int jj=1; jj<7; jj++){
            xpi *= ii; sx[jj] += xpi; 
            if (jj<4) sy[jj] += xpi*yi;
        }
    }
    if (mz_blob[mz_len1-1]>max_mz) max_mz=mz_blob[mz_len1-1]; 


    const size_t size = 3+32+3*4+intens_recs.size()*4+mz_len1*8; // worst case
    *compressed = (unsigned char*)malloc(size*sizeof(unsigned char)); 
    if (*compressed==NULL){
        cout << "compressed buf allocation failed" << endl; free_safe(vect); return false;
    }

    std::vector<unsigned int> intensity, intensity_index; int err_cnt = 0;
    for (int ii=0; ii<mz_len1; ii++){
        float yi = intens_blob[ii];
        if (yi>0.0){
            unsigned int val = (unsigned int)yi;
            if (val>0xFFFFFF){
                if (err_cnt>8){
                    cout << "incoming intensity value=" << val << " exceeded max=16,777,215 at ii=" << ii << endl; err_cnt++;
                }
            }
            val /= LSB_FACTOR; if (val>max_intensity) max_intensity = val; intensity_sum_i += val;
            intensity.push_back(val); intensity_index.push_back(ii); 
        }
    }
    *intensity_sum = intensity_sum_i;

    createSpectraHash(intensity);

    Predictor* pred = new Predictor(3);
    pred->initialize(vect);
    double* data = pred->predict();
    if (!data){
        cout << "null predict data" << endl; delete pred; free_safe(vect); return false;
    }
    unsigned char* ptr = createLocalDictionaryType02(*compressed, mz_len1, data); 
    free_safe(data); free_safe(vect); delete pred;

    //unsigned char* ptr3 = ptr;   //std::vector<unsigned char> words; // <======== for debug
    //std::vector<std::vector<unsigned int>> array(intens_recs.size()); // <======== for debug
    std::vector<unsigned int> indices;
    //int sum = 0;
    bool hflag = false; unsigned char hold = 0;
    for (unsigned int ii=0; ii<intens_recs.size(); ii++){                    
        unsigned int vali = intens_recs[ii].value, cur = 0; indices.clear();
        for (unsigned int jj=0; jj<intensity.size(); jj++){
            unsigned int valj = intensity[jj]; 
            if (valj==vali){
                unsigned int posj = intensity_index[jj]; //array[ii].push_back(posj);
                indices.push_back(posj - cur-1);
                cur = posj;
            }
        }

        for (unsigned int jj=0; jj<indices.size(); jj++){
            cur = indices[jj];
            if (ii==0x10 && jj>=0x2)
                jj+=0;
            if (cur<7){
                if (!hflag){
                    hold = cur | 0x8; hflag = true;
                } else {
                    *ptr++ = (hold<<4) | cur | 0x8; hflag = false; //words.push_back((hold<<4) | cur | 0x8);
                }
            } else if (cur<71){
                cur -= 7;
                if (!hflag){
                    *ptr++ = cur | 0x40;  //words.push_back(cur | 0x40);
                } else {
                    *ptr++ = (hold<<4) | (cur>>4) | 0x4;  //words.push_back((hold<<4) | (cur>>4) | 0x4);
                    hold = cur & 0x0F; 
                }
            } else if (cur<583){ // 1.5
                cur -= 71;
                if (!hflag){
                    *ptr++ = (cur>>4) | 0x20; //words.push_back((cur>>4) | 0x20);
                    hold = cur & 0xF; hflag = true;
                } else {
                    *ptr++ = (hold<<4) | (cur>>8) | 0x2; //words.push_back((hold<<4) | (cur>>8) | 0x2);
                    *ptr++ = cur & 0xFF; hflag = false; //words.push_back(cur & 0xFF);
                }
            } else if (cur<4679){
                cur -= 583;
                if (!hflag){
                    *ptr++ = (cur>>8) | 0x10; //words.push_back((cur>>8) | 0x10);
                    *ptr++ = cur & 0xFF; //words.push_back(cur & 0xFF);
                } else {
                    *ptr++ = (hold<<4) | (cur>>12) | 0x1; //words.push_back((hold<<4) | (cur>>12) | 0x1);
                    *ptr++ = (cur>>4) & 0xFF; //words.push_back((cur>>4) & 0xFF);
                    hold = cur & 0x0F; 
                }
            } else if (cur<37446){ // 2.5
                cur -= 4679;
                if (!hflag){
                    *ptr++ = (cur>>12) | 0x8; //words.push_back((cur>>12) | 0x8);
                    *ptr++ = (cur>>4) & 0xFF; //words.push_back((cur>>4) & 0xFF);
                    hold = cur & 0xF; hflag = true;
                } else {
                    *ptr++ = (hold<<4); //words.push_back(hold<<4); 
                    *ptr++ = (cur>>8) | 0x80; //words.push_back((cur>>8) | 0x40);
                    *ptr++ = cur & 0xFF; hflag = false; //words.push_back(cur & 0xFF); 
                }
            } else {
                cur -= 37446;
                if (!hflag){
                    *ptr++ = (cur>>16); //words.push_back(cur>>16);
                    *ptr++ = (cur>>8) & 0xFF; //words.push_back((cur>>8) & 0xFF);
                    *ptr++ = cur & 0xFF; //words.push_back(cur & 0xFF);
                } else {
                    *ptr++ = (hold<<4); //words.push_back(hold<<4);
                    *ptr++ = (cur>>12); //words.push_back(cur>>12);
                    *ptr++ = (cur>>4) & 0xFF;  //words.push_back((cur>>4) & 0xFF);
                    hold = cur & 0xF; 
                }
            }
        }

        if (!hflag){
            hold = 0xF; hflag = true;
        } else {
            *ptr++ = (hold<<4) | 0x0F; hflag = false;
        }
        ii+=0;
    }
    if (hflag){
        *ptr++ = (hold<<4); hflag = false;
    } 
    *compressed_length = static_cast<unsigned int>(ptr - *compressed);
    //sum/=2;

/*
    // --------- restore dict -----------
    //unsigned char *ptx = *compressed;
    ptr = *compressed; 
    unsigned int restored_mz_size = *(unsigned int*)ptr; ptr+=4; //*length = restored_mz_size; 
    double restored_b0 = *(double*)ptr; ptr+=8; 
    double restored_b1 = *(double*)ptr; ptr+=8; 
    double restored_b2 = *(double*)ptr; ptr+=8; 
    double restored_b3 = *(double*)ptr; ptr+=8; 
        
    // ------ restore intensity table -------
    unsigned char LSB_FACTOR = *(unsigned char*)ptr; ptr++; 
    unsigned int intens_tbl_size = *(unsigned int*)ptr; ptr+=4; 
    std::vector<unsigned int> intes_vals;
//    map<unsigned int, unsigned int> hmap_intens; 
    for (unsigned int ii=0; ii<intens_tbl_size; ii++){
        //unsigned int val = (unsigned int)*(unsigned short*)ptr; ptr+=2; 
        unsigned int val1 = (unsigned int)*(unsigned short*)ptr; ptr+=2; 
        if (val1>32767){
            val1 -= 32768;
        } else {
            unsigned char lsb = *(unsigned char*)ptr; ptr++; 
            val1 = val1<<8 | lsb;
        }
        intes_vals.push_back(val1);
        //hmap_intens.insert(std::pair<unsigned int, unsigned int>(val, val1*LSB_FACTOR));
        //hmap_intens.insert(std::pair<unsigned int, unsigned int>(ii, val1));
    }


    // ------------ decompress indices --------------
    std::vector<unsigned int> base(mz_len1);
    unsigned char *ptd = ptr3;
    hflag = false; hold = 0;
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
            if ((int)cur>=mz_len1){
                cout << "cur index exceeded m/z range" << endl; // return false;
            }
            base[cur] = vali; 
            
            unsigned int posc = array[ii][cnt]; 
            if (cur != posc){
                cout << "cur=" << cur << ", pos=" << posc << endl;
            }
            cnt++;
        }
        ii+=0;
    }

    // --------- restore intensity --------------
    std::vector<unsigned int> intens(mz_len1);
    for (unsigned int ii=0; ii<mz_len1; ii++){
        unsigned int idxi = base[ii];
        if (idxi>0){
            unsigned int vali = idxi*LSB_FACTOR; //hmap_intens[idxi]*LSB_FACTOR; 
            intens[ii] = vali;
            if (vali!= (int)(intens_blob[ii])){
                cout << "dif at ii=" << ii << ", val=" << vali << ", orig=" << intens_blob[ii] << endl;
                ii+=0;
            }
        }
    }

    //unsigned char* ptr2 = generateIndices(intensity, ptr);
    auto duration = chrono::system_clock::now() - start;
    auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
*/
    //*compressed_length = ptr3 - *compressed; 
    return true;
}


// compress raw type centroided 1
bool PicoLocalCompress::compressRawTypeCentroided1(const double* mz_blob, const float* intens_blob, int mz_len1, unsigned char** compressed_mz, unsigned int* compressed_mz_length,
        unsigned char** compressed_intensity, unsigned int* compressed_intensity_length, unsigned long long* intensity_sum, bool peak_sorting)
{
    // some vendor centroiding dlls are buggy -- occasionally returning closly spaced centroided
    // peaks in non-monotone m/z order, hence it is necessary to sort centroided peaks in mz order.
    // the sorting should be quick, if the input is in nearly sorted order.
    // consider bypassing the sorting for good vendor's dll, to avoid copying the data
    std::vector<PeakF> fpeaks; fpeaks.reserve(mz_len1);
    float min_intensity = 9999999, sum_intens = 0; max_intensity = 0;  int imax = 0;
    for (int ii=0; ii<mz_len1; ii++){
        float yfi = intens_blob[ii]; 
        sum_intens += yfi;
        if (yfi > max_intensity) {max_intensity = (long long)yfi; imax=ii;} if (yfi < min_intensity) min_intensity = yfi; 
        fpeaks.push_back(PeakF(mz_blob[ii], yfi));
    }
    *intensity_sum = (unsigned long long)sum_intens;
    if (peak_sorting)
        std::sort(fpeaks.begin(), fpeaks.end(), fpeak_sort_by_mz); 

    bool no_compression = false; double mz0 = 0, delta_x = 1;
    if (mz_len1 < MIN_CENTROIDED_PEAKS){
        no_compression = true; 
    } else {
        mz0 = fpeaks[0].mz;
        delta_x = fpeaks[mz_len1-1].mz - mz0;
        if (delta_x < MIN_CENTROIDED_PEAK_MZ_SPAN)
            no_compression = true; 
    }

    if (no_compression){
        int size = 4+mz_len1*8; // worst case
        *compressed_mz = (unsigned char*)malloc(size*sizeof(unsigned char)); 
        if (*compressed_mz==NULL){
            if (verbose) std::cout << "compressed mz buf allocation failed" << endl; return false;
        }
        unsigned char* ptr = *compressed_mz;
        *(unsigned int *)ptr = mz_len1 | 0x80000000; ptr+=4;
        for (int ii=0; ii<mz_len1; ii++){
            *(double *)ptr = fpeaks[ii].mz; ptr+=8;
        }
        *compressed_mz_length = static_cast<unsigned int>(ptr - *compressed_mz);

        size = 4+mz_len1*4; // worst case
        *compressed_intensity = (unsigned char*)malloc(size*sizeof(unsigned char)); 
        if (*compressed_intensity==NULL){
            if (verbose) std::cout << "compressed mz buf allocation failed" << endl; return false;
        }
        ptr = *compressed_intensity;
        *(unsigned int *)ptr = mz_len1 | 0x80000000; ptr+=4;
        for (int ii=0; ii<mz_len1; ii++){
            *(float *)ptr = fpeaks[ii].intensity; ptr+=4;
        }
        *compressed_intensity_length = static_cast<unsigned int>(ptr - *compressed_intensity);
        return true;
    }

    float s_factor = 1.0f;
    //int MAX_INTENSITY = 1000000;
    if (max_intensity<1000.0f){
        s_factor = 1000.0f;
    }

    std::vector<Peak> peaks; peaks.reserve(mz_len1);
    unsigned int min_intens = 9999999; 
    for (int ii=0; ii<mz_len1; ii++){
        float yfi = fpeaks[ii].intensity; unsigned int yi = 0;
        if (s_factor!=1.0F)
            yfi *= s_factor;
        if (yfi>0){
            yi = (unsigned int)(yfi+0.5F); 
            if (yfi<=0.5F)
                yi = 1;
        }        
        if (yi < min_intens) min_intens = yi; 
        peaks.push_back(Peak(mz_blob[ii], yi));
    }
    fpeaks.clear();

    delta_x /= 1E8;

    Predictor* pred = new Predictor(3);    
    //unsigned long long intensity_sum_i = 0;
    double* vect = (double *)calloc(11, sizeof(double));
    double* sx = &vect[0], *sy = &vect[7]; sx[0] = mz_len1; 
    for (int ii=0; ii<mz_len1; ii++){
        double yi = mz_blob[ii], xpi = 1; sy[0] += yi;
        int kk = (int)((yi-mz0)/delta_x);
        for (int jj=1; jj<7; jj++){
            xpi *= kk; sx[jj] += xpi; 
            if (jj<4) sy[jj] += xpi*yi;
        }
    }
    pred->initialize(vect);
    double* data = pred->predict();
    double a = data[3], b = data[2], c = data[1], d = data[0];
    free_safe(data); free_safe(vect); delete pred;

    double max_err = 0; unsigned int kprv = 0, min_kdel = 9999999, max_kdel = 0; int avg = 0, cntb=0;
    std::vector<unsigned int> mszi, mzsk; //mszi.push_back(0);
    mszi.reserve(mz_len1);
    for (int ii=1; ii<mz_len1; ii++){
        double xi = peaks[ii].mz; int k = (int)((xi-mz0)/delta_x), k2 = k*k;
        double vi = a*k*k2+b*k2+c*k+d;
        double err = abs(xi - vi); if (err>max_err) max_err=err;
        unsigned int kdel = k-kprv-1; if(kdel>65536) {mzsk.push_back(kdel-65536); cntb++;}
        mszi.push_back(kdel); avg += kdel;
        if (kdel>max_kdel) max_kdel = kdel; if (kdel<min_kdel) min_kdel = kdel; 
        kprv = k;
    } avg /= (mz_len1-1);


/*    std::vector<int> mszd; 
    map<unsigned int, unsigned int> hxmap; std::vector<unsigned int> hxcnts; std::vector<int> hxvals;
    for (int ii=0; ii<mz_len1-1; ii++){
        int difi = (int)mszi[ii]-min_kdel;
        mszd.push_back(difi); 
        unsigned int id;
        map<unsigned int, unsigned int>::iterator itr = hxmap.find(difi);
        if (itr==hxmap.end()){
            id = hxmap.size(); hxmap.insert(std::pair<unsigned int, unsigned int>(difi, hxmap.size())); hxcnts.push_back(1); hxvals.push_back(difi);
        } else {
            id = itr->second; hxcnts[id] += 1;
        }

    } 

    std::vector<CntRecord> hx_recs;
    for (unsigned int ii=0; ii<hxcnts.size(); ii++)
        hx_recs.push_back(CntRecord(hxcnts[ii], hxvals[ii], ii)); //min_kdel
    sort(hx_recs.begin(), hx_recs.end(), sort_by_count); */

    std::vector<unsigned int> mzsx;
    mzsx.reserve(mz_len1);
    mzsx.push_back(0);
    //int cnt0 = 0, cnt1=0, cnt2=0, cnt3=0;
    for (int ii=0 ; ii<mz_len1-1; ii++){
        unsigned int vali = mszi[ii]-min_kdel; mzsx.push_back(vali+min_kdel+1);
    }

    size_t size = 40 + mz_len1 * 8; // worst case
    *compressed_mz = (unsigned char*)malloc(size*sizeof(unsigned char)); 
    if (*compressed_mz==NULL){
        if (verbose) std::cout << "compressed mz buf allocation failed" << endl; return false;
    }

    // -- no dict, just length ---
    unsigned char* ptr = *compressed_mz;
    *(unsigned int *)ptr = mz_len1; ptr+=4;
    *(double *)ptr = a; ptr+=8;
    *(double *)ptr = b; ptr+=8;
    *(double *)ptr = c; ptr+=8;
    *(double *)ptr = d; ptr+=8;
    *(unsigned int *)ptr = (unsigned int)(min_kdel+1); ptr+=4;

    bool hflag = false; unsigned char hold = 0;
    for (int ii=0 ; ii<mz_len1-1; ii++){
        unsigned int cur = mszi[ii]-min_kdel;
//        if (ii==45)
//            ii+=0;
        if (cur<128){
            if (!hflag){
                *ptr++ = cur | 0x80;  
            } else {
                *ptr++ = (hold<<4) | (cur>>4) | 0x8;  
                hold = cur & 0x0F; 
            }
        } else if (cur<1152){ // 1.5
            cur -= 128;
            if (!hflag){
                *ptr++ = (cur>>4) | 0x40; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4) | (cur>>8) | 0x4; 
                *ptr++ = cur & 0xFF; hflag = false; 
            }
        } else if (cur<9344){ // 2
            cur -= 1152;
            if (!hflag){
                *ptr++ = (cur>>8) | 0x20; 
                *ptr++ = cur & 0xFF; 
            } else {
                *ptr++ = (hold<<4) | (cur>>12) | 0x2; 
                *ptr++ = (cur>>4) & 0xFF; 
                hold = cur & 0x0F; 
            }
        } else if (cur<74880) { // 2.5
            cur -= 9344;
            if (!hflag){
                *ptr++ = (cur>>12) | 0x10; 
                *ptr++ = (cur>>4) & 0xFF; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4) | 0x1; 
                *ptr++ = (cur>>8); 
                *ptr++ = cur & 0xFF; hflag = false; 
            }
        } else if (cur<599168) { // 3  
            cur -= 74880;
            if (!hflag){
                *ptr++ = (cur>>16) | 0x08; 
                *ptr++ = (cur>>8);
                *ptr++ = cur;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>12) | 0x80; 
                *ptr++ = (cur>>4);
                hold = cur & 0xF; 
            }
        } else if (cur<4793472) { // 3.5
            cur -= 599168;
            if (!hflag){
                *ptr++ = (cur>>20) | 0x04; 
                *ptr++ = (cur>>12);
                *ptr++ = (cur>>4);
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>16)| 0x40;
                *ptr++ = (cur>>8); 
                *ptr++ = cur; hflag = false;
            }
        } else if (cur<38347904) { // 4
            cur -= 4793472;
            if (!hflag){
                *ptr++ = (cur>>24) | 0x02; 
                *ptr++ = (cur>>16); 
                *ptr++ = (cur>>8);
                *ptr++ = cur; 
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>20) | 0x20; 
                *ptr++ = (cur>>12);
                *ptr++ = (cur>>4);
                hold = cur & 0xF; 
            }
        } else if (cur<306783360) { // 4.5
            cur -= 38347904;
            if (!hflag){
                *ptr++ = 0x01; 
                *ptr++ = (cur>>20);
                *ptr++ = (cur>>12); 
                *ptr++ = (cur>>4);
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>24) | 0x10; 
                *ptr++ = (cur>>16); 
                *ptr++ = (cur>>8); 
                *ptr++ = cur; hflag = false;
            }
        } else { // 5
            cur -= 306783360;
            if (!hflag){
                *ptr++ = 0x0; 
                *ptr++ = (cur>>24); 
                *ptr++ = (cur>>16); 
                *ptr++ = (cur>>8);
                *ptr++ = cur;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>28); 
                *ptr++ = (cur>>20); 
                *ptr++ = (cur>>12); 
                *ptr++ = (cur>>4); 
                hold = cur & 0xF; 
            }
        }
    }
    if (hflag){
        *ptr++ = (hold<<4); hflag = false;
    } 

    *compressed_mz_length = static_cast<unsigned int>(ptr - *compressed_mz);

    // ----------------- intensity ------------------

    hop_map.clear(); std::vector<unsigned int> hop_cnts, hop_vals; 
    for (int ii=0; ii<(int)mz_len1; ii++){
        unsigned int yi = peaks[ii].intensity, id;
        map<unsigned int, unsigned int>::iterator itr = hop_map.find(yi);
        if (itr == hop_map.end()) {
            id = static_cast<unsigned int>(hop_map.size());
            hop_map.insert(std::pair<unsigned int, unsigned int>(
                yi, static_cast<unsigned int>(hop_map.size())));
            hop_cnts.push_back(1);
            hop_vals.push_back(yi);
        } else {
            id = itr->second;
            hop_cnts[id] += 1;
        }
    }
    hop_recs.clear();
    hop_recs.reserve(hop_cnts.size());
    for (unsigned int ii=0; ii<hop_cnts.size(); ii++)
        hop_recs.push_back(CntRecord(hop_cnts[ii], hop_vals[ii], ii)); 
    std::sort(hop_recs.begin(), hop_recs.end(), sort_by_count); 

    unsigned int scale_fact = 1;
    for (int jj=0; jj<8; jj++){
        scale_fact *= 2;
        bool fail = false;
        for (int ii=0; ii<(int)hop_recs.size(); ii++){
            unsigned int yi = hop_recs[ii].value;
            if ((yi/scale_fact)*scale_fact != yi){
                fail=true; break;
            }
        }
        if (fail){
            scale_fact/=2; break;
        }
    }

    map<unsigned int, unsigned int> hm8;
    for (int ii=0; ii<(int)hop_recs.size(); ii++)
        hm8.insert(std::pair<unsigned int, unsigned int>(hop_recs[ii].value, static_cast<unsigned int>(hm8.size())));

    size = 3+4*4+hop_recs.size()*4+mz_len1*4; // worst case
    *compressed_intensity = (unsigned char*)malloc(size*sizeof(unsigned char)); 
    if (*compressed_intensity==NULL){
        if (verbose) std::cout << "compressed intensity buf allocation failed" << endl; return false;
    }

    // -- create dict ---
    ptr = *compressed_intensity;
    *(unsigned int *)ptr = mz_len1; ptr+=sizeof(unsigned int);
    *(float *)ptr = s_factor; ptr+=sizeof(float);
    *(unsigned int *)ptr = min_intens; ptr+=sizeof(unsigned int);
    *(unsigned short*)ptr = static_cast<unsigned short>(hop_recs.size()); ptr+=sizeof(unsigned short);
    *ptr++ = scale_fact; 

    hflag = false; hold = 0;
    for (int ii=0; ii<(int)hop_recs.size(); ii++){
        unsigned int cur = (hop_recs[ii].value - min_intens)/scale_fact;
//        if (ii==109)
//            ii+=0;
        if (cur<128){
            if (!hflag){
                *ptr++ = cur | 0x80;  
            } else {
                *ptr++ = (hold<<4) | (cur>>4) | 0x8;  
                hold = cur & 0x0F; 
            }
        } else if (cur<1152){ // 1.5
            cur -= 128;
            if (!hflag){
                *ptr++ = (cur>>4) | 0x40; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4) | (cur>>8) | 0x4; 
                *ptr++ = cur & 0xFF; hflag = false; 
            }
        } else if (cur<9344){ // 2
            cur -= 1152;
            if (!hflag){
                *ptr++ = (cur>>8) | 0x20; 
                *ptr++ = cur & 0xFF; 
            } else {
                *ptr++ = (hold<<4) | (cur>>12) | 0x2; 
                *ptr++ = (cur>>4) & 0xFF; 
                hold = cur & 0x0F; 
            }
        } else if (cur<74880) { // 2.5
            cur -= 9344;
            if (!hflag){
                *ptr++ = (cur>>12) | 0x10; 
                *ptr++ = (cur>>4) & 0xFF; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4) | 0x1; 
                *ptr++ = (cur>>8); 
                *ptr++ = cur & 0xFF; hflag = false; 
            }
        } else if (cur<599168) { // 3  
            cur -= 74880;
            if (!hflag){
                *ptr++ = (cur>>16) | 0x08; 
                *ptr++ = (cur>>8);// & 0xFF; 
                *ptr++ = cur;// & 0xFF; 
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>12) | 0x80; 
                *ptr++ = (cur>>4);// & 0xFF; 
                hold = cur & 0xF; 
            }
        } else if (cur<4793472) { // 3.5
            cur -= 599168;
            if (!hflag){
                *ptr++ = (cur>>20) | 0x04; 
                *ptr++ = (cur>>12);// & 0xFF; 
                *ptr++ = (cur>>4);// & 0xFF; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>16)| 0x40;
                *ptr++ = (cur>>8); 
                *ptr++ = cur; hflag = false; // & 0xFF
            }
        } else if (cur<38347904) { // 4
            cur -= 4793472;
            if (!hflag){
                *ptr++ = (cur>>24) | 0x02; 
                *ptr++ = (cur>>16);// & 0xFF;  
                *ptr++ = (cur>>8);// & 0xFF; 
                *ptr++ = cur;// & 0xFF; 
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>20) | 0x20; 
                *ptr++ = (cur>>12);// & 0xFF; 
                *ptr++ = (cur>>4);// & 0xFF; 
                hold = cur & 0xF; 
            }
        } else if (cur<306783360) { // 4.5
            cur -= 38347904;
            if (!hflag){
                *ptr++ = 0x01; 
                *ptr++ = (cur>>20);// & 0xFF; 
                *ptr++ = (cur>>12);// & 0xFF; 
                *ptr++ = (cur>>4);// & 0xFF; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>24) | 0x10; 
                *ptr++ = (cur>>16); 
                *ptr++ = (cur>>8); 
                *ptr++ = cur; hflag = false; //& 0xFF
            }
        } else if (cur<2454267008) { // 5
            cur -= 306783360;
            if (!hflag){
                *ptr++ = 0x0; 
                *ptr++ = (cur>>24) | 0x80; 
                *ptr++ = (cur>>16);
                *ptr++ = (cur>>8);
                *ptr++ = cur;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>28) | 0x08; 
                *ptr++ = (cur>>20); 
                *ptr++ = (cur>>12); 
                *ptr++ = (cur>>4); 
                hold = cur & 0xF; 
            }
        } else { // 5.5
            cur -= 2454267008;
            if (!hflag){
                *ptr++ = 0x0; 
                *ptr++ = (cur>>28);
                *ptr++ = (cur>>20);
                *ptr++ = (cur>>12);
                *ptr++ = (cur>>4);
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4);
                *ptr++ = 0x0;
                *ptr++ = (cur>>24); 
                *ptr++ = (cur>>16); 
                *ptr++ = (cur>>8); 
                *ptr++ = cur; hflag = false;
            }
        }
    }
    if (hflag){
        *ptr++ = (hold<<4); hflag = false;
    } 
    //*compressed_intensity_length = ptr - *compressed_intensity;

    // ---- generate indices --------
    for (int ii=0; ii<(int)mz_len1; ii++){
        if (ii==109)
            ii+=0;
        unsigned int yi = peaks[ii].intensity;
        unsigned int cur = hm8[yi];
        if (cur<128){
            if (!hflag){
                *ptr++ = cur | 0x80;  
            } else {
                *ptr++ = (hold<<4) | (cur>>4) | 0x8;  
                hold = cur & 0x0F; 
            }
        } else if (cur<1152){ // 1.5
            cur -= 128;
            if (!hflag){
                *ptr++ = (cur>>4) | 0x40; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4) | (cur>>8) | 0x4; 
                *ptr++ = cur & 0xFF; hflag = false; 
            }
        } else if (cur<9344){ // 2
            cur -= 1152;
            if (!hflag){
                *ptr++ = (cur>>8) | 0x20; 
                *ptr++ = cur & 0xFF; 
            } else {
                *ptr++ = (hold<<4) | (cur>>12) | 0x2; 
                *ptr++ = (cur>>4) & 0xFF; 
                hold = cur & 0x0F; 
            }
        } else if (cur<74880) { // 2.5
            cur -= 9344;
            if (!hflag){
                *ptr++ = (cur>>12) | 0x10; 
                *ptr++ = (cur>>4) & 0xFF; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4) | 0x1; 
                *ptr++ = (cur>>8); 
                *ptr++ = cur & 0xFF; hflag = false; 
            }
        } else if (cur<599168) { // 3  
            cur -= 74880;
            if (!hflag){
                *ptr++ = (cur>>16) | 0x08; 
                *ptr++ = (cur>>8);
                *ptr++ = cur;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>12) | 0x80; 
                *ptr++ = (cur>>4);
                hold = cur & 0xF; 
            }
        } else if (cur<4793472) { // 3.5
            cur -= 599168;
            if (!hflag){
                *ptr++ = (cur>>20) | 0x04; 
                *ptr++ = (cur>>12);
                *ptr++ = (cur>>4);
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>16)| 0x40;
                *ptr++ = (cur>>8); 
                *ptr++ = cur; hflag = false;
            }
        } else if (cur<38347904) { // 4
            cur -= 4793472;
            if (!hflag){
                *ptr++ = (cur>>24) | 0x02; 
                *ptr++ = (cur>>16); 
                *ptr++ = (cur>>8);
                *ptr++ = cur; 
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>20) | 0x20; 
                *ptr++ = (cur>>12);
                *ptr++ = (cur>>4);
                hold = cur & 0xF; 
            }
        } else if (cur<306783360) { // 4.5
            cur -= 38347904;
            if (!hflag){
                *ptr++ = 0x01; 
                *ptr++ = (cur>>20);
                *ptr++ = (cur>>12); 
                *ptr++ = (cur>>4);
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>24) | 0x10; 
                *ptr++ = (cur>>16); 
                *ptr++ = (cur>>8); 
                *ptr++ = cur; hflag = false; //& 0xFF
            }
        } else if (cur<2454267008) { // 5
            cur -= 306783360;
            if (!hflag){
                *ptr++ = 0x0; 
                *ptr++ = (cur>>24) | 0x80; 
                *ptr++ = (cur>>16);  
                *ptr++ = (cur>>8); 
                *ptr++ = cur; 
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>28) | 0x08; 
                *ptr++ = (cur>>20); 
                *ptr++ = (cur>>12); 
                *ptr++ = (cur>>4); 
                hold = cur & 0xF; 
            }
        } else { // 5.5
            cur -= 2454267008;
            if (!hflag){
                *ptr++ = 0x0; 
                *ptr++ = (cur>>28);
                *ptr++ = (cur>>20); 
                *ptr++ = (cur>>12); 
                *ptr++ = (cur>>4); 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = 0x0; 
                *ptr++ = (cur>>24); 
                *ptr++ = (cur>>16); 
                *ptr++ = (cur>>8); 
                *ptr++ = cur; hflag = false; 
            }
        }
    }
    if (hflag){
        *ptr++ = (hold<<4); hflag = false;
    } 

    *compressed_intensity_length = static_cast<unsigned int>(ptr - *compressed_intensity);
    return true;
}

// compress mz type 1
bool PicoLocalCompress::compressMzType1(const double* mz_blob, int mz_len1, unsigned char* compressed, unsigned int* compressed_length)
{
    //unsigned long long max_long = 0;
    std::vector<double> diff4; diff4.push_back(0);
    map<double, unsigned int> hmpD; std::vector<unsigned int> hcntsD; std::vector<double> hvalsD;
    double prev = mz_blob[0];
    if (prev<min_mz) min_mz=prev;
    for (int ii=1; ii<mz_len1; ii++){
        double val = mz_blob[ii], dif = val-prev; diff4.push_back(dif);
        prev = val;

        map<double, unsigned int>::iterator itr = hmpD.find(dif); unsigned int id;
        if (itr == hmpD.end()) {
            id = static_cast<unsigned int>(hmpD.size());
            hmpD.insert(
                std::pair<double, unsigned int>(dif, static_cast<unsigned int>(hmpD.size())));
            hcntsD.push_back(1);
            hvalsD.push_back(dif);
        } else {
            id = itr->second;
            hcntsD[id] += 1;
        }
    }
    if (prev>max_mz) max_mz=prev;
    std::vector<CntRecordD> intens_recsD;
    for (unsigned int ii=0; ii<hcntsD.size(); ii++)
        intens_recsD.push_back(CntRecordD(hcntsD[ii], hvalsD[ii], ii)); 
    sort(intens_recsD.begin(), intens_recsD.end(), sort_by_count_D);

    map<double, unsigned short> hmp6; 
    for (unsigned int ii=0; ii<intens_recsD.size(); ii++)
        hmp6.insert(std::pair<double, unsigned short>(intens_recsD[ii].value, static_cast<unsigned short>(hmp6.size())));

    std::vector<unsigned short> indices;
    for (int ii=1; ii<mz_len1; ii++){
        double dif = diff4[ii];
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
    if (seq7.size()<2)
        seq7.push_back(mz_len1-1); 

    // ----------- compress scan -----------------
    //*compressed = (unsigned char*)malloc(mz_len*sizeof(double)); 
    //if (compressed == NULL){
    //    cout << "compressed m/z buf allocation failed" << endl; return false;
    //}

    // ----- dict ---------
    *(unsigned int*)(compressed+10) = mz_len1;
    *(double*)(compressed+14) = mz_blob[0];
    unsigned char* ptr = compressed+22;
    for (unsigned int ii=0; ii<intens_recsD.size(); ii++){
        unsigned short vali = (unsigned short)(intens_recsD[ii].value*32768); 
        *(unsigned short*)ptr = vali; ptr+=2; 
    }
    unsigned short dict_len = static_cast<unsigned short>(ptr - compressed - 22);
    *(unsigned short*)compressed = dict_len;

    // -------- base -------
    std::vector<unsigned int> base; unsigned int cur7 = 0; 
    for (unsigned int ii=0; ii<seq7.size()/2; ii++){
        unsigned int vali = seq7[2*ii], loci = seq7[2*ii+1]; 
        *ptr++ = vali; // set vali
        unsigned int cnt7 = loci - cur7; 
        *ptr++ = (cnt7>>8); // set loc (16 bits)
        *ptr++ = cnt7&0xFF; 
        for (unsigned int jj=0; jj<cnt7; jj++)
            base.push_back(vali); 
        cur7 = loci;
    }
    unsigned int base_len = static_cast<unsigned int>(ptr-compressed)-dict_len-22;
    *(unsigned short*)(compressed+2) = (unsigned short)base_len;

    // -------- indices --------
    std::vector<std::vector<unsigned int>> array(intens_recsD.size());
    for (unsigned int ii=0; ii<indices.size(); ii++){
        unsigned short idx = indices[ii], idx1 = base.size()>0?base[ii]:0;
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
                    *ptr++ = (deli | 0x80); // set loc (8 bits)
                } else if (deli<16383){
                    *ptr++ = (((deli>>8)&0xFF) | 0x40); // set loc (16 bits)
                    *ptr++ = deli&0xFF;
                } else {
                    *ptr++ = deli>>16;  // set loc (24 bits)
                    *ptr++ = (deli>>8)&0xFF;  
                    *ptr++ = deli&0xFF; 
                }
                cur7 = loci; 
            }
            *ptr++ = 255; 
        } //else
            //ii+=0;
    }
    unsigned int hfr_len = static_cast<unsigned int>(ptr-compressed)-base_len-dict_len-22;
    *(unsigned int*)(compressed+4) = hfr_len;
                
    // -------- lfr --------
    for (unsigned int ii=256; ii<intens_recsD.size(); ii++){ // series of skip n write val (1 byte per data)
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
    return true;
}

// compress type 1 intensity
bool PicoLocalCompress::compressIntensityType1(const float* intens_blob, int mz_len1, unsigned char** compressed, unsigned int* compressed_length, unsigned long long* intensity_sum)
{
    std::vector<unsigned int> intensity;
    intensity.clear(); unsigned int max_hop = 0; max_intensity = 0; int iprev = -1;
    //auto start = chrono::system_clock::now(); 
    for (int ii=0; ii<mz_len1; ii++){
        float yi = intens_blob[ii];
        unsigned int val = (unsigned int)yi; if (val>max_intensity) max_intensity = val; *intensity_sum += val;
        if (val>0){
            unsigned int del = ii-iprev-1; iprev = ii; if (del>max_hop) max_hop = del;
            if (del<4)
                intensity.push_back((unsigned int)(val*4+del)); 
            else {
                intensity.push_back((unsigned int)(del | 0x30000000));
                intensity.push_back((unsigned int)(val*4)); 
            }
        }
    }

    createCombinedSpectraHash(intensity); 
    //if (intens_recs.size()>max_cnt) max_cnt = intens_recs.size();

    const size_t size = 3+32+6*4+intens_recs.size()*8+intensity.size()*12; // not hop_recs, not hvals
    *compressed = (unsigned char*)malloc(size*sizeof(unsigned char)); 
    if (*compressed==0){
        cout << "compressed intensity buf allocation failed" << endl; return false;
    }

    //auto start = chrono::system_clock::now();
    unsigned char* ptr1 = *compressed;
    unsigned char* ptr = createCombinedDictionary(ptr1, mz_len1);
    //int length1 = ptr - compressed; cout << length1 << endl; //<====== temp

    unsigned char* ptr2 = generateCombinedIndices(intensity, ptr); 
    //auto duration = chrono::system_clock::now() - start; duration += duration1;
    //auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
    *compressed_length = static_cast<unsigned int>(ptr2-*compressed);
    return true;
}

// compress type 1 intensity unsigned integer input
bool PicoLocalCompress::compressIntensityType1_I(const unsigned int* intens_blob, int mz_len1, unsigned char** compressed, unsigned int* compressed_length, unsigned long long* intensity_sum)
{
    std::vector<unsigned int> intensity;
    intensity.clear(); unsigned int max_hop = 0; max_intensity = 0; int iprev = -1;
    for (int ii=0; ii<mz_len1; ii++){
        unsigned int val = intens_blob[ii]; if (val>max_intensity) max_intensity = val; *intensity_sum += val;
        if (val>0){
            unsigned int del = ii-iprev-1; iprev = ii; if (del>max_hop) max_hop = del;
            if (del<4)
                intensity.push_back((unsigned int)(val*4+del)); 
            else {
                intensity.push_back((unsigned int)(del | 0x30000000));
                intensity.push_back((unsigned int)(val*4)); 
            }
        }
    }
    createCombinedSpectraHash(intensity); 

    const size_t size = 3+32+6*4+intens_recs.size()*8+intensity.size()*12; // not hop_recs, not hvals
    *compressed = (unsigned char*)malloc(size*sizeof(unsigned char)); 
    if (*compressed==0){
        cout << "compressed intensity buf allocation failed" << endl; return false;
    }

    unsigned char* ptr1 = *compressed;
    unsigned char* ptr = createCombinedDictionary(ptr1, mz_len1);
    //int length1 = ptr - compressed; cout << length1 << endl; //<====== temp
    unsigned char* ptr2 = generateCombinedIndices(intensity, ptr); 
    *compressed_length = static_cast<unsigned int>(ptr2-*compressed);
    return true;
}

unsigned char* PicoLocalCompress::createCombinedDictionary(unsigned char* compressed, const unsigned int mz_len1) //, const double* data)
{
    unsigned char* ptr = compressed;
    // ---- generate mz rlc table ------
    *(unsigned int*)ptr = mz_len1; ptr+=4; //.putInt(mzs.length);
    //for (int ii=0; ii<4; ii++){
    //    *(double*)ptr = data[ii]; ptr+=8; // bidm.putDouble(best[ii]);
    //}
                
    // ---- generate intensity table ------
    //unsigned char* ptd = ptr;  // for decompression

    unsigned int idx = 0;
    for ( ; idx<hvals.size(); idx++)
        if (hvals[idx]>4095) break;                        
    unsigned int idx1 = idx;
    for ( ; idx1<hvals.size(); idx1++)
        if (hvals[idx1]>65535) break;
    unsigned int idx2 = idx1;
    for ( ; idx2<hvals.size(); idx2++)
        if (hvals[idx2]>1048575) break;
    //cout << "idx=" << idx << ", idx1=" << idx1 << ", idx2=" << idx2 <<  ", total=" << (idx*3+(idx1-idx)*4+(idx2-idx1)*5)/2 << endl;

    *(unsigned int*)ptr = idx; ptr+=4;
    *(unsigned int*)ptr = idx1-idx; ptr+=4;
    *(unsigned int*)ptr = idx2-idx1; ptr+=4;
    *(unsigned int*)ptr = static_cast<unsigned int>(hvals.size()); ptr+=4; //bidm.putInt(intens_recs.size()); 
    unsigned char hold = 0; bool hflag = false;
    for (unsigned int ii=0; ii<idx; ii++){
        unsigned int val = hvals[ii]; //cout << "--> in=" << hex << val << endl;
        if (!hflag) {
            unsigned int val1 = val>>4;
            *(unsigned char*)ptr = (unsigned char)val1; ptr++; //cout << ii << " - " << hex << val1 << ", h=" << hex << hold << endl;
            hold = (unsigned char)(val&0x0F); hflag = true;
        } else {
            unsigned int val1 = (val>>8) | (((unsigned long long)hold)<<4);
            *(unsigned char*)ptr = (unsigned char)val1; ptr++; //cout << ii << " a- " << hex << val1 << endl;
            *(unsigned char*)ptr = (unsigned char)(val&0xFF); ptr++; //cout << ii << " b- " << hex << val << endl;
            hflag = false;
        }
    }
    if (hflag){
        *(unsigned char*)ptr = (unsigned char)(hold<<4); ptr++; hflag = false;
    }

    for (unsigned int ii=idx; ii<idx1; ii++){
        unsigned int val = hvals[ii];
        *(unsigned int*)ptr = (unsigned int)val; ptr+=4; //cout << hex << val << endl;
    }

    for (unsigned int ii=idx1; ii<idx2; ii++){
        unsigned int val = hvals[ii]; //cout << "--> in=" << hex << val << endl;
        if (!hflag) {
            *(unsigned int*)ptr = (unsigned int)(val>>4); ptr+=4;  
            hold = (unsigned char)(val&0x0F); hflag = true; //cout << ii << " - " << hex << val/16 << ", h=" << hex << hold << endl;
        } else {
            unsigned int val1 = (hold<<4) | (val>>16)&0x0F;
            *(unsigned char*)ptr = (unsigned char)val1; ptr++; //cout << ii << " a- " << hex << val1 << endl;
            *(unsigned int*)ptr = (unsigned int)(val&0xFFFF); ptr+=4; //cout << ii << " b- " << hex << (val&0xFFFF) << endl;
            hflag = false;
        }
    }
    if (hflag){
        *(unsigned char*)ptr = (unsigned char)(hold<<4); ptr++; hflag = false;
    }

    for (unsigned int ii=idx2; ii<hvals.size(); ii++){
        unsigned int val = hvals[ii];  //cout << "--> in=" << hex << val << endl;
        *(unsigned short*)ptr = (unsigned short)(val>>16); ptr+=2; //cout << ii << " a- " << hex << (val>>16) << endl;
        *(unsigned int*)ptr = (unsigned int)(val&0xFFFF); ptr+=4;  //cout << ii << " b- " << hex << (val&0xFFFF) << endl;
    }




/*
    unsigned int idx = 0;
    for ( ; idx<intens_recs.size(); idx++)
        if (intens_recs[idx].value>4095) break;                        
    unsigned int idx1 = idx;
    for ( ; idx1<intens_recs.size(); idx1++)
        if (intens_recs[idx1].value>65535) break;
    unsigned int idx2 = idx1;
    for ( ; idx2<intens_recs.size(); idx2++)
        if (intens_recs[idx2].value>1048575) break;
    cout << "idx=" << idx << ", idx1=" << idx1 << ", idx2=" << idx2 <<  ", total=" << (idx*3+(idx1-idx)*4+(idx2-idx1)*5)/2 << endl;

    *(unsigned int*)ptr = idx; ptr+=4;
    *(unsigned int*)ptr = idx1-idx; ptr+=4;
    *(unsigned int*)ptr = idx2-idx1; ptr+=4;
    *(unsigned int*)ptr = intens_recs.size(); ptr+=4; //bidm.putInt(intens_recs.size()); 
    unsigned char hold = 0; bool hflag = false;
    for (unsigned int ii=0; ii<idx; ii++){
        unsigned int val = intens_recs[ii].value; //cout << "--> in=" << hex << val << endl;
        if (!hflag) {
            unsigned int val1 = val>>4;
            *(unsigned char*)ptr = (unsigned char)val1; ptr++; //cout << ii << " - " << hex << val1 << ", h=" << hex << hold << endl;
            hold = (unsigned char)(val&0x0F); hflag = true;
        } else {
            unsigned int val1 = (val>>8) | (((unsigned long long)hold)<<4);
            *(unsigned char*)ptr = (unsigned char)val1; ptr++; //cout << ii << " a- " << hex << val1 << endl;
            *(unsigned char*)ptr = (unsigned char)(val&0xFF); ptr++; //cout << ii << " b- " << hex << val << endl;
            hflag = false;
        }
    }
    if (hflag){
        *(unsigned char*)ptr = (unsigned char)(hold<<4); ptr++; hflag = false;
    }

    for (unsigned int ii=idx; ii<idx1; ii++){
        unsigned int val = intens_recs[ii].value;
        *(unsigned int*)ptr = (unsigned int)val; ptr+=4; //cout << hex << val << endl;
    }

    for (unsigned int ii=idx1; ii<idx2; ii++){
        unsigned int val = intens_recs[ii].value; //cout << "--> in=" << hex << val << endl;
        if (!hflag) {
            *(unsigned int*)ptr = (unsigned int)(val>>4); ptr+=4;  
            hold = (unsigned char)(val&0x0F); hflag = true; //cout << ii << " - " << hex << val/16 << ", h=" << hex << hold << endl;
        } else {
            unsigned int val1 = (hold<<4) | (val>>16)&0x0F;
            *(unsigned char*)ptr = (unsigned char)val1; ptr++; //cout << ii << " a- " << hex << val1 << endl;
            *(unsigned int*)ptr = (unsigned int)(val&0xFFFF); ptr+=4; //cout << ii << " b- " << hex << (val&0xFFFF) << endl;
            hflag = false;
        }
    }
    if (hflag){
        *(unsigned char*)ptr = (unsigned char)(hold<<4); ptr++; hflag = false;
    }

    for (unsigned int ii=idx2; ii<intens_recs.size(); ii++){
        unsigned int val = intens_recs[ii].value;  //cout << "--> in=" << hex << val << endl;
        *(unsigned short*)ptr = (unsigned short)(val>>16); ptr+=2; //cout << ii << " a- " << hex << (val>>16) << endl;
        *(unsigned int*)ptr = (unsigned int)(val&0xFFFF); ptr+=4;  //cout << ii << " b- " << hex << (val&0xFFFF) << endl;
    }


/*    for (unsigned int ii=0; ii<intens_recs.size(); ii++){
        //CntRecord cr = intens_recs[ii];
        //*(unsigned short*)ptr = (short)cr.idx; ptr+=2; //bidm.putShort((short)(int)cr.idx); bidm.putShort((short)(int)(cr.idx/4)); //bidm.putInt(cr.value); 
//#ifdef TEST 
        //if (cr.value>65535)
        //    ptr+=0;            // 69266
//#endif
        unsigned int val = intens_recs[ii].value;
        if (val<=32767){
            *(unsigned short*)ptr = (unsigned short)(val|0x8000); ptr+=2; 
        } else {
            *(unsigned short*)ptr = (unsigned short)(val>>8); ptr+=2; 
            *(unsigned char*)ptr = (unsigned char)(val&0xFF); ptr++; 
        }
        //*(unsigned short*)ptr = (short)cr.value; ptr+=2; //(short)cr.idx/4; ptr+=2; //?? <=============== ??
    }
/*    for (unsigned int ii=0; ii<idx; ii++){
        unsigned long long val = intens_recs_L[ii].value; //cout << "--> in=" << hex << val << endl;
        if (!hflag) {
            unsigned long long val1 = val>>4;
            *(unsigned char*)ptr = (unsigned char)val1; ptr++; //cout << ii << " - " << hex << val1 << ", h=" << hex << hold << endl;
            hold = (unsigned char)(val&0x0F); hflag = true;
        } else {
            unsigned long long val1 = (val>>8) | (((unsigned long long)hold)<<4);
            //unsigned long long val2 = val&0xFF; //cout << "    val2=" << hex << val2 << "    val1=" << hex << val1 << endl;
            *(unsigned char*)ptr = (unsigned char)val1; ptr++; //cout << ii << " a- " << hex << val1 << endl;
            *(unsigned char*)ptr = (unsigned char)(val&0xFF); ptr++; //cout << ii << " b- " << hex << val << endl;
            hflag = false;
        }
    }
    if (hflag){
        *(unsigned char*)ptr = (unsigned char)(hold<<4); ptr++; hflag = false;
    }

    for (unsigned int ii=idx; ii<idx1; ii++){
        unsigned long long val = intens_recs_L[ii].value;
        *(unsigned int*)ptr = (unsigned int)val; ptr+=4; 
    }

    for (unsigned int ii=idx1; ii<idx2; ii++){
        unsigned long long val = intens_recs_L[ii].value; //cout << "--> in=" << hex << val << endl;
        if (!hflag) {
            *(unsigned int*)ptr = (unsigned int)(val>>4); ptr+=4;  
            hold = (unsigned char)(val&0x0F); hflag = true; //cout << ii << " - " << hex << val/16 << ", h=" << hex << hold << endl;
        } else {
            unsigned int val1 = (hold<<4) | (val>>16)&0x0F;
            *(unsigned char*)ptr = (unsigned char)val1; ptr++; //cout << ii << " a- " << hex << val1 << endl;
            *(unsigned int*)ptr = (unsigned int)(val&0xFFFF); ptr+=4; //cout << ii << " b- " << hex << (val&0xFFFF) << endl;
            hflag = false;
        }
    }
    if (hflag){
        *(unsigned char*)ptr = (unsigned char)(hold<<4); ptr++; hflag = false;
    }

    for (unsigned int ii=idx2; ii<intens_recs_L.size(); ii++){
        unsigned long long val = intens_recs_L[ii].value;  //cout << "--> in=" << hex << val << endl;
        *(unsigned short*)ptr = (unsigned short)(val>>16); ptr+=2; //cout << ii << " a- " << hex << (val>>16) << endl;
        *(unsigned int*)ptr = (unsigned int)(val&0xFFFF); ptr+=4;  //cout << ii << " b- " << hex << (val&0xFFFF) << endl;
    }


    for (unsigned int ii=0; ii<intens_recs_L.size(); ii++){
        CntRecordL cr = intens_recs_L[ii];
        //*(unsigned short*)ptr = (short)cr.idx; ptr+=2; //bidm.putShort((short)(int)cr.idx); bidm.putShort((short)(int)(cr.idx/4)); //bidm.putInt(cr.value); 
#ifdef TEST 
        if (cr.value>65535)
            ptr+=0;            // 69266
#endif
        unsigned long long val = cr.value;
        if (val<=32767){
            *(unsigned short*)ptr = (unsigned short)(val|0x8000); ptr+=2; 
        } else {
            *(unsigned short*)ptr = (unsigned short)(val>>8); ptr+=2; 
            *(unsigned char*)ptr = (unsigned char)(val&0xFF); ptr++; 
        }
        //*(unsigned short*)ptr = (short)cr.value; ptr+=2; //(short)cr.idx/4; ptr+=2; //?? <=============== ??
    }*/

    // read combined intensity records
/*****    std::vector<unsigned int> values_L; 
    hflag = false; hold = 0;
    unsigned int idz = *(unsigned int*)ptd; ptd+=4;
    unsigned int idz1 = *(unsigned int*)ptd; ptd+=4;
    unsigned int idz2 = *(unsigned int*)ptd; ptd+=4;
    unsigned int total_cnt = *(unsigned int*)ptd; ptd+=4;

    for (unsigned int ii=0; ii<min(idz,total_cnt); ii++){
        if (!hflag){
            unsigned int val1 = *(unsigned char*)ptd; ptd++;
            unsigned int val = *(unsigned char*)ptd; ptd++;
            hold = (unsigned char)(val&0x0F); 
            val = (val1<<4) | (val>>4); values_L.push_back(val); hflag = true; 
        } else {
            unsigned int val1 = *(unsigned char*)ptd; ptd++;
            unsigned int val = val1 | (((unsigned long long)hold)<<8);
            values_L.push_back(val); hflag = false; 
        }
    }

    hold = 0; hflag = false;
    for (unsigned int ii=0; ii<min(idz1,total_cnt); ii++){
        unsigned int val = *(unsigned int*)ptd; ptd+=4; values_L.push_back(val); //cout << "--> val=" << hex << val << endl;
    }

    for (unsigned int ii=0; ii<min(idz2,total_cnt); ii++){
        if (!hflag){
            unsigned int val = *(unsigned int*)ptd; ptd+=4; val <<= 4; //cout << "   val=" << hex << val << endl;
            hold = *(unsigned char*)ptd; ptd++;    //cout << "   h=" << hex << hold << endl;
            val |= (hold>>4); values_L.push_back(val); //cout << "--> val=" << hex << val << endl;
            hflag = true; 
        } else {
            unsigned int val = *(unsigned int*)ptd; ptd+=4; //cout << "   val=" << hex << val << endl; cout << "   h=" << hex << hold << endl;
            unsigned int val1 = (hold & 0x0F); //cout << "   val1=" << hex << val1 << endl;
            val |= (val1<<16); values_L.push_back(val); //cout << "--> val=" << hex << val << endl;
            hflag = false; 
        }
    }

    hold = 0; hflag = false;
    for (unsigned int ii=idz+idz1+idz2; ii<total_cnt; ii++){
        unsigned int val1 = *(unsigned short*)ptd; ptd+=2; //cout << "   val1=" << hex << val1 << endl;
        unsigned int val = *(unsigned int*)ptd; ptd+=4;  //cout << "   val=" << hex << val << endl;
        val |= (val1<<16); values_L.push_back(val); //cout << "--> val=" << hex << val << endl;
    }

    // -------- compare ------------
    for (unsigned int ii=0; ii<total_cnt; ii++){
        unsigned int val = hvals[ii];
        unsigned int val1 = values_L[ii];
        if (val!=val1){
            ii+=0;
        }
    }
    //writeHmapToFile(hvals, "c:\\data\\hvals.txt");
    // --------- end read intensity table
*******/
    intens_recs.clear();
    std::vector<CntRecord>().swap(intens_recs);
    return ptr;
}

// create global mz  dictionary
bool PicoLocalCompress::createGlobalMzDict1(Sqlite* sql_in, unsigned char** /*compressed*/, unsigned int* /*length*/)
{
    //std::vector<unsigned char> mz_seq;
    sqlite3_stmt *stmt; bool success = false;
    sqlite3* db_in = sql_in->getDatabase();
    sqlite3_exec(db_in, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "Select PeaksMz FROM Peaks WHERE Id = 4811";
    if (sqlite3_prepare_v2(db_in, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                int mz_len1 = sqlite3_column_bytes(stmt, 0) / 8; mz_len = mz_len1;
                if (mz_len<=2){
                    success = true; break;
                }
                double* mz_blob = (double*)sqlite3_column_blob(stmt, 0);

                //unsigned long long max_long = 0;
                std::vector<double> diff4; diff4.push_back(0);
                map<double, unsigned int> hmpD; std::vector<unsigned int> hcntsD; std::vector<double> hvalsD;
                double prev = mz_blob[0];
                if (prev<min_mz) min_mz=prev;
                for (int ii=1; ii<mz_len; ii++){
                    double val = mz_blob[ii], dif = val-prev; diff4.push_back(dif);
                    prev = val;

                    map<double, unsigned int>::iterator itr = hmpD.find(dif); unsigned int id;
                    if (itr == hmpD.end()) {
                        id = static_cast<unsigned int>(hmpD.size());
                        hmpD.insert(std::pair<double, unsigned int>(
                            dif, static_cast<unsigned int>(hmpD.size())));
                        hcntsD.push_back(1);
                        hvalsD.push_back(dif);
                    } else {
                        id = itr->second;
                        hcntsD[id] += 1;
                    }
                }
                if (prev>max_mz) max_mz=prev;
                std::vector<CntRecordD> intens_recsD;
                for (unsigned int ii=0; ii<hcntsD.size(); ii++)
                    intens_recsD.push_back(CntRecordD(hcntsD[ii], hvalsD[ii], ii)); 
                sort(intens_recsD.begin(), intens_recsD.end(), sort_by_count_D);

        /*        std::vector<unsigned short> inv_map;
                for (unsigned int ii=0; ii<intens_recsD.size(); ii++){ 
                    unsigned short vali = (unsigned short)(32768*intens_recsD[ii].value);
                    inv_map.push_back(vali);
                }

            /*    std::vector<unsigned short> over;//, unsorted_over;  // list for debug
                for (unsigned int ii=0; ii<hvalsD.size(); ii++){ 
                    unsigned short vali = (unsigned short)(32768*hvalsD[ii]);
                    over.push_back(vali); //unsorted_over.push_back(vali);
                    //cout << ii << " - " << setprecision(9) << vali << endl;
                }
                sort(over.begin(), over.end()); */

                map<double, unsigned short> hmp6; 
                for (unsigned int ii=0; ii<intens_recsD.size(); ii++)
                    hmp6.insert(std::pair<double, unsigned short>(intens_recsD[ii].value, static_cast<unsigned short>(hmp6.size())));

                std::vector<unsigned short> indices;
                for (int ii=1; ii<mz_len; ii++){
                    double dif = diff4[ii];
                    unsigned short idx = hmp6[dif];
                    indices.push_back(idx);
                }

                std::vector<unsigned int> seq7; unsigned short prev7 = 0;
                //unsigned int previ = 0;
                for (int ii=1; ii<mz_len-3; ii++){
                    unsigned short idx = indices[ii], idx1 = indices[ii+1], idx2 = indices[ii+2];
                    if (idx==idx1 && idx1==idx2 && idx!=prev7){
                        if (seq7.size()>0) 
                            seq7.push_back(ii); 
                        seq7.push_back(idx); prev7=idx; ii+=2;
                    }
                }
                seq7.push_back(mz_len-1); 


                // ----------- compress scan -----------------
                unsigned char* compressed = (unsigned char*)malloc(mz_len*sizeof(unsigned char)); 
                if (!compressed){
                    cout << "null compressed data" << endl; return false;
                }
                unsigned char* ptr = compressed + 22;

                // ----- dict ---------
                *(double*)(compressed+14) = mz_blob[0];
                for (unsigned int ii=0; ii<intens_recsD.size(); ii++){
                    unsigned short vali = (unsigned short)(intens_recsD[ii].value*32768); 
                    *(unsigned short*)ptr = vali; ptr+=2; 
                }
                unsigned short dict_len = static_cast<unsigned short>(ptr - compressed - 22);
                *(unsigned short*)compressed = dict_len;

                // -------- base --------
                std::vector<unsigned int> base; unsigned int cur7 = 0; 
                for (unsigned int ii=0; ii<seq7.size()/2; ii++){
                    unsigned int vali = seq7[2*ii], loci = seq7[2*ii+1]; 
                    *ptr++ = vali; // set vali
                    unsigned int cnt7 = loci - cur7; 
                    *ptr++ = (cnt7>>8); // set loc (16 bits)
                    *ptr++ = cnt7&0xFF; 
                    for (unsigned int jj=0; jj<cnt7; jj++)
                        base.push_back(vali);
                    cur7 = loci;
                }
                unsigned int base_len = static_cast<unsigned int>(ptr-compressed)-dict_len-22;
                *(unsigned short*)(compressed+2) = base_len;

                // -------- indices --------
                std::vector<std::vector<unsigned int>> array(intens_recsD.size());
                for (unsigned int ii=0; ii<indices.size(); ii++){
                    unsigned short idx = indices[ii], idx1 = base[ii];
                    if (idx!=idx1){
                        array[idx].push_back(ii);
                    }
                }

                // -------- hfr --------
                for (int ii=0; ii<256; ii++){ // series of skip n write (1-3 byte per data)
                    if (array[ii].size()>0){                        
                        cur7 = 0; 
                        for (unsigned int jj=0; jj<array[ii].size(); jj++){
                            unsigned int loci = array[ii][jj], deli = loci - cur7; 
                            if (deli<127) {
                                *ptr++ = (deli | 0x80); // set loc (8 bits)
                            } else if (deli<16383){
                                *ptr++ = (((deli>>8)&0xFF) | 0x40); // set loc (16 bits)
                                *ptr++ = deli&0xFF;
                            } else {
                                *ptr++ = deli>>16;  // set loc (24 bits)
                                *ptr++ = (deli>>8)&0xFF;  
                                *ptr++ = deli&0xFF; 
                            }
                            cur7 = loci; 
                        }
                        *ptr++ = 255; 
                    } //else
                        //ii+=0;
                }
                unsigned int hfr_len = static_cast<unsigned int>(ptr-compressed)-base_len-dict_len-22;
                *(unsigned int*)(compressed+4) = hfr_len;
                
                // -------- lfr --------
                for (unsigned int ii=256; ii<intens_recsD.size(); ii++){ // series of skip n write val (1 byte per data)
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
                    } else
                        ii+=0;
                    ii+=0;
                }
                const unsigned int compressed_length = static_cast<unsigned int>(ptr - compressed);

                unsigned int lfr_len = compressed_length -hfr_len-base_len-dict_len-22;
                *(unsigned short*)(compressed+8) = lfr_len;
                *(unsigned int*)(compressed+10) = mz_len;
                
                cout << "  done m/z:  compressed_length=" << compressed_length << ", orig=" << mz_len*8 << " bytes (cr=" << setprecision(5) << (float)mz_len*8/compressed_length << setprecision(0) << ")" << endl; // [" << decompress_millis << "]" << endl;


                // ------- decompress ---------
                unsigned char* ptd = compressed+22;
                unsigned short dictd_len = *(unsigned short*)(compressed);
                unsigned short based_len = *(unsigned short*)(compressed+2);
                //unsigned int hfrd_len = *(unsigned int*)(compressed+4);
                //unsigned short lfrd_len = *(unsigned short*)(compressed+8);
                //unsigned int mzd_len = *(unsigned int*)(compressed+10);
                double mz0 = *(double*)(compressed+14);

                //unsigned char* decompress = (unsigned char*)malloc(mz_len*sizeof(double)); 

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
                for (int ii=0; ii<256; ii++){
                    if (ii==70)
                        ii+=0;
                    cur7 = 0; 
                    for ( ; ; ){
                        unsigned int skip = *ptd++; 
                        if (skip==255)
                            break;
                        if (skip>128)
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
                        if ((int)cur7>mz_len-1)
                            cur7+=0;                        
                        base1[cur7] = ii; 
                    }
                    ii+=0;
                }

                // -------- lfr --------
                for (unsigned int ii=256; ii<intens_recsD.size(); ii++){ // series of loc x val 
                    for ( ; ; ){
                        unsigned int loc1 = *ptd++;
                        bool last = false;
                        if (loc1>127){
                            loc1 = loc1-128; last = true;
                        }
                        unsigned int loc2 = *ptd++, loc3 = *ptd++;
                        unsigned int loci = (loc1<<16) | (loc2<<8) | loc3; //*ptd++;
                        if ((int)loci>mz_len-1)
                            loci+=0;    
                        base1[loci] = ii;
                        if (last)
                            break;
                    }
                    ii+=0;
                }

                // --------- verify indices ----------
                for (unsigned int ii=0; ii<base1.size(); ii++){
                    unsigned short idx = indices[ii], idx1 = base1[ii];
                    if (idx!=idx1){
                        ii+=0; 
                    }
                }

                // --------- restore m/z and verify  ----------
                //double * mzv = new double[mz_len]; mzv[0]=mz0; 
                double d_pos = mz0;
                for (int ii=1; ii<mz_len; ii++){
/*                    if (ii==109)
                        ii+=0;
                        double err1 = abs(mzv[ii-1] - d_pos);
                        double del0x = mz_blob[ii]-mz_blob[ii-1];
                        unsigned short idx6 = hmp6[del0x];
                        unsigned short vid8 = hmpd[idx6]; */

                    unsigned short idx = base1[ii-1];
                    unsigned short vid = hmpd[idx];
                    double vali = (double)vid/32768.0;
                    double step = d_pos + vali;
                    //mzv[ii] = step; 
                    d_pos = step; 
                    //double err = abs(mzv[ii] - mz_blob[ii]);
                    //if (err>0) //1E-7)
                    //    ii+=0;
                }
                //delete[] mzv;

                for (int ii=1; ii<mz_len; ii++){
                    unsigned short idx = base1[ii-1];
                    unsigned short vid = hmpd[idx];
                    double vali = (double)vid/32768.0;
                    double step = d_pos + vali;
                    //*(double *)decompress++ = step; 
                    d_pos = step; 
                }

                 //free_safe(decompress); decompress = NULL;

/*
                std::vector<CntRecord> intens_recs4; //std::vector<unsigned int> hcnts4, hvals4, mz_del;
                map<unsigned int, unsigned int> hmp5; int hindex5 = 0;
                for (unsigned int ii=0; ii<intens_recs4.size(); ii++)
                    hmp5.insert(std::pair<unsigned int, unsigned int>(intens_recs4[ii].value, hindex5++));

                //unsigned char* compressed = (unsigned char*)malloc(mz_len*sizeof(unsigned char)); 

                //unsigned char* ptr = compressed;
                // ---- generate mz rlc table ------
                *(unsigned int*)ptr = mz_len; ptr+=8; 
                *(double*)ptr = mz_blob[0]; ptr+=8;

                // ---- generate intensity table ------
    
                *(unsigned int*)ptr = hvals4.size(); ptr+=4; 

                for (unsigned int ii=0; ii<hvals4.size(); ii++){
                    //*(unsigned int*)ptr = hvals4[ii]; ptr+=4; 
                    *(unsigned int*)ptr = intens_recs4[ii].value; ptr+=4; 
                }

                // ---------- generate sequence --------------

                std::vector<unsigned int> vect;
                unsigned int prv=-1, max_idx = 0, max_vec = 0, vcnt = 0; int rlc = 0, max_rlc = 0; 
                bool hflag = false; unsigned char hold = 0;
                for (int ii=0; ii<mz_len-2; ii++){
                    unsigned int delta = mz_del[ii], cur = hmp5[delta];
                    unsigned int delta1 = mz_del[ii+1], nxt = hmp5[delta1];
                    if (ii==0){
                        prv = cur; vect.push_back(cur | 0x80); vcnt++; // set short index (8 bit)
                    } else {
                        if (prv!=cur){ 
                            if (cur==nxt){ // set index
                                if (rlc>max_rlc) max_rlc = rlc;
                                if (rlc>0){
                                    int cntr = rlc/31;
                                    for (int jj=0; jj<cntr; jj++)
                                        vect.push_back(0x5F); vcnt++; // large rlc (8 bits)
                                    int rem = rlc - 31*cntr;
                                    if (rem>0)
                                        vect.push_back(rem|0x40); vcnt++; // sml rlc (8 bits)
                                    rlc = 0; 
                                }
                                prv = cur; vcnt++; if (cur>max_idx) max_idx=cur; 
                                if (cur<32)
                                    vect.push_back(cur | 0x60); 
                                else 
                                    vect.push_back(cur | 0x1200); 

                            } else { // one off
                                if (rlc>max_rlc) max_rlc = rlc;
                                if (rlc>0){
                                    int cntr = rlc/31;
                                    for (int jj=0; jj<cntr; jj++)
                                        vect.push_back(0x5F); vcnt++;  // large rlc (8 bits)
                                    int rem = rlc - 31*cntr;
                                    if (rem>0)
                                        vect.push_back(rem|0x40); vcnt++; // sml rlc (8 bits)
                                    rlc = 0;
                                }
                                vcnt++; 
                                if (cur<128)
                                    vect.push_back(cur | 0x80); 
                                else 
                                    vect.push_back(cur | 0x1000); 
                            }

                        } else // rpt
                            rlc++;
                    }

                    //write bytes
                    for (unsigned int ii=0; ii<vect.size(); ii++){
                        unsigned int val = vect[ii];
                        if (!hflag) { // no hold
                            if ((val & 0x1000) == 0) { // one byte
                                *ptr = (unsigned char)(val&0xFF); ptr++;
                            } else {
                                *ptr = (unsigned char)((val>>8)&0xFF); ptr++;
                                hold = val&0x0F; hflag = true;
                            }
                        } else {
                            if ((val & 0x1000) == 0) { // one byte
                                unsigned int val1 = (hold<<4) | (val>>4)&0x0F;
                                *ptr = (unsigned char)(val); ptr++;
                                hold = val&0x0F;
                            } else {
                                unsigned int val1 = (hold<<4) | (val>>8)&0x0F;
                                *ptr = (unsigned char)(val); 
                                *ptr = (unsigned char)(val&0xFF); ptr+=2;
                                hflag = false;
                            }
                        }
                    }

                    vect.clear();
                }
                unsigned int delta1 = mz_del[mz_len-2], nxt = hmp5[delta1];
                if (nxt==prv)
                    rlc++;
                if (rlc>max_rlc) max_rlc = rlc;
                if (rlc>0){
                    int cntr = rlc/31;
                    for (int jj=0; jj<cntr; jj++)
                        vect.push_back(0x5F); vcnt++;  // large rlc (8 bits)
                    int rem = rlc - 31*cntr;
                    if (rem>0)
                        vect.push_back(rem|0x40); vcnt++; // sml rlc (8 bits)
                }                
                if (nxt!=prv){
                    vcnt++; 
                    if (nxt<128)
                        vect.push_back(nxt | 0x80); 
                    else 
                        vect.push_back(nxt | 0x1000); 
                }

                //write bytes
                for (unsigned int ii=0; ii<vect.size(); ii++){
                    unsigned int val = vect[ii];
                    if (!hflag) { // no hold
                        if ((val & 0x1000) == 0) { // one byte
                            *ptr = (unsigned char)(val&0xFF); ptr++;
                        } else {
                            *ptr = (unsigned char)((val>>8)&0xFF); ptr++;
                            hold = val&0x0F; hflag = true;
                        }
                    } else {
                        if ((val & 0x1000) == 0) { // one byte
                            unsigned int val1 = (hold<<4) | (val>>4)&0x0F;
                            *ptr = (unsigned char)(val); ptr++;
                            hold = val&0x0F;
                        } else {
                            unsigned int val1 = (hold<<4) | (val>>8)&0x0F;
                            *ptr = (unsigned char)(val); 
                            *ptr = (unsigned char)(val&0xFF); ptr+=2;
                            hflag = false;
                        }
                    }
                }
                if (hflag){
                    *ptr = (unsigned char)(hold<<4); ptr++; vcnt++; hflag = false;
                }
                *length = ptr - compressed;
                *(unsigned int*)(compressed+4) = vcnt;
                nxt +=0;

                // ------ decode m/z --------------
                //unsigned char* ptd = compressed;  // for decompression
                unsigned int mz_len2 = *(unsigned int*)ptd; ptd+=4;
                unsigned int codesize = *(unsigned int*)ptd; ptd+=4;
                double mzc = *(double*)ptd; ptd+=8;

                unsigned int total_cnt = *(unsigned int*)ptd; ptd+=4;

                map<unsigned int, unsigned int> inv_hmp4; 
                for (unsigned int ii=0; ii<total_cnt; ii++){
                    unsigned int val = *(unsigned int*)ptd; ptd+=4;
                    inv_hmp4.insert(std::pair<unsigned int, unsigned int>(ii, val));
                }

                double * mzs = new double[mz_len]; mzs[0] = mzc; 
                unsigned int val = 0, offset = 0, current = 1; 
                for (unsigned int ii=0; ii<codesize; ii++){
                    if (!hflag) { // no hold
                        unsigned int bb = *(unsigned char*)ptd; ptd++; //dcm_cnt++; //msb
                        if (bb>127){
                            val = bb - 128; // short idx
                            if (ii==0){
                                offset = inv_hmp4[val]; 
                                mzc += ((double)offset)/1E8; 
                            } else {
                                unsigned int vali = inv_hmp4[val]; 
                                mzc += ((double)vali)/1E8; 
                            }
                            mzs[current++] = mzc; 

                        } else if (bb>96){
                            val = bb - 96; // short set idx
                            offset = inv_hmp4[val]; 
                            mzc += ((double)offset)/1E8; mzs[current++] = mzc; 

                        } else if (bb>64){
                            val = bb - 64; // repeat
                            for (unsigned int jj=0; jj<val; jj++){
                                mzc += ((double)offset)/1E8; mzs[current++] = mzc; 
                            }

                        } else if (bb>32){ // 12-bit 
                            val = bb - 32; // long set idx
                            unsigned int bb1 = *(unsigned char*)ptd; ptd++; 
                            val = (val<<4) | (bb1>>4); val+=32;
                            hold = bb1&0x0F; hflag = true;
                            offset = inv_hmp4[val]; 
                            mzc += ((double)offset)/1E8; mzs[current++] = mzc; 
                            

                        } else { // long idx
                            unsigned int bb1 = *(unsigned char*)ptd; ptd++; 
                            val = (val<<4) | (bb1>>4); val+=128;
                            hold = bb1&0x0F; hflag = true;
                            unsigned int vali = inv_hmp4[val]; 
                            mzc += ((double)vali)/1E8; mzs[current++] = mzc; 
                        }

                    } else { // hold

                        unsigned int bb = *(unsigned char*)ptd; ptd++; //dcm_cnt++; //msb
                        if (hold>7){
                            val = ((hold & 0x7)<<4) | (bb>>4); // short idx
                            hold = bb & 0x0F;
                            unsigned int vali = inv_hmp4[val]; 
                            mzc += ((double)vali)/1E8; mzs[current++] = mzc; 

                        } else if (hold>6){
                            val = ((hold & 0x1)<<4) | (bb>>4); // short set idx
                            hold = bb & 0x0F;
                            offset = inv_hmp4[val]; 
                            mzc += ((double)offset)/1E8; mzs[current++] = mzc; 

                        } else if (hold>4){
                            val = ((hold & 0x1)<<4) | (bb>>4); // repeat
                            hold = bb & 0x0F; 
                            for (unsigned int jj=0; jj<val; jj++){
                                mzc += ((double)offset)/1E8; mzs[current++] = mzc; 
                            }

                        } else if (hold>2){ // 12-bit 
                            val = ((hold & 0x1)<<8) | bb; // long set idx
                            hflag = false; val+=32;
                            offset = inv_hmp4[val]; 
                            mzc += ((double)offset)/1E8; mzs[current++] = mzc;                             

                        } else { // long idx
                            val = ((hold & 0x1)<<8) | bb; // long idx
                            hflag = false; val+=128;
                            unsigned int vali = inv_hmp4[val]; 
                            mzc += ((double)vali)/1E8; mzs[current++] = mzc; 
                        }
                    }
                    if ((int)current>=mz_len){
                        cout << "current index exceeded length" << endl;
                        break;
                    }
                }

                //---------- verify -----------
                //cout << setprecision(9) << "ii=1A:" << ", val=" << mz_blob[1] << ", dif=" << (mz_blob[1]-mz_blob[0]) << ", del=" << (unsigned int)(1E8*((mz_blob[1]-mz_blob[0]))) << endl;
                for (int ii=0; ii<mz_len; ii++){
                    double val1 = mz_blob[ii], val2 = mzs[ii], dif = val2-val1;
                    if (abs(dif)>1E-6){
                        cout << "ii=" << ii << ", val=" << setprecision(9) << val2 << ", orig=" << val1 << ", dif=" << dif << endl;
                        ii+=0;
                    }
                }
                current+=0;
*/
                free_safe(compressed);
            } else if (rc == SQLITE_DONE) {
                success = true; break;
            } else {
                cout << "rc=" << rc << endl; break;
            }
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db_in, "COMMIT;", NULL, NULL, NULL);
    //std::cout << "done! overall compression=" << commas(compressed_size).c_str() << " orig=" << commas(original_size).c_str() << " bytes (cr=" << setprecision(5) << (float)original_size/compressed_size << setprecision(0) << ")" << endl;
    sql_in->closeDatabase(); delete sql_in; 
    return success;

}


// ---- generate combined indices table ------
unsigned char* PicoLocalCompress::generateCombinedIndices(const std::vector<unsigned int> &intensity, unsigned char* ptr2) //std::vector<unsigned long long> &intensity_L, unsigned char* ptr2)
{
    unsigned char* ptr = ptr2;
    //unsigned char* ptd = ptr; //<================= for decomp
    //cout << "   p-1=" << hex << *(unsigned int*)(ptr-4) << endl;
    //*(unsigned int*)ptr = intensity_L.size(); ptr+=4; 
    *(unsigned int*)ptr = static_cast<unsigned int>(intensity.size()); ptr+=4; //cout << "   p=" << hex << *(unsigned int*)(ptr-4) << endl;
    unsigned char hold = 0; bool hflag = false; 
    //for (unsigned int ii=0; ii<intensity_L.size(); ii++){
    //    unsigned long long val = hmap_L[intensity_L[ii]];  //cout << ii << ", val=" << hex << val << endl;
    //std::vector<unsigned char> codes; std::vector<unsigned int> values, code_idx, val5; int cmp_cnt = 0;
    for (unsigned int ii=0; ii<intensity.size(); ii++){
        unsigned int val = hmap[intensity[ii]];  //values.push_back(val); val5.push_back(intensity[ii]); //cout << ii << ", val=" << hex << val << endl;
        if (val==959)
            ii+=0;
        //if (cmp_cnt>11133)
        //    ii+=0;

        if (!hflag) {  // no hold

            if (val<128){
                *(unsigned char*)ptr = (unsigned char)(val|0x80); ptr++; //code_idx.push_back(cmp_cnt); codes.push_back(val|0x80); cmp_cnt++; //bidm.put((byte)(val1|0x80)); bcnt++;
            } else if (val<1152) {
                val -= 128; val |= 0x400;
                *(unsigned char*)ptr = (unsigned char)(val>>4); ptr++; //code_idx.push_back(cmp_cnt); codes.push_back((val>>4)); cmp_cnt++; //msb
                hold = (unsigned char)(val&0x0F); hflag = true; //lsb
            } else {
                val -= 1152;
                *(unsigned char*)ptr = (unsigned char)((val>>8)&0x3F); ptr++; //code_idx.push_back(cmp_cnt); codes.push_back((val>>8)&0x3F); cmp_cnt++; //msb
                *(unsigned char*)ptr = (unsigned char)(val&0xFF); ptr++; //code_idx.push_back(cmp_cnt); codes.push_back(val&0xFF); cmp_cnt++; //lsb
            }

        } else { // hold

            if (val<128){
                val |= 0x80; 
                unsigned int val1 = (hold<<4) | (val>>4); 
                *(unsigned char*)ptr = (unsigned char)(val1); ptr++;// code_idx.push_back(cmp_cnt); codes.push_back(val1); cmp_cnt++; //msb
                hold = (unsigned char)(val&0x0F); //lsb
            } else if (val<1152) {
                val -= 128; val |= 0x400;
                unsigned int val1 = (hold<<4) | (val>>8); 
                *(unsigned char*)ptr = (unsigned char)(val1); ptr++; //code_idx.push_back(cmp_cnt); codes.push_back(val1); cmp_cnt++; //msb
                *(unsigned char*)ptr = (unsigned char)(val&0xFF); ptr++; //code_idx.push_back(cmp_cnt); codes.push_back(val&0xFF); cmp_cnt++; //lsb
                hflag = false; 
            } else {
                val -= 1152;
                unsigned int val1 = (hold<<4) | (val>>12); 
                *(unsigned char*)ptr = (unsigned char)(val1); ptr++; //code_idx.push_back(cmp_cnt); codes.push_back(val1); cmp_cnt++; //msb
                unsigned int val2 = (val>>4)&0xFF;
                *(unsigned char*)ptr = (unsigned char)(val2); ptr++; //code_idx.push_back(cmp_cnt); codes.push_back(val2); cmp_cnt++; //mid-sb
                hold = (unsigned char)(val&0x0F); //lsb
            }
        }  
    }
    if (hflag){ // left over
        *(unsigned char*)ptr = (unsigned char)(hold<<4); ptr++; //code_idx.push_back(0); codes.push_back(hold<<4); cmp_cnt++; 
    }



    //int dcm_cnt = 0;
            // --------- decompress scan -----
/*        double * restored_mz = (double *)malloc(mz_len*sizeof(double)); //mz_buf = restored_mz; //new double[restored_mz_size];
        if (*restored_mz==NULL){
            std::cout << "null mz buf alloc" << endl; return false; //cout << "malloc decompress out of memory at 
        }
        for (int ii=0; ii<mz_len; ii++){
            double val = c0+c1*ii+c2*ii*ii+c3*ii*ii*ii;
            restored_mz[ii] = val;
        }
        
        unsigned int current = 0;
        unsigned int indices_size = *(unsigned int*)ptd; ptd+=4; //cout << "   size=" << hex << indices_size << endl; //bidm.getInt();
        float * restored_intensity = (float *)calloc(mz_len, sizeof(float)); //intensity_buf = restored_intensity; //new float[restored_mz_size];
        if (*restored_intensity==NULL){
            std::cout << "null intensity buf alloc" << endl; return false; //cout << "malloc decompress out of memory" 
        }

        bool first = true; 
        hflag = false; hold = 0; unsigned int val; 
        for (unsigned int ii=0; ii<indices_size; ii++){
            //if (dcm_cnt>11133)
            //    ii+=0;
            if (!hflag) { // no hold
                unsigned int bb = *(unsigned char*)ptd; ptd++; //dcm_cnt++; //msb
                if (bb>127)
                    val = bb - 128;
                else {
                    unsigned int bb1 = *(unsigned char*)ptd; ptd++; //dcm_cnt++; //lsb                    
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

                unsigned int bb = *(unsigned char*)ptd; ptd++; //dcm_cnt++; //msb
                if (hold>7) {
                    val = ((hold & 0x7)<<4) + (bb>>4);
                    hold = bb & 0x0F;//)<<4;
                } else if (hold>3){
                    val = (((unsigned int)hold & 0x3)<<8) + bb;
                    val += 128; hflag = false;
                } else {
                    unsigned int bb1 = *(unsigned char*)ptd; ptd++; //dcm_cnt++; // mid - sb
                    val = ((unsigned int)hold<<12) + (bb<<4) + (bb1>>4);
                    hold = bb1 & 0x0F;//)<<4; 
                    val += 1152; 
                }
            }
        
            if (current>10243)
                ii+=0;
            unsigned int cword = hvals[val]; 
            unsigned int del = cword & 0x03; unsigned int intens = cword >> 2; 
            if (del>0){
                for (int jj=0; jj<(int)del; jj++){
                    if (y_data[current+jj] > 0){
                        cout << dec << "   ii=" << (current+jj) << ", intens=" << 0 << ", del=" << del << ", y_dat=" << y_data[current+jj] << ", dif=" << -((int)y_data[current+jj]) << endl;
                        jj+=0;
                    }
                }
            }
            
            int dif1 = (int)intens-(int)y_data[current+del];
            if (dif1 != 0){// || current>10242){
                cout << dec << "ii=" << (current+del) << ", intens=" << intens << ", del=" << del << ", y_dat=" << y_data[current+del] << ", dif=" << ((int)intens-(int)(y_data[current+del])) << endl;
                ii += 0;
            }

            del++; current += del; 
            if (del>3)
                del+=0;

            if (current>(unsigned int)mz_len){
                if (first) {
                    std::cout << "current hop exceeds range at ii=" << ii << endl; first = false; //return false;
                }
                continue;
            } else {
                restored_intensity[current] = (float)intens; 
            }
        }
*/

    //writeCodesToFile(codes, code_idx, string("c:\\data\\codes.txt")); writeValuesToFile(val5, values, code_idx, string("c:\\data\\values.txt"));
    return ptr;
}


// compress raw type 4 binary data (ABSciex)
bool PicoLocalCompress::compressRawType4(const double* uncompressed_mz, const float* uncompressed_intensity, int uncompressed_length, unsigned char* compressed, unsigned int* compressed_mz_length, unsigned long long* /*intensity_sum*/)
{
    hmap.clear(); hvals.clear(); 
    std::vector<double> va_mz, va_del; std::vector<unsigned int> va_int; std::vector<unsigned int> hcnts; 
    double mz0 = uncompressed_mz[0], mzn = uncompressed_mz[uncompressed_length-1]; int intens_max = 0; //mz_prev=0,
    for (int ii=0; ii<uncompressed_length; ii++){ 
        int intens_0 = int(uncompressed_intensity[ii]+.5), intens_1 = -1, intens_2 = -1, intens_3 = -1; 
        if (intens_0>0){
            if (intens_0>intens_max) intens_max = intens_0;
            if (intens_max>0xffffff){
                intens_0 = 0xFFFFFF; // limit
                cout << "max intensity exceeded 0xFFFFFF" << endl; 
            }
            map<unsigned int, unsigned int>::iterator itr = hmap.find(intens_0);
            if (itr == hmap.end()) {
                hmap.insert(std::pair<unsigned int, unsigned int>(
                    static_cast<unsigned int>(intens_0), static_cast<unsigned int>(hmap.size())));
                hcnts.push_back(1);
                hvals.push_back(intens_0);
            } else {
                hcnts[itr->second] += 1;
            }
        }
        if (ii<uncompressed_length-1)
            intens_1 = int(uncompressed_intensity[ii+1]+.5);
        if (ii<uncompressed_length-2)
            intens_2 = int(uncompressed_intensity[ii+2]+.5);
        if (ii<uncompressed_length-3)
            intens_3 = int(uncompressed_intensity[ii+3]+.5);
        if (intens_0>0 && intens_1==0 && intens_2==0 && intens_3>0){
            double mzi_0 = uncompressed_mz[ii], mzi_1 = uncompressed_mz[ii+1], deli_0 = mzi_1 - mzi_0;
            va_mz.push_back(mzi_0); va_del.push_back(deli_0); va_int.push_back(intens_0); 
            double mzi_2 = uncompressed_mz[ii+2];
            va_mz.push_back(mzi_1); va_del.push_back(-1.0); va_int.push_back(intens_1); 
            double mzi_3 = uncompressed_mz[ii+3], deli_2 = mzi_3 - mzi_2;
            va_mz.push_back(mzi_2); va_del.push_back(deli_2); va_int.push_back(intens_2); 

            ii+=2; continue;
        } else if (intens_0>0 && intens_1==0 && intens_2>0){
            double mzi_0 = uncompressed_mz[ii], mzi_1 = uncompressed_mz[ii+1], deli_0 = mzi_1 - mzi_0;
            va_mz.push_back(mzi_0); va_del.push_back(deli_0); va_int.push_back(intens_0);
            double mzi_2 = uncompressed_mz[ii+2], deli_2 = mzi_2 - mzi_1;
            va_mz.push_back(mzi_1); va_del.push_back(deli_2); va_int.push_back(intens_1);

            ii+=1; continue;
        } else if (intens_0>0 && intens_1>0){
            double mzi_0 = uncompressed_mz[ii], mzi_1 = uncompressed_mz[ii+1], deli_0 = mzi_1 - mzi_0;
            va_mz.push_back(mzi_0); va_del.push_back(deli_0); va_int.push_back(intens_0); 

            continue;
        } else if (intens_0>0 && intens_1<0){
            double mzi_0 = uncompressed_mz[ii];
            va_mz.push_back(mzi_0); va_del.push_back(-2.0); va_int.push_back(intens_0);
            break;
        } else {
            std::cout << "unrecognized intensity pattern" << endl; 
        }
        ii+=0;
    }
    intens_recs.clear();
    intens_recs.reserve(hcnts.size());
    for (unsigned int ii=0; ii<hcnts.size(); ii++){
        intens_recs.push_back(CntRecord(hcnts[ii], hvals[ii], ii));
    }
    sort(intens_recs.begin(), intens_recs.end(), sort_by_count);
    //writeMzsDelIdxAndIntensToFile(va_mz, va_del, va_int, va_idx, "c:\\data\\ABSciex\\index2.csv");


    double* vect = (double *)calloc(11, sizeof(double)); 
    if (!vect){
        cerr << "vect allocation failed" << endl; return false;
    }
    double* sx = &vect[0], *sy = &vect[7];
    int cnt0 = 0;
    for (unsigned int ii=0; ii<va_del.size(); ii++){
        double yi = va_del[ii];
        if (yi<0) continue;        
        double mzi = va_mz[ii], xi = log10(mzi);  
        double xpi = 1.0; sy[0] += yi; cnt0++;
        for (int jj=1; jj<7; jj++){
            xpi *= xi; sx[jj] += xpi; 
            if (jj<4) sy[jj] += xpi*yi;
        }
    }
    sx[0] = cnt0;
    Predictor* pred = new Predictor(3);    // for dx
    pred->initialize(vect);
    double* data = pred->predict();
    double da0 = data[0], da1 = data[1], da2 = data[2], da3 = data[3];

    std::vector<double> vx_mz; 
    std::vector<unsigned int> vx_int, vx_idx, va_idx; va_idx.push_back(0);
    //int pcnt=1, jj_idx = 1;
    unsigned int vp_ofset = 0;
    for (unsigned int ii=0; ii<va_int.size()-1; ii++){
        unsigned int intens0 = va_int[ii]; 
        if (intens0==0) continue;
        vx_int.push_back(intens0); vx_mz.push_back(va_mz[ii]);
        unsigned int jj = ii+1;
        for ( ; jj<va_int.size(); jj++){
            unsigned int intens0 = va_int[jj];
            if (intens0>0) break;
        }
        int cnt = jj-ii-1;
        if (cnt==0){
            va_idx.push_back(++vp_ofset); vx_idx.push_back(1);
        } else if (cnt==1){
            vp_ofset += 2; va_idx.push_back(vp_ofset); vx_idx.push_back(2);
        } else {
            double pi = va_mz[ii], zi = log10(pi); unsigned int cnti = 0;
            for ( ; ; ){
                double dzi = da0+da1*zi+da2*zi*zi+da3*zi*zi*zi;
                //double prev_pi = pi;
                pi = pow(10.0, zi)+dzi; vp_ofset++; cnti++;
                zi = log10(pi);    
                if (pi>va_mz[jj]-1E-4)
                    break;
            }
            va_idx.push_back(vp_ofset); vx_idx.push_back(cnti);
        }
        ii+=0;
    }
    vx_idx.push_back(0); vx_int.push_back(va_int[va_int.size()-1]);
    int psize = va_idx[va_idx.size()-1];

    // pack indices
    std::vector<unsigned int> skp_idx; skp_idx.push_back(intens_recs[0].value);
    std::vector<unsigned int> hcnt1, hval1; map<unsigned int, unsigned int> hm1;
    for (unsigned int ii=0; ii<vx_idx.size()-1; ii++){ 
        unsigned int idxi = vx_idx[ii];
        if (ii==8300)
            ii+=0;
        if (idxi>1){
            unsigned int vali = 2*idxi;
            skp_idx.push_back(vali); 
            map<unsigned int, unsigned int>::iterator itr = hm1.find(vali);
            if (itr == hm1.end()) {
                hm1.insert(std::pair<unsigned int, unsigned int>(
                    vali, static_cast<unsigned int>(hm1.size())));
                hcnt1.push_back(1);
                hval1.push_back(vali);
            } else {
                hcnt1[itr->second] += 1;
            }
        } else {
            unsigned int jj = ii+1, idxj;
            while ((idxj=vx_idx[jj])==1){
                jj++;
                if (jj==vx_idx.size())
                    break;
            }
            if (jj==ii+1){
                skp_idx.push_back(2); ii++;
                map<unsigned int, unsigned int>::iterator itr = hm1.find(2);
                if (itr == hm1.end()) {
                    hm1.insert(std::pair<unsigned int, unsigned int>(
                        2, static_cast<unsigned int>(hm1.size())));
                    hcnt1.push_back(1);
                    hval1.push_back(2);
                } else {
                    hcnt1[itr->second] += 1;
                }
            } else {
                int cntj = jj-ii; unsigned int vali = cntj*2+1;
                skp_idx.push_back(vali); ii += cntj; 
                map<unsigned int, unsigned int>::iterator itr = hm1.find(vali);
                if (itr == hm1.end()) {
                    hm1.insert(std::pair<unsigned int, unsigned int>(
                        vali, static_cast<unsigned int>(hm1.size())));
                    hcnt1.push_back(1);
                    hval1.push_back(vali);
                } else {
                    hcnt1[itr->second] += 1;
                }
            }
            unsigned int vali = 2*idxj;
            skp_idx.push_back(vali);         
            map<unsigned int, unsigned int>::iterator itr = hm1.find(vali);
            if (itr == hm1.end()) {
                hm1.insert(std::pair<unsigned int, unsigned int>(
                    vali, static_cast<unsigned int>(hm1.size())));
                hcnt1.push_back(1);
                hval1.push_back(vali);
            } else {
                hcnt1[itr->second] += 1;
            }
        }
    }
    skp_idx.push_back(0); 
    std::vector<CntRecord> rec1; rec1.reserve(hcnt1.size()); 
    for (unsigned int rr=0; rr<hcnt1.size(); rr++)
        rec1.push_back(CntRecord(hcnt1[rr], hval1[rr], rr));
    sort(rec1.begin(), rec1.end(), sort_by_count);

    // unpack indices
    // int total_cnt = skp_idx.size();

    // ----------- compress scan -----------------
//    compressed = (unsigned char*)malloc(uncompressed_length*sizeof(unsigned char)); 
    unsigned char* ptr = compressed+58;

    // ----- dict ---------
    *(double*)(compressed) = mz0;
    *(double*)(compressed+8) = mzn;
    *(double*)(compressed+16) = data[0];
    *(double*)(compressed+24) = data[1];
    *(double*)(compressed+32) = data[2];
    *(double*)(compressed+40) = data[3];
    *(unsigned int*)(compressed+48) = psize;
    *(unsigned short*)(compressed+52) = (unsigned short) intens_recs.size();
    *(unsigned int*)(compressed+54) = uncompressed_length;
    free_safe(data); free_safe(vect); delete pred;

    bool hflag = false; unsigned char hold = 0;

    for (unsigned int ii=0; ii<skp_idx.size(); ii++){ // encode base level 2*vali        
        unsigned int cur = skp_idx[ii];
        if (cur<128){
            if (!hflag){
                *ptr++ = cur | 0x80;  
            } else {
                *ptr++ = (hold<<4) | (cur>>4) | 0x8;  
                hold = cur & 0x0F; 
            }
        } else if (cur<1152){ // 1.5
            cur -= 128;
            if (!hflag){
                *ptr++ = (cur>>4) | 0x40; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4) | (cur>>8) | 0x4; 
                *ptr++ = cur & 0xFF; hflag = false; 
            }
        } else if (cur<9344){ // 2
            cur -= 1152;
            if (!hflag){
                *ptr++ = (cur>>8) | 0x20; 
                *ptr++ = cur & 0xFF; 
            } else {
                *ptr++ = (hold<<4) | (cur>>12) | 0x2; 
                *ptr++ = (cur>>4) & 0xFF; 
                hold = cur & 0x0F; 
            }
        } else if (cur<74880) { // 2.5
            cur -= 9344;
            if (!hflag){
                *ptr++ = (cur>>12) | 0x10; 
                *ptr++ = (cur>>4) & 0xFF; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4) | 0x1; 
                *ptr++ = (cur>>8); 
                *ptr++ = cur & 0xFF; hflag = false; 
            }
        } else { //if (deli<599168) { // 3  
            cur -= 74880;
            if (!hflag){
                *ptr++ = (cur>>16) | 0x08; 
                *ptr++ = (cur>>8);
                *ptr++ = cur;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>12) | 0x80; 
                *ptr++ = (cur>>4);
                hold = cur & 0xF; 
            }
        }
    }
    //int base_len = ptr - compressed;

    // generate levels -- no dict
    for (unsigned int ii=1; ii<intens_recs.size(); ii++){ 
        int vali = intens_recs[ii].value, prev_loc = 0; 
        map<unsigned int, unsigned int> hm2; std::vector<unsigned int> hcnt2, hval2, vx_hops, vx_lev; vx_lev.push_back(vali);
        for (unsigned int jj=0; jj<vx_int.size(); jj++){
            int intensi = vx_int[jj];
            if (intensi != vali) continue;
            int deli = jj - prev_loc; vx_hops.push_back(deli); vx_lev.push_back(deli);
            prev_loc = jj;

            map<unsigned int, unsigned int>::iterator itr = hm2.find(deli);
            if (itr == hm2.end()) {
                hm2.insert(std::pair<unsigned int, unsigned int>(
                    static_cast<unsigned int>(deli), static_cast<unsigned int>(hm2.size())));
                hcnt2.push_back(1);
                hval2.push_back(deli);
            } else {
                hcnt2[itr->second] += 1;
            }
        } 
        vx_lev.push_back(0);
        std::vector<CntRecord> recs; recs.reserve(hcnt2.size()); 
        for (unsigned int rr=0; rr<hcnt2.size(); rr++)
            recs.push_back(CntRecord(hcnt2[rr], hval2[rr], rr));
        sort(recs.begin(), recs.end(), sort_by_count);

        for (unsigned int jj=0; jj<vx_lev.size(); jj++){
            unsigned int cur = vx_lev[jj];
            if (cur<128){
                if (!hflag){
                    *ptr++ = cur | 0x80;  
                } else {
                    *ptr++ = (hold<<4) | (cur>>4) | 0x8;  
                    hold = cur & 0x0F; 
                }
            } else if (cur<1152){ // 1.5
                cur -= 128;
                if (!hflag){
                    *ptr++ = (cur>>4) | 0x40; 
                    hold = cur & 0xF; hflag = true;
                } else {
                    *ptr++ = (hold<<4) | (cur>>8) | 0x4; 
                    *ptr++ = cur & 0xFF; hflag = false; 
                }
            } else if (cur<9344){ // 2
                cur -= 1152;
                if (!hflag){
                    *ptr++ = (cur>>8) | 0x20; 
                    *ptr++ = cur & 0xFF; 
                } else {
                    *ptr++ = (hold<<4) | (cur>>12) | 0x2; 
                    *ptr++ = (cur>>4) & 0xFF; 
                    hold = cur & 0x0F; 
                }
            } else if (cur<74880) { // 2.5
                cur -= 9344;
                if (!hflag){
                    *ptr++ = (cur>>12) | 0x10; 
                    *ptr++ = (cur>>4) & 0xFF; 
                    hold = cur & 0xF; hflag = true;
                } else {
                    *ptr++ = (hold<<4) | 0x1; 
                    *ptr++ = (cur>>8); 
                    *ptr++ = cur & 0xFF; hflag = false; 
                }
            } else { //if (deli<599168) { // 3  
                cur -= 74880;
                if (!hflag){
                    *ptr++ = (cur>>16) | 0x08; 
                    *ptr++ = (cur>>8);
                    *ptr++ = cur;
                } else {
                    *ptr++ = (hold<<4); 
                    *ptr++ = (cur>>12) | 0x80; 
                    *ptr++ = (cur>>4);
                    hold = cur & 0xF; 
                }
            }
        }
        //int lev_len = ptr - compressed - 58 - base_len;
        // total_cnt += vx_lev.size();
    }
    if (hflag){
        *ptr++ = (hold<<4); hflag = false;
    } 
    const unsigned int total_len = static_cast<unsigned int>(ptr - compressed);
    if (total_len>(unsigned int)(mz_len*1.5)){
        cout << "length exceeeded allocated buf len" << endl; return false;
    }
    *compressed_mz_length = total_len;
    return true;
}

// compress raw type 1 binary data (Waters)
bool PicoLocalCompress::compressRawType1_N(const std::vector<unsigned int> &uncompressed_buf,
                                           int uncompressed_length, unsigned char* compressed,
                                           unsigned int* compressed_mz_length, unsigned long long* intensity_sum,
                                           const std::vector<double> &calib_coeff_, bool ms_type_6)
{
    std::vector<double> calib_coeff(calib_coeff_);
    if (uncompressed_length == 0){
        *(unsigned int *)compressed = 0x80000000;
        *compressed_mz_length = 4; return true;
    }
    unsigned int calib_size = static_cast<unsigned int>(calib_coeff.size());
    bool no_calibration = (calib_size == 0); // note 0 size means no calibration
    if (calib_size > 6) {
        cout << "caution: more than 6 calibration coefficients -- higher coefficients ignored" << endl;
    }
    else if (calib_size > 0){
        calib_coeff.resize(6);
        if (calib_size == 1)
            calib_coeff[1] = 1.0;
    }
    if (!no_calibration){
        bool calib_test = true;
        for (unsigned int ii = 0; ii < calib_coeff.size(); ii++){
            if (ii == 1 && calib_coeff[1] != 1.0){
                calib_test = false; break;
            }
            else {
                if (calib_coeff[ii] != 0){
                    calib_test = false; break;
                }
            }
        }
        if (calib_test == true){
            calib_coeff.clear(); no_calibration = true;
        }
    }

    PicoLocalDecompress pld;
    double* vect = (double *)calloc(11, sizeof(double));
    double* sx = &vect[0], *sy = &vect[7]; int pcnt = 0;
    std::vector<double> delta_dbl, delta_dbl_mz;
    double min_delta = 999999.0, max_delta = 0, previous_mz = 0; unsigned int previous_intens = 0; //, min_mz_dbl = 0
    for (int ii = 0; ii < uncompressed_length; ii++){
        unsigned int intens_i = uncompressed_buf[2 * ii], mzi = uncompressed_buf[2 * ii + 1];
        double mzi_dbl = pld.decodeAndCalibrateMzType1(mzi, calib_coeff, NULL);
        double delta = mzi_dbl - previous_mz;
        if (delta<min_delta) min_delta = delta;
        if (intens_i > 0 || previous_intens>0){
            if (delta>max_delta) max_delta = delta;
            if (ii > 0){
                //if (pcnt == 0) min_mz_dbl = mzi_dbl;
                delta_dbl.push_back(delta); delta_dbl_mz.push_back(mzi_dbl);
                sy[0] += delta; pcnt++; double xpi = 1.0;
                for (int jj = 1; jj < 7; jj++){
                    xpi *= mzi_dbl; sx[jj] += xpi;
                    if (jj < 4) sy[jj] += xpi*delta;
                }
            }
        }
        previous_intens = intens_i; previous_mz = mzi_dbl;
        //if (pcnt == 2) break;  //<-------- debug only
    }

    sx[0] = pcnt;
    Predictor* pred = new Predictor(3);
    pred->initialize(vect);
    double* data = pred->predict();
    //double aa = data[0], bb = data[1], cc = data[2], dd = data[3];
    delete pred; free_safe(vect);
    const int steps_size = 12;
    unsigned int thrs[] = { 0x3C000000, 0x44000000, 0x4C000000, 0x54000000, 0x5C000000, 0x64000000, 0x6c000000, 0x74000000, 0x7C000000, 0x84000000 }; //std::vector<double> mzzz; std::vector<unsigned int> mzzraw;
    unsigned int steps[] = { 3520, 2496, 1768, 1248, 880, 624, 440, 312, 256, 0, 0, 0 };
    hmap.clear(); hvals.clear(); int intens_max = 0;
    std::vector<unsigned int> hcnts, va_del, va_int, va_stp, va_pos, va_bmx, delta;
    std::vector<unsigned int> shift_idxs, va_gear, va_mzr, va_scl; std::vector<unsigned char> shift_levs;
    unsigned long long sum_intens = 0;
    int istart = 0, iend = uncompressed_length, level = -1; // remove up to 2 leading / trailing zeros
    if ((int)uncompressed_buf.size()>0)
        if (uncompressed_buf[2 * istart] == 0) istart++;
    if ((int)uncompressed_buf.size()>2)
        if (uncompressed_buf[2 * istart] == 0) istart++;
    if (iend > 0)
        if (uncompressed_buf[2 * iend - 2] == 0) iend--;
    if (iend > 0)
        if (uncompressed_buf[2 * iend - 2] == 0) iend--;
    if (iend < istart){ //<============ test only
        *(unsigned int *)compressed = 0x80000000;
        *compressed_mz_length = 4; return true;
    }
    unsigned int mz_prev = 0, mz0 = 0, min_below = 99999, thr = 0, stp = 0; //, min_above=0
    if ((2 * istart + 1) < (int)uncompressed_buf.size()){
        mz0 = uncompressed_buf[2 * istart + 1];
    } else {
        if (uncompressed_buf.size()>0)
            mz0 = uncompressed_buf[uncompressed_buf.size() - 1];
    }
    if (ms_type_6){
        steps[0]=272; steps[1]=196; steps[2]=128; steps[3]=96; steps[4]=64; steps[5]=48; steps[6]=32; steps[7]=16; stp=steps[0];
        unsigned char prev_lev = mz0; mz0 >>= 8; 
        shift_levs.push_back(prev_lev); shift_idxs.push_back(0); //va_gear.pushback(lev);
        Reader *rdr = new Reader(verbose);
        std::vector<std::pair<double, double>> in_data, nz_data;
        double* vect = (double *)calloc(11, sizeof(double)); 
        double* sx = &vect[0], *sy = &vect[7]; int pcnt = 0; 
        double prev_intens=istart>0?uncompressed_buf[2*istart-2]:0, prev_mzi=istart>0?rdr->decodeMzType1_6(uncompressed_buf[2*istart-1]):0;
        for (int ii=istart; ii<iend; ii++){ 
            int intens_0 = uncompressed_buf[2*ii];
            unsigned int mzi_0 = uncompressed_buf[2*ii+1]; double mzi_d = rdr->decodeMzType1_6(mzi_0); 
            if (intens_0>0){
                nz_data.push_back(std::pair<double, double>(mzi_d, intens_0)); va_mzr.push_back(mzi_0>>8); va_scl.push_back(mzi_0&0xff); 
                sum_intens += intens_0;
                if (intens_0>intens_max) intens_max = intens_0; va_int.push_back(intens_0>>8); 
                if (intens_max>0xffffff){
                    intens_0 = 0xFFFFFF; // limit
                    cout << "max intensity exceeded 0xFFFFFF" << endl; //break;
                }
                map<unsigned int, unsigned int>::iterator itr = hmap.find(intens_0);
                if (itr == hmap.end()) {
                    hmap.insert(std::pair<unsigned int, unsigned int>(
                        static_cast<unsigned int>(intens_0),
                        static_cast<unsigned int>(hmap.size())));
                    hcnts.push_back(1);
                    hvals.push_back(intens_0);
                } else {
                    hcnts[itr->second] += 1;
                }

                unsigned char lev = mzi_0; mzi_0 >>= 8; va_gear.push_back(lev);
                if (lev != prev_lev){
                    prev_lev = lev; shift_levs.push_back(lev); shift_idxs.push_back(ii); 
                }
                unsigned int deli = mzi_0-mz_prev;
                if (lev != prev_lev) {
                    prev_lev = lev;
                    delta.push_back(min_below / 8 - 1);
                    va_bmx.push_back(min_below);
                    va_pos.push_back(static_cast<unsigned int>(va_del.size()));
                    while (prev_lev++
                           < lev) //<====================xxxxxxxxxxxxxxxxxx===========yyyyyyyyyyyyy
                        thr = thrs[++level];
                    stp = steps[level];
                    va_stp.push_back(level);
                    min_below = stp;
                }
                if (deli<stp){
                    if (deli<min_below) min_below=deli;
                }
                if (ii<iend) 
                    va_del.push_back(deli); 
                else
                    va_del.push_back(0);
                mz_prev = mzi_0;

                if (prev_intens==0){  // zero predictor
                    double mzi_md = prev_mzi;
                    double delmz = mzi_d-mzi_md, xpi = 1.0;
                    sy[0] += delmz; pcnt++;
                    for (int jj=1; jj<7; jj++){
                        xpi *= mzi_d; sx[jj] += xpi; 
                        if (jj<4) sy[jj] += xpi*delmz;
                    }
                    in_data.push_back(std::pair<double, double>(mzi_md, delmz));
                } else {
                    if ((2*ii+3)<(int)uncompressed_buf.size()){
                        unsigned int mzi_p = uncompressed_buf[2*ii+3];
                        double mzi_pd = rdr->decodeMzType1_6(mzi_p);
                        double delmz = mzi_pd-mzi_d, xpi = 1.0;
                        sy[0] += delmz; pcnt++;
                        for (int jj=1; jj<7; jj++){
                            xpi *= mzi_d; sx[jj] += xpi; 
                            if (jj<4) sy[jj] += xpi*delmz;
                        }
                        in_data.push_back(std::pair<double, double>(mzi_d, delmz));
                    }
                }
            }
            prev_intens=intens_0; prev_mzi=mzi_d;
        }
        va_pos.push_back(static_cast<unsigned int>(va_del.size())); delta.push_back(0); va_stp.push_back(steps[0]);
        delete rdr;
        sx[0] = pcnt;
        Predictor* pred = new Predictor(3);
        pred->initialize(vect);
        double *data = pred->predict(); ///---------------

        std::vector<double> vaa_mz, vaa_delz, pred_delz, nz_mzi, nz_intensi; double max_er = 0, mz_er=0; //<-------- debug only
        for (unsigned int ii=0; ii<nz_data.size(); ii++){ 
            std::pair<double, double> pair = nz_data[ii];
            nz_mzi.push_back(pair.first); nz_intensi.push_back(pair.second);
        }
        double aa=*(data+3), bb=*(data+2), cc=*(data+1), dd=*data;
        for (unsigned int ii=0; ii<in_data.size(); ii++){ 
            std::pair<double, double> pair = in_data[ii];
            vaa_mz.push_back(pair.first); vaa_delz.push_back(pair.second);
            double cur = pair.first;
            double k2 = (double)cur*cur;
            double vali = aa*cur*k2+bb*k2+cc*cur+dd; pred_delz.push_back(vali);
            double err = abs(pair.second-vali);
            if (err>max_er){
                max_er=err; mz_er=cur;
            }
        }
        free_safe(vect); delete pred; //free_safe(data); 
        
    } else {
        mz0 = 0;
        if ((2 * istart + 1) < (int)uncompressed_buf.size()){
            mz0 = uncompressed_buf[2 * istart + 1];
        } else {
            if (uncompressed_buf.size()>0)
                mz0 = uncompressed_buf[uncompressed_buf.size() - 1];
        }

        mz_prev = 0;
        unsigned int min_below=99999, thr=thrs[0], stp=0; bool first_del = true;
        for (int ii=istart; ii<iend; ii++){ 
            int intens_0 = uncompressed_buf[2*ii]; 
            if (intens_0>0){
                sum_intens += intens_0;
                if (intens_0>intens_max) intens_max = intens_0; va_int.push_back(intens_0); 
                //if (intens_max>0xffffff){
                //    intens_0 = 0xFFFFFF; // limit
                //    cout << "max intensity exceeded 0xFFFFFF" << endl; //break;
                //}
                map<unsigned int, unsigned int>::iterator itr = hmap.find(intens_0);
                if (itr == hmap.end()) {
                    hmap.insert(std::pair<unsigned int, unsigned int>(
                        static_cast<unsigned int>(intens_0),
                        static_cast<unsigned int>(hmap.size())));
                    hcnts.push_back(1);
                    hvals.push_back(intens_0);
                } else {
                    hcnts[itr->second] += 1;
                }

                unsigned int mzi_0 = uncompressed_buf[2*ii+1], deli = mzi_0-mz_prev;
                if ((deli%8)!=0){
                    if (first_del){
                        cout << "del not 8 multiple" << endl; first_del = false;
                    }
                }
                if (mzi_0>thr){
                    if (thr>0){
                        delta.push_back(min_below/8-1); va_bmx.push_back(min_below); va_pos.push_back(static_cast<unsigned int>(va_del.size()));
                    }
                    while(mzi_0>thr)
                        thr = thrs[++level];
                    stp = steps[level]; va_stp.push_back(level); min_below=stp;
                }
                if (deli<stp){
                    if (deli<min_below) min_below=deli;
                }
                if (ii<iend) 
                    va_del.push_back(deli); 
                else
                    va_del.push_back(0);
                mz_prev = mzi_0;
            }
            ii+=0;
        }
        va_pos.push_back(static_cast<unsigned int>(va_del.size()) + 1);
    }

    intens_recs.clear(); 
    if (hcnts.size()>0)
        intens_recs.reserve(hcnts.size());
    for (unsigned int ii=0; ii<hcnts.size(); ii++)
        intens_recs.push_back(CntRecord(hcnts[ii], hvals[ii], ii));
    std::sort(intens_recs.begin(), intens_recs.end(), sort_by_count);
    *intensity_sum = sum_intens;

    bool no_compression = false; 
    if (va_int.size() < MIN_CENTROIDED_PEAKS){
        no_compression = true; 
    } else {
        double mzn = uncompressed_buf[2*iend-1];
        double delta_x = mzn - mz0;
        if (delta_x < MIN_WATERS_PEAK_MZ_SPAN)
            no_compression = true; 
    }

    if (no_compression){
        unsigned char* ptr = compressed;
        *(unsigned int *)ptr = static_cast<unsigned int>(va_del.size()) | 0x80000000 | (no_calibration?0:0x40000000); ptr+=4; 
        if (!no_calibration){
            for (int ii = 0; ii<5; ii++){
                *(double *)ptr = calib_coeff[ii]; ptr += 8;
            }
            if (calib_coeff[1]>0){
                *(double *)ptr = calib_coeff[5]; ptr += 8;
            }
        }
        //*ptr++ = COMP_VER;
        if (va_del.size() > 0){
            for (int ii = 0; ii < 4; ii++){
                *(double *)ptr = data[ii]; ptr += 8; //<==================== NEW QDF OVERRIDE =====================
            }
        }
        free_safe(data);

        unsigned int mz_1 = 0;//, mz_e = 0;
/*        if (istart>1){
            mz_1 = uncompressed_buf[2*istart-1]; 
            *(unsigned int *)ptr = mz_1; ptr+=4; // include boundary
        }*/
        for (unsigned int ii=0; ii<va_del.size(); ii++){
            unsigned int mzi = va_del[ii];
            if (mz_1!=0)
                mzi -= mz_1;
            *(unsigned int *)ptr = mzi; ptr+=4; 
            *(unsigned int *)ptr = va_int[ii]; ptr+=4; 
            mz_1=0; //mz_e = mzi;
        }
/*        if (iend<uncompressed_length-1){
            unsigned int mzi = uncompressed_buf[2*uncompressed_length-1]-mz_e;
            *(unsigned int *)ptr = mzi; ptr+=4; 
        }*/
        *compressed_mz_length = static_cast<unsigned int>(ptr - compressed);
        return true;
    }

    // ----- dict --------- 
    unsigned char* ptr = compressed;//+42 -->  +90
    *(unsigned int*)ptr = static_cast<unsigned int>(va_int.size()) | (no_calibration?0:0x40000000) | (ms_type_6?0x20000000:0); ptr+=4;// base len
    if (!no_calibration){
        for (int ii=0; ii<5; ii++){
            *(double *)ptr = calib_coeff[ii]; ptr+=8;
        }
        if (calib_coeff[1]>0){
            *(double *)ptr = calib_coeff[5]; ptr += 8;
        }
    }
    //*ptr++ = COMP_VER;
    for (int ii = 0; ii < 4; ii++){
        *(double *)ptr = data[ii]; ptr += 8; //<==================== NEW QDF OVERRIDE =====================
    }
    free_safe(data);

    if (ms_type_6){
        *(unsigned int*)ptr = mz0 | (static_cast<unsigned int>(shift_levs.size())<<24); ptr += 4;
        *(unsigned short*)ptr = (unsigned short)intens_recs[0].value; ptr+=2;// base intens
        for (unsigned int ii=0; ii<3; ii++){
            *(double*)ptr = *(data+ii); ptr+=8; // double aa=*(data+3), bb=*(data+2), cc=*(data+1), dd=*data;
        }
        *(unsigned char*)ptr++ = shift_levs[0];
        for (unsigned int ii=1; ii<shift_levs.size(); ii++){
            *(unsigned char*)ptr++ = (shift_levs[ii]-shift_levs[ii-1]); 
        }
        for (unsigned int ii=1; ii<shift_idxs.size(); ii++){
            *(unsigned short*)ptr = (unsigned short)(shift_idxs[ii]-shift_idxs[ii-1]); ptr+=2;
        }
        //free_safe(data); 

    } else {
        *(unsigned int*)ptr = mz0; ptr += 4;
        *(unsigned short*)ptr = (unsigned short)intens_recs[0].value; ptr+=2;// base intens
        *ptr++ = static_cast<unsigned char>(va_stp.size());
        for (int ii=0; ii<(int)va_stp.size(); ii++)
            *ptr++ = va_stp[ii];
        for (int ii=0; ii<(int)va_stp.size(); ii++){
            *(unsigned short*)ptr = (unsigned short)delta[ii]; ptr+=2; //12+4
        }
        unsigned int val = va_pos[0]; unsigned char usb = val>>16;
        *(unsigned short*)ptr = val; ptr+=2; *ptr++ = usb;
        for (int ii=0; ii<(int)va_stp.size(); ii++){
            val = va_pos[ii+1]-va_pos[ii]; usb = val>>16;
            *(unsigned short*)ptr = val; ptr+=2; *ptr++ = usb;
        }
    }
    *(unsigned int*)ptr = uncompressed_length; ptr+=4; //38


    // other intensity levels
    bool hflag = false; unsigned char hold = 0;
    std::vector<std::vector<unsigned int>> indices_intensity; indices_intensity.resize(intens_recs.size()-1);
    for (unsigned int ii=1; ii<intens_recs.size(); ii++){
        std::vector<unsigned int> indices;
        unsigned int val = intens_recs[ii].value; indices.push_back(val);
        unsigned int prev_jj = 0;
        for (unsigned int jj=0; jj<va_int.size(); jj++){
            if (va_int[jj] != val) continue;
            unsigned int del = jj - prev_jj; indices.push_back(del);
            prev_jj = jj;            
        }
        indices.push_back(0); indices_intensity[ii-1] = indices;

        //---- encode intensity indices -----
        for (unsigned int ii=0; ii<indices.size(); ii++){     
            unsigned int cur = indices[ii];
            if (cur<128){
                if (!hflag){
                    *ptr++ = cur | 0x80;  
                } else {
                    *ptr++ = (hold<<4) | (cur>>4) | 0x8;  
                    hold = cur & 0x0F; 
                }
            } else if (cur<1152){ // 1.5
                cur -= 128;
                if (!hflag){
                    *ptr++ = (cur>>4) | 0x40; 
                    hold = cur & 0xF; hflag = true;
                } else {
                    *ptr++ = (hold<<4) | (cur>>8) | 0x4; 
                    *ptr++ = cur & 0xFF; hflag = false; 
                }
            } else if (cur<9344){ // 2
                cur -= 1152;
                if (!hflag){
                    *ptr++ = (cur>>8) | 0x20; 
                    *ptr++ = cur & 0xFF; 
                } else {
                    *ptr++ = (hold<<4) | (cur>>12) | 0x2; 
                    *ptr++ = (cur>>4) & 0xFF; 
                    hold = cur & 0x0F; 
                }
            } else if (cur<74880) { // 2.5
                cur -= 9344;
                if (!hflag){
                    *ptr++ = (cur>>12) | 0x10; 
                    *ptr++ = (cur>>4) & 0xFF; 
                    hold = cur & 0xF; hflag = true;
                } else {
                    *ptr++ = (hold<<4) | 0x1; 
                    *ptr++ = (cur>>8); 
                    *ptr++ = cur & 0xFF; hflag = false; 
                }
            } else if (cur<599168) { // 3  
                cur -= 74880;
                if (!hflag){
                    *ptr++ = (cur>>16) | 0x08; 
                    *ptr++ = (cur>>8); 
                    *ptr++ = cur;
                } else {
                    *ptr++ = (hold<<4); 
                    *ptr++ = (cur>>12) | 0x80; 
                    *ptr++ = (cur>>4);
                    hold = cur & 0xF; 
                }
            } else if (cur<4793472) { // 3.5
                cur -= 599168;
                if (!hflag){
                    *ptr++ = (cur>>20) | 0x04; 
                    *ptr++ = (cur>>12); 
                    *ptr++ = (cur>>4); 
                    hold = cur & 0xF; hflag = true;
                } else {
                    *ptr++ = (hold<<4); 
                    *ptr++ = (cur>>16)| 0x40;
                    *ptr++ = (cur>>8); 
                    *ptr++ = cur; hflag = false;
                }
            } else if (cur<38347904) { // 4
                cur -= 4793472;
                if (!hflag){
                    *ptr++ = (cur>>24) | 0x02; 
                    *ptr++ = (cur>>16); 
                    *ptr++ = (cur>>8); 
                    *ptr++ = cur; 
                } else {
                    *ptr++ = (hold<<4); 
                    *ptr++ = (cur>>20) | 0x20; 
                    *ptr++ = (cur>>12);
                    *ptr++ = (cur>>4);
                    hold = cur & 0xF; 
                }
            } else if (cur<306783360) { // 4.5
                cur -= 38347904;
                if (!hflag){
                    *ptr++ = 0x01; 
                    *ptr++ = (cur>>20);
                    *ptr++ = (cur>>12);
                    *ptr++ = (cur>>4);
                    hold = cur & 0xF; hflag = true;
                } else {
                    *ptr++ = (hold<<4); 
                    *ptr++ = (cur>>24) | 0x10; 
                    *ptr++ = (cur>>16); 
                    *ptr++ = (cur>>8); 
                    *ptr++ = cur; hflag = false;
                }
            } else { // 5
                cur -= 306783360;
                if (!hflag){
                    *ptr++ = 0x0; 
                    *ptr++ = (cur>>24); 
                    *ptr++ = (cur>>16); 
                    *ptr++ = (cur>>8);
                    *ptr++ = cur;
                } else {
                    *ptr++ = (hold<<4); 
                    *ptr++ = (cur>>28); 
                    *ptr++ = (cur>>20); 
                    *ptr++ = (cur>>12); 
                    *ptr++ = (cur>>4); 
                    hold = cur & 0xF; 
                }
            }
        }        
    }
    if (!hflag){ // final zero
        *ptr++ = 0x80;  
    } else {
        *ptr++ = (hold<<4) | 0x8;  hold = 0; 
    }
    //int blen_intens = ptr - compressed;

    // ---------- encode mz indices ---------------
    std::vector<unsigned int> bruns, run_len; unsigned int lasti = 0, last_val = 0, last_big = 0; level=0;
    unsigned int pos=va_pos[0], del=delta[0];
    if (ms_type_6) {
        stp = static_cast<unsigned int>(va_int.size()) + 1; // mz_len+1;
    } else {
        stp = steps[va_stp[level]];
    }
    for (unsigned int ii=1; ii<va_del.size(); ii++){ 
        unsigned int deli = va_del[ii];
        if (ii>=pos){
            if (level < (int)va_pos.size()-1){
                pos = va_pos[++level];
                if ((int)pos < va_int.size()){
                    del = delta[level]; stp = steps[va_stp[level]];
                } else {
                    del = delta[va_stp.size() - 1]; 
                    unsigned int stpi = va_stp[va_stp.size() - 1];
                    if (stpi < va_stp.size()){
                        stp = steps[stpi];
                    }
                }
            }
        }

        if (deli<stp){
            if (deli/8 < del)
                ii+=0;
            unsigned int vali = deli/8-del; 
            if (vali!=last_val){
                run_len.push_back(last_val); run_len.push_back(ii-lasti);  
                last_val = vali; lasti = ii;
            }
        } else {
            unsigned int vali = 1+(deli-stp)/8; 
            if (bruns.size()==0){
                bruns.push_back(ii); 
            } else {
                bruns.push_back(ii-last_big-1); 
            }
            bruns.push_back(vali); last_big = ii; 
        }
    }
    run_len.push_back(last_val); run_len.push_back(static_cast<unsigned int>(va_del.size())-lasti); run_len.push_back(0);

    // convert to inc/dec/abs
    std::vector<unsigned int> run_len1; unsigned int prv_val = 0;
    for (unsigned int ii=0; ii<run_len.size()/2; ii++){
        unsigned int vali = run_len[2*ii], cnti = run_len[2*ii+1];
        if (vali == prv_val+1){
            run_len1.push_back((cnti<<2)); // inc
        } else if (vali == prv_val-1){
            run_len1.push_back((cnti<<2)+1); // dec
        } else {
            run_len1.push_back((cnti<<2)+2); run_len1.push_back(vali); // abs
        }
        prv_val = vali;
    }

    // convert bruns to inc/dec/abs
    std::vector<unsigned int> bruns1; 
    for (unsigned int ii=0; ii<bruns.size()/2; ii++){
        unsigned int cnti = bruns[2*ii], vali = bruns[2*ii+1];
        if (cnti == 0){
            bruns1.push_back((vali<<2)); // inc
        } else if (cnti == 1){
            bruns1.push_back((vali<<2)+1); // +2
        } else {
            bruns1.push_back((vali<<2)+2); bruns1.push_back(cnti); // abs
        }
    }


    // -------- encode base --------
    //unsigned char *ptr3 = ptr;
    for (unsigned int ii=0; ii<run_len1.size(); ii++){
        unsigned int cur = run_len1[ii];
        if (cur<128){
            if (!hflag){
                *ptr++ = cur | 0x80;  
            } else {
                *ptr++ = (hold<<4) | (cur>>4) | 0x8;  
                hold = cur & 0x0F; 
            }
        } else if (cur<1152){ // 1.5
            cur -= 128;
            if (!hflag){
                *ptr++ = (cur>>4) | 0x40; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4) | (cur>>8) | 0x4; 
                *ptr++ = cur & 0xFF; hflag = false; 
            }
        } else if (cur<9344){ // 2
            cur -= 1152;
            if (!hflag){
                *ptr++ = (cur>>8) | 0x20; 
                *ptr++ = cur & 0xFF; 
            } else {
                *ptr++ = (hold<<4) | (cur>>12) | 0x2; 
                *ptr++ = (cur>>4) & 0xFF; 
                hold = cur & 0x0F; 
            }
        } else if (cur<74880) { // 2.5
            cur -= 9344;
            if (!hflag){
                *ptr++ = (cur>>12) | 0x10; 
                *ptr++ = (cur>>4) & 0xFF; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4) | 0x1; 
                *ptr++ = (cur>>8); 
                *ptr++ = cur & 0xFF; hflag = false; 
            }
        } else if (cur<599168) { // 3  
            cur -= 74880;
            if (!hflag){
                *ptr++ = (cur>>16) | 0x08; 
                *ptr++ = (cur>>8);
                *ptr++ = cur;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>12) | 0x80; 
                *ptr++ = (cur>>4);
                hold = cur & 0xF; 
            }
        } else if (cur<4793472) { // 3.5
            cur -= 599168;
            if (!hflag){
                *ptr++ = (cur>>20) | 0x04; 
                *ptr++ = (cur>>12);
                *ptr++ = (cur>>4);
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>16)| 0x40;
                *ptr++ = (cur>>8); 
                *ptr++ = cur; hflag = false;
            }
        } else if (cur<38347904) { // 4
            cur -= 4793472;
            if (!hflag){
                *ptr++ = (cur>>24) | 0x02; 
                *ptr++ = (cur>>16);
                *ptr++ = (cur>>8);
                *ptr++ = cur;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>20) | 0x20; 
                *ptr++ = (cur>>12);
                *ptr++ = (cur>>4);
                hold = cur & 0xF; 
            }
        } else if (cur<306783360) { // 4.5
            cur -= 38347904;
            if (!hflag){
                *ptr++ = 0x01; 
                *ptr++ = (cur>>20);
                *ptr++ = (cur>>12);
                *ptr++ = (cur>>4);
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>24) | 0x10; 
                *ptr++ = (cur>>16); 
                *ptr++ = (cur>>8); 
                *ptr++ = cur; hflag = false;
            }
        } else { // 5
            cur -= 306783360;
            if (!hflag){
                *ptr++ = 0x0; 
                *ptr++ = (cur>>24); 
                *ptr++ = (cur>>16);
                *ptr++ = (cur>>8);
                *ptr++ = cur;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>28); 
                *ptr++ = (cur>>20); 
                *ptr++ = (cur>>12); 
                *ptr++ = (cur>>4); 
                hold = cur & 0xF; 
            }
        }
    }
    if (!hflag){ // final zero
        *ptr++ = 0x80;  
    } else {
        *ptr++ = (hold<<4) | 0x8;  hold = 0; 
    }
    //unsigned int total_len3 = ptr - ptr3;

    // --------- encode brun ---------------
    //unsigned char *ptr4 = ptr;
    for (unsigned int ii=0; ii<bruns1.size(); ii++){
        unsigned int cur = bruns1[ii];
        if (cur<128){
            if (!hflag){
                *ptr++ = cur | 0x80;  
            } else {
                *ptr++ = (hold<<4) | (cur>>4) | 0x8;  
                hold = cur & 0x0F; 
            }
        } else if (cur<1152){ // 1.5
            cur -= 128;
            if (!hflag){
                *ptr++ = (cur>>4) | 0x40; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4) | (cur>>8) | 0x4; 
                *ptr++ = cur & 0xFF; hflag = false; 
            }
        } else if (cur<9344){ // 2
            cur -= 1152;
            if (!hflag){
                *ptr++ = (cur>>8) | 0x20; 
                *ptr++ = cur & 0xFF; 
            } else {
                *ptr++ = (hold<<4) | (cur>>12) | 0x2; 
                *ptr++ = (cur>>4) & 0xFF; 
                hold = cur & 0x0F; 
            }
        } else if (cur<74880) { // 2.5
            cur -= 9344;
            if (!hflag){
                *ptr++ = (cur>>12) | 0x10; 
                *ptr++ = (cur>>4) & 0xFF; 
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4) | 0x1; 
                *ptr++ = (cur>>8); 
                *ptr++ = cur & 0xFF; hflag = false; 
            }
        } else if (cur<599168) { // 3  
            cur -= 74880;
            if (!hflag){
                *ptr++ = (cur>>16) | 0x08; 
                *ptr++ = (cur>>8);
                *ptr++ = cur;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>12) | 0x80; 
                *ptr++ = (cur>>4);
                hold = cur & 0xF; 
            }
        } else if (cur<4793472) { // 3.5
            cur -= 599168;
            if (!hflag){
                *ptr++ = (cur>>20) | 0x04; 
                *ptr++ = (cur>>12);
                *ptr++ = (cur>>4);
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>16)| 0x40;
                *ptr++ = (cur>>8); 
                *ptr++ = cur; hflag = false;
            }
        } else if (cur<38347904) { // 4
            cur -= 4793472;
            if (!hflag){
                *ptr++ = (cur>>24) | 0x02; 
                *ptr++ = (cur>>16);
                *ptr++ = (cur>>8);
                *ptr++ = cur;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>20) | 0x20; 
                *ptr++ = (cur>>12);
                *ptr++ = (cur>>4);
                hold = cur & 0xF; 
            }
        } else if (cur<306783360) { // 4.5
            cur -= 38347904;
            if (!hflag){
                *ptr++ = 0x01; 
                *ptr++ = (cur>>20);
                *ptr++ = (cur>>12);
                *ptr++ = (cur>>4);
                hold = cur & 0xF; hflag = true;
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>24) | 0x10; 
                *ptr++ = (cur>>16); 
                *ptr++ = (cur>>8); 
                *ptr++ = cur; hflag = false; 
            }
        } else { // 5
            cur -= 306783360;
            if (!hflag){
                *ptr++ = 0x0; 
                *ptr++ = (cur>>24); 
                *ptr++ = (cur>>16);
                *ptr++ = (cur>>8);
                *ptr++ = cur; 
            } else {
                *ptr++ = (hold<<4); 
                *ptr++ = (cur>>28); 
                *ptr++ = (cur>>20); 
                *ptr++ = (cur>>12); 
                *ptr++ = (cur>>4); 
                hold = cur & 0xF; 
            }
        }
    }
    if (!hflag){ // final zero
        *ptr++ = 0x80;  
    } else {
        *ptr++ = (hold<<4) | 0x8;  hold = 0; 
    }
    if (hflag){
        *ptr++ = (hold<<4); hflag = false;
    } 
    //unsigned int total_len4 = ptr - ptr4;
    *compressed_mz_length = static_cast<unsigned int>(ptr - compressed);
    return true;
}

int PicoLocalCompress::writeCodesToFile(const std::vector<unsigned char> &codes, const std::vector<unsigned int> &code_idx, const std::wstring &filename)
{
    ofstream myfile;
    myfile.open (filename); 
    for (unsigned int ii=0; ii<codes.size(); ii++)
        myfile << dec << (int)code_idx[ii] << ": " << (int)codes[ii] << " - " << hex << (int)codes[ii] << endl;
    myfile.close();
    return 0;
}

bool PicoLocalCompress::writeUint2ToFile(const std::vector<unsigned int> &codes, const std::vector<unsigned int> &code_idx, const wstring &filename)
{
    ofstream myfile;
    myfile.open (filename); 
    for (unsigned int ii=0; ii<codes.size(); ii++)
        myfile << codes[ii] << "," << code_idx[ii] << endl;
    myfile.close();
    return 0;
}

int PicoLocalCompress::writeValuesToFile(const std::vector<unsigned int> &val5, const std::vector<unsigned int> &values, const std::vector<unsigned int> &code_idx, const wstring &filename)
{
    ofstream myfile;
    myfile.open (filename); 
    for (unsigned int ii=0; ii<values.size(); ii++)
        myfile << dec << (int)code_idx[ii] << ": " << "intens=" << (int)val5[ii] << ", hmap=" << (int)values[ii] << " - " << hex << (int)values[ii] << endl;
    myfile.close();
    return 0;
}

int PicoLocalCompress::writeHmapToFile(const std::vector<unsigned int> &values, const wstring &filename)
{
    ofstream myfile;
    myfile.open (filename); 
    for (unsigned int ii=0; ii<values.size(); ii++)
        myfile << dec << "index=" << ii << ": " << "intens=" << values[ii] << " - " << hex << values[ii] << endl;
    myfile.close();
    return 0;
}

bool PicoLocalCompress::writeMzsToFile(double* mzs, int length, const wstring &filename)
{
    ofstream myfile;
    myfile.open (filename); 
    for (int ii=0; ii<length; ii++)
        myfile << std::setprecision(12) << mzs[ii] << endl;
    myfile.close();
    return true;
}

bool PicoLocalCompress::writeIntensToFile(const std::vector<unsigned int> &intens, const wstring &filename)
{
    ofstream myfile;
    myfile.open (filename); 
    for (unsigned int ii=0; ii<intens.size(); ii++)
        myfile << intens[ii] << endl; 
    myfile.close();
    return 0;
}

bool PicoLocalCompress::writeMzsAndIntensToFile(const std::vector<double> &mzs, const std::vector<unsigned int> &intens,
                                                const wstring &filename)
{
    ofstream myfile;
    myfile.open (filename); 
    for (unsigned int ii=0; ii<mzs.size(); ii++)
        myfile << std::setprecision(12) << mzs[ii] << "," << intens[ii] << endl; 
    myfile.close();
    return 0;
}

bool PicoLocalCompress::writeMzsDelAndIntensToFile(const std::vector<double> &mzs, const std::vector<double> &del,
                                                   const std::vector<unsigned int> &intens, const wstring &filename)
{
    ofstream myfile;
    myfile.open (filename); 
    for (unsigned int ii=0; ii<mzs.size(); ii++)
        myfile << std::setprecision(12) << mzs[ii] << "," << del[ii] << "," << intens[ii] << endl; 
    myfile.close();
    return 0;
}

bool PicoLocalCompress:: writeUint3ToFile(const std::vector<unsigned int> &mzs,
                                          const std::vector<unsigned int> &del,
                                          const std::vector<unsigned int> &intens,
                                          const wstring &filename)
{
    ofstream myfile;
    myfile.open (filename); 
    for (unsigned int ii=0; ii<mzs.size(); ii++)
        myfile << mzs[ii] << "," << del[ii] << "," << intens[ii] << endl; 
    myfile.close();
    return 0;
}

/*
bool PicoLocalCompress::writeMzsDelIdxAndIntensToFile(std::vector<double> mzs, std::vector<double> del, std::vector<int> intens, std::vector<int> indx, string filename){
    ofstream myfile;
    myfile.open (filename); 
    for (unsigned int ii=0; ii<mzs.size(); ii++)
        myfile << std::setprecision(12) << mzs[ii] << "," << del[ii] << "," << intens[ii] << "," << indx[ii] << endl; 
    myfile.close();
    return 0;
}*/

bool PicoLocalCompress::writeDouble2ToFile(const std::vector<double> &dbl1, const std::vector<double> &dbl2,
                                           const wstring &filename)
{
    ofstream myfile;
    myfile.open (filename); 
    for (unsigned int ii=0; ii<dbl1.size(); ii++)
        myfile << std::setprecision(12) << dbl1[ii] << "," << dbl2[ii] << endl; 
    myfile.close();
    return 0;
}

int PicoLocalCompress::writeFile(const wstring &filename)
{
    ofstream myfile;
    myfile.open (filename); 
    for (unsigned int ii=0; ii<hop_dels.size(); ii++)
        myfile << hop_dels[ii] << endl;
    myfile.close();
    return 0;
}

// set lsb factor
void PicoLocalCompress::setLsbFactor(unsigned int val)
{
    LSB_FACTOR = val;
}

// clear m/z intensity limits
void PicoLocalCompress::clearMzIntensityMinMax()
{
    min_mz = 9999999.0;
    max_mz = 0.0;
    max_intensity = 0;
}

// get max intensity
long long PicoLocalCompress::getMaxIntensity() 
{
    return max_intensity;
}

// get max m/z
double PicoLocalCompress::getMaxMz() 
{
    return max_mz;
}

// get min m/z
double PicoLocalCompress::getMinMz() 
{
    return min_mz;
}

_PICO_END
