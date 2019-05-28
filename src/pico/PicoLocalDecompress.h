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
// PicoLocalDecompress::decompressSpectra(unsigned char** compressed, unsigned int* compressed_length, 
//         double** restored_mz, float** restored_intensity, unsigned int* length)
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

#ifndef PICO_DECOMPRESS_H
#define PICO_DECOMPRESS_H

#include <ios>
#include <iostream>
#include <map>
#include <vector>
#include <assert.h> 
#include "PicoCommon.h"
#include "WatersCalibration.h"

class Sqlite;

#define ZERO_MZ_SNAP 300
#define DEFAULT_ZERO_GAP 0.01
#define MAX_ZERO_GAP 0.11 //0.025

_PICO_BEGIN

class PicoLocalDecompressMemoryOnly {
    PicoLocalDecompressMemoryOnly();

    bool decompress_mz(const unsigned char * compressed, int compressed_length,
        unsigned char ** uncompressed_mz, unsigned char ** uncompressed_inten, int * uncompressed_length);
};

enum CompressionType
{
    Unknown = 0,
    Bruker_Type0 = 1,
    Centroid_Type1 = 2,
    Waters_Type1 = 3
};

class PicoLocalDecompress
{
public:
    // Constructor:
    PicoLocalDecompress();

    // constructor
    PicoLocalDecompress(bool verbose1);

    // Destructor:
    ~PicoLocalDecompress();

    // decompress file
    bool decompress(const std::wstring &input_compressed_db_filename, const std::wstring &out_uncompressed_db_filename, CompressType type, int single_scan, bool restore_zero_peaks);

    // decompress type 0 spectra memory to memory
    bool decompressSpectraType0(unsigned char** compressed, double** restored_mz, float** restored_intensity, int* size) const;

    // decompress type 02 spectra memory to memory
    bool decompressSpectraType02(unsigned char** compressed, double** restored_mz, float** restored_intensity, int* length);

    // decompress type 1 spectra memory to memory
    bool decompressSpectraType1(unsigned char** compressed_mz, unsigned char** compressed_intensity, const WatersCalibration::Coefficents *cal_mod_coef, int compress_info_id, unsigned int compressed_length, double** restored_mz, float** restored_intensity, int* restored_mz_length, bool restore_zero_peaks, const bool file_decompress);

    // decompress centroided type 1 spectra memory to memory
    bool decompressSpectraCentroidedType1(unsigned char** compressed_mz, unsigned char** compressed_intensity, double** restored_mz, float** restored_intensity, int* length) const;

    // decompress type 4 spectra memory to memory (ABSciex)
    bool decompressRawType4(unsigned char* compressed_mz, int compressed_mz_length, double** restored_mz, float** restored_intensity, int* restored_mz_length, bool restore_zero_peaks) const;

    // decompress raw type 1 memory to memory (Waters)
    bool decompressRawType1_N(unsigned char* compressed_mz, const WatersCalibration::Coefficents *cal_mod_coef, double** restored_mz, float** restored_intensity, int* restored_mz_length, bool restore_zero_peaks, const bool file_decompress) const;

    // get compression info type
    bool getCompressionInfoType(const std::wstring &input_compressed_db_filename, std::string &compression_type);

    // decode and calibrate mz
    double decodeAndCalibrateMzType1(unsigned int mz, const std::vector<double> &calib_coefi, const WatersCalibration::Coefficents *cal_mod_coef) const;
    double decodeAndCalibrateMzType1_old(unsigned int mz, const std::vector<double> &calib_coefi) const;

    bool test_decompress_pico_byspec(const std::wstring &input_compressed_db_filename, const std::string &output_filename);

private:
    // private vars
    //int mz_len;
    //unsigned int LSB_FACTOR;

    bool verbose;

    // private functions
    // decompress type 0 file
    bool decompressType0(const std::wstring &input_compressed_db_filename, const std::wstring &out_uncompressed_db_filename);

    // decompress type 02 file
    bool decompressType02(const std::wstring &input_compressed_db_filename, const std::wstring &out_uncompressed_db_filename);

    // decompress type 1 file
    bool decompressType1(const std::wstring &input_compressed_db_filename, const std::wstring &out_uncompressed_db_filename, int single_scan, bool restore_zero_peaks);

    // decompress centroided type 1 file
    bool decompressTypeCentroided1(const std::wstring &input_compressed_db_filename, const std::wstring &out_uncompressed_db_filename, int single_scan);

    // decompress type 4 file (ABSciex)
    bool decompressType4(const std::wstring &input_compressed_db_filename, const std::wstring &out_uncompressed_db_filename, int single_scan, bool restore_zero_peaks);

    // decompress type 0 spectra
    //bool decompressSpectraType0(unsigned char** compressed, double** restored_mz, float** restored_intensity, int* length);

    // decompress mz Type1 
    bool decompressMzType1(unsigned char** compressed, double** restored_mz, int* length) const;

    // decompress intensity Type1 
    bool decompressIntensityType1(unsigned char** compressed, float** restored_intensity, int* length) const;

    // decompress mz centroided Type1 
    bool decompressMzCentroidedType1(unsigned char** compressed, double** restored_mz, int* length) const;

    // decompress intensity centroided Type1 
    bool decompressIntensityCentroidedType1(unsigned char** compressed, float** restored_intensity, int* length) const;

    // parse calibration std::string
    bool parseCalibrationString(const std::string &coeff_str, std::vector<double> &coeffs);

    int writeFile(const std::vector<int> &hop_del, const std::wstring &filename);

    // write spectra
    bool writeSpectra(Sqlite* sql, int scan, std::vector<unsigned int> &intensity_index, std::vector<unsigned int> &intensity, int* max_intensity, CompressType type);
};

_PICO_END

#endif
