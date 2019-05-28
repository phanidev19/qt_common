// ********************************************************************************************
// PicoLocalCompress.h : The main compression module
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
// PicoLocalCompress::compressSpectra(Sqlite* sql, int scan, unsigned char** compressed, unsigned int* length)
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

#ifndef PICO_COMPRESS_H
#define PICO_COMPRESS_H

#include <ios>
#include <iostream>
#include <map>
#include <vector>
#include <assert.h>
#include "Sqlite.h"
#include "PicoCommon.h"
# include <chrono>

_PICO_BEGIN

struct Peak
{
    double mz;
    unsigned int intensity;

    Peak (double mz1, unsigned int intensity1){
        mz = mz1; intensity = intensity1;
    }
};

struct PeakF
{
    double mz;
    float intensity;

    PeakF (double mz1, float intensity1){
        mz = mz1; intensity = intensity1;
    }
};

struct CntRecord
{
    unsigned int count;
    unsigned int value;
    unsigned int idx;

    CntRecord(unsigned int cnt, unsigned int val, unsigned int indx)
        : count(cnt), value(val), idx(indx)
    {
    }
};

struct CntRecordD
{
    unsigned int count;
    double value;
    unsigned int idx;

    CntRecordD (unsigned int cnt, double val, unsigned int indx){
        count = cnt; value = val; idx=indx;
    }
};

class PicoLocalCompress
{
public:
    // Constructor:
    PicoLocalCompress();
    PicoLocalCompress(bool verbose1);

    // Destructor:
    ~PicoLocalCompress();

    // compress file
    bool compress(const std::wstring &input_db_filename, const std::wstring &out_compressed_db_filename, CompressType type, int single_scan);

    // initialize raw to sqlite compression
    bool initialize_raw2sqlite(const std::wstring &input_db_filename, const std::wstring &out_compressed_db_filename, Sqlite** sql_out);
    // add spectra to compressed sqlite file
    bool add_spectra2sqlite(int peaksId, const double* uncompressed_mz, const double* uncompressed_intensity, int uncompressed_size, Sqlite* sql_out);
    // close the output sqlite file
    void close_raw2sqlite(Sqlite* sql_out);

    // compress raw type 0 binary data
    bool compressRawType0(const double* uncompressed_mz, const double* uncompressed_intensity, int uncompressed_length, unsigned char** compressed, unsigned int* compressed_length,
        unsigned long long* intensity_sum);

    // compress raw type 02 intensity
    bool compressRawType02(const double* uncompressed_mz, const float* uncompressed_intensity, int mz_len1, unsigned char** compressed, unsigned int* compressed_length, unsigned long long* intensity_sum);

    // compress raw type 1 binary data
    bool compressRawType1(const double* uncompressed_mz, const float* uncompressed_intensity, int uncompressed_length, unsigned char** compressed_mz, unsigned int* compressed_mz_length,
        unsigned char** compressed_intensity, unsigned int* compressed_intensity_length, unsigned long long* intensity_sum);
    // compressor raw type 1 binary data unsigned integer data
    bool compressRawType1_I(const double* uncompressed_mz, const unsigned int* uncompressed_intensity, int uncompressed_mz_length, unsigned char** compressed_mz, unsigned int* compressed_mz_length,
        unsigned char** compressed_intensity, unsigned int* compressed_intensity_length, unsigned long long* intensity_sum);
    // compress raw type 1 binary data (Waters)
    bool compressRawType1_N(const std::vector<unsigned int> &uncompressed_buf, int uncompressed_length, unsigned char* compressed, unsigned int* compressed_mz_length, unsigned long long* intensity_sum, const std::vector<double> &calib_coeff_, bool ms_type_6);

    // compress raw type centroided 1 binary data
    bool compressRawTypeCentroided1(const double* uncompressed_mz, const float* uncompressed_intensity, int uncompressed_length, unsigned char** compressed_mz, unsigned int* compressed_mz_length,
        unsigned char** compressed_intensity, unsigned int* compressed_intensity_length, unsigned long long* intensity_sum, bool apply_peak_sorting);

    // compress raw type 4 binary data (ABSciex)
    bool compressRawType4(const double* uncompressed_mz, const float* uncompressed_intensity, int uncompressed_length, unsigned char* compressed, unsigned int* compressed_mz_length, unsigned long long* intensity_sum);

    // compress one type 0 spectra
    bool compressSpectraType0(Sqlite* sql, int scan, unsigned char** compressed, unsigned int* length);

    // compress mz type 1
    bool compressMzType1(const double* mz_blob, int mz_len, unsigned char* compressed, unsigned int* compressed_length);

    // compress type 1 intensity unsigned integer input
    bool compressIntensityType1_I(const unsigned int* intens_blob, int mz_len1, unsigned char** compressed, unsigned int* compressed_length, unsigned long long* intensity_sum);

    // set lsb factor
    void setLsbFactor(unsigned int val);

    // clear m/z intensity limits
    void clearMzIntensityMinMax();

    // get max intensity
    long long getMaxIntensity();

    // get max m/z
    double getMaxMz();

    // get min m/z
    double getMinMz();

    bool writeMzsToFile(double* mzs, int length, const std::wstring &filename);

private:
    // private vars
    int mz_len;
    double max_mz, min_mz;
    long long max_intensity;
    unsigned int LSB_FACTOR;
    std::vector<unsigned int> hvals, hop_dels;

    std::map<unsigned int, unsigned int> hmap;
    std::vector<CntRecord> intens_recs;

    std::map<unsigned int, unsigned int> hop_map;
    std::vector<CntRecord> hop_recs;

    bool verbose;

    long long rd_mz_millis, rd_intens_millis, compress_millis;

    // private functions
    int writeFile(const std::wstring &filename);
    bool writeIntensToFile(const std::vector<unsigned int> &intens, const std::wstring &filename);
    bool writeMzsAndIntensToFile(const std::vector<double> &mzs, const std::vector<unsigned int> &intens, const std::wstring &filename);
    bool writeMzsDelAndIntensToFile(const std::vector<double> &mzs, const std::vector<double> &del, const std::vector<unsigned int> &intens, const std::wstring &filename);
    bool writeMzsDelIdxAndIntensToFile(const std::vector<double> &mzs, const std::vector<double> &del, const std::vector<int> &intens, const std::vector<int> indx, const std::wstring &filename);
    int writeHmapToFile(const std::vector<unsigned int> &values, const std::wstring &filename);
    int writeCodesToFile(const std::vector<unsigned char> &codes, const std::vector<unsigned int> &code_idx, const std::wstring &filename);
    int writeValuesToFile(const std::vector<unsigned int> &val5, const std::vector<unsigned int> &values, const std::vector<unsigned int> &code_idx, const std::wstring &filename);
    bool writeUint2ToFile(const std::vector<unsigned int> &codes, const std::vector<unsigned int> &code_idx, const std::wstring &filename);
    bool writeUint3ToFile(const std::vector<unsigned int> &mzs, const std::vector<unsigned int> &del, const std::vector<unsigned int> &intens, const std::wstring &filename);
    bool writeDouble2ToFile(const std::vector<double> &dbl1, const std::vector<double> &dbl2, const std::wstring &filename);

    // compress type 0 file
    bool compressType0(const std::wstring &input_db_filename, const std::wstring &out_compressed_db_filename);

    // compress type 02 file
    bool compressType02(const std::wstring &input_db_filename, const std::wstring &out_compressed_db_filename);

    // compress type 1 file
    bool compressType1(const std::wstring &input_db_filename, const std::wstring &out_compressed_db_filename, int single_scan);

    // compress centroided type file
    bool compressCentroided(const std::wstring &input_db_filename, const std::wstring &out_compressed_db_filename, int single_scan);

    // compress type 4 file (ABSciex)
    bool compressType4(const std::wstring &input_db_filename, const std::wstring &out_compressed_db_filename, int single_scan);

    // read spectra
    bool readSpectra(Sqlite* sql, int scan, double** data, std::vector<unsigned int> &intensity_index,
                     std::vector<unsigned int> &intensity, int* max_intensity);

    // hash spectra
    void createSpectraHash(const std::vector<unsigned int> &intensity);

    // hop spectra
    void createHopHash(const std::vector<unsigned int> &intensity_index);

    // combined spectra hash
    void createCombinedSpectraHash(const std::vector<unsigned int> &intensity);

    // create local dictionary
    unsigned char* createLocalDictionary(unsigned char* compressed, unsigned int mz_len, const double* data);

    // create local dictionary
    unsigned char* createLocalDictionaryType02(unsigned char* compressed, unsigned int mz_len, const double* data);

    // generate indices
    unsigned char* generateIndices(const std::vector<unsigned int> &intensity, unsigned char* ptr);

    // create combined dictionary
    unsigned char* createCombinedDictionary(unsigned char* compressed, unsigned int mz_len);//, const double* data);

    // create global mz  dictionary
    bool createGlobalMzDict1(Sqlite* sql_in, unsigned char** dict, unsigned int* length);

    // compress type 1 intensity
    bool compressIntensityType1(const float* intens_blob, int mz_len, unsigned char** compressed, unsigned int* compressed_length, unsigned long long* intensity_sum);

    // generate combined indices
    unsigned char* generateCombinedIndices(const std::vector<unsigned int> &intensity, unsigned char* ptr2);
};

_PICO_END

#endif
