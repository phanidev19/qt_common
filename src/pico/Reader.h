#ifndef PICO_READER_H
#define PICO_READER_H

#include <ios>
#include <iostream>
#include <fstream>
#include <chrono>
#include <map>
#include <vector>
//#include <assert.h> 
#include "FunctionInfoMetaDataPrinter.h"
#include "Centroid.h"
#include "Sqlite.h"
#include "PicoCommon.h"
#include "WatersMetaReading.h"
#include "PicoUtil.h"

#include "Reader.h"
//#include "gtest/gtest.h"

#define MIN_IDX 4
#define MAX_IDX 14

//Enable this only for testing.  This is to emulate missing _function.inf file from all waters folders.
//#define TEST_MISSING__FUNCTNS_INF

extern std::wstring g_extern_inf_filename;

extern bool g_output_function_info;

extern FunctionInfoMetaDataPrinter functionInfoMetaDataPrinter;

class WatersMetaReading;
//class WatersCalibration;

_PICO_BEGIN

bool updateSpectraCommentFieldForSupernovo_Patch(sqlite3 * db);

enum ReaderType {
    Type_ReadBruker1 = 0,
    Type_ReadWaters1 = 1
};

enum ReadMode {
    Read_All = 0,
    Read_Chromatograms = 1,
};

enum kIntegrityErrType {
    kNoError = 0,
    kFileIoError = 1,  // missing or can't read
    kParseError = 2,
    kMissingExternInf = 4,
    kMissingFunctionsInf = 8,
    kMissingHeaderTxt = 16,
    kMissingDatFunction = 32,
    kMissingIdxFunction = 64,
    kMissingStsFunction = 128,
    kMissingChromInf = 256,
    kChromParseError = 512,
    kMissingChromDat = 1024,
};

// mode summary:  Peaks_MS1Centroided is only created in MS1Profile modes, either compressed or not
//     MSn (n>1) always gets centroided, except in test mode (for testing profile compression)
//     The compression (or no compression) is only applied to centroided data -- profile data is always compressed
//     Mode = 0: Centroiding=ALL;  Centroid_compression=no;  Centroid_data_insertion=yes; Profile_compression=no; MS1(centroid)(compression)+MS2(centroid)(compression)->Peaks;         no Peaks_MS1Centroided;       Spectra_insertion=yes; centroid=MS1+MS2; profile=none;
//            1: Centroiding=ALL;  Centroid_compression=yes; Centroid_data_insertion=yes; Profile_compression=no; MS1(centroid)(no_compression)+MS2(centroid)(no_compression)->Peaks;   no Peaks_MS1Centroided;       Spectra_insertion=yes; centroid=MS1+MS2; profile=none;
//            2: Centroiding=MS1;  Centroid_compression=yes; Centroid_data_insertion=yes; Profile_compression=MS1(profile)+MS2(profile)->Peaks;                  MS1(centroid)(no-compression)->Peaks_MS1Centroided;  Spectra_insertion=yes; centroid=MS1_only; profile=MS1+MS2;
//            3: Centroiding=MS1;  Centroid_compression=no;  Centroid_data_insertion=yes; Profile=MS1(profile)+MS2(profile)->Peaks, no_compression;              MS1(centroid)(compressed)->Peaks_MS1Centroided;      Spectra_insertion=yes; centroid=MS1_only; profile=MS1+MS2;
//            -- -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//            4: Centroiding=ALL;  Centroid_compression=yes; Centroid_data_insertion=yes; Profile_compression=MS1(profile)+MS2(centroid)(compression)->Peaks;    MS1(centroid)(compression)->Peaks_MS1Centroided;     Spectra_insertion=yes; centroid=MS1+MS2; profile=MS1_only;
//            5: Centroiding=ALL;  Centroid_compression=no;  Centroid_data_insertion=yes; Profile_compression=MS1(profile)+MS2(centroid)(no_compression)->Peaks; MS1(centroid)(no-compression)->Peaks_MS1Centroided;  Spectra_insertion=yes; centroid=MS1+MS2; profile=MS1_only;
//            6: Centroiding=ALL;  Centroid_compression=yes; Centroid_data_insertion=yes; Profile_compression=(MS1+MS2)(profile)(compression)->Peaks;            (MS1+MS2)(centroid)(compression)->Peaks_MS1Centroided; Spectra_insertion=yes; centroid=MS1+MS2; profile=MS1+MS2;
enum CentroidProcessing {
    Centroid_All_Uncompressed = 0,
    Centroid_All = 1,
    Centroid_MS1Profile = 2,
    Centroid_MS1Profile_Uncompressed = 3,
    Centroid_MSn = 4,
    Centroid_MSn_Uncompressed = 5, 
    Test_Profile_Compression = 6 
};

struct kFileIntegrityError {
    kIntegrityErrType err_type;
    short channel;
    short ms_level;
    kFileIntegrityError(kIntegrityErrType err_type1, short channel1, short ms_level1){
        err_type = err_type1; channel = channel1; ms_level = ms_level1;
    }
};

struct TicRec
{
    float retention_time;
    float tic_value;
    TicRec (float retention_time1, float tic_value1){
        retention_time = retention_time1; tic_value = tic_value1;
    }
};

enum FunctionType {
    FT_MS = 0, // FunctionType_Scan = 0,              /// Standard MS scanning function
    FT_SIR,    // FunctionType_SIR,                   /// Selected ion recording
    FT_DLY,    // FunctionType_Delay,                 /// No longer supported
    FT_CAT,    // FunctionType_Concatenated,          /// No longer supported
    FT_OFF,    // FunctionType_Off,                   /// No longer supported
    FT_PAR,    // FunctionType_Parents,               /// MSMS Parent scan
    FT_MSMS,   // FunctionType_Daughters,             /// MSMS Daughter scan
    FT_NL,     // FunctionType_Neutral_Loss,          /// MSMS Neutral Loss
    FT_NG,     // FunctionType_Neutral_Gain,          /// MSMS Neutral Gain
    FT_MRM,    // FunctionType_MRM,                   /// Multiple Reaction Monitoring
    FT_Q1F,    // FunctionType_Q1F,                   /// Special function used on Quattro IIs for scanning MS1 (Q1) but uses the final detector
    FT_MS2,    // FunctionType_MS2,                   /// Special function used on triple quads for scanning MS2. Used for calibration experiments.
    FT_DAD,    // FunctionType_Diode_Array,           /// Diode array type function
    FT_TOF,    // FunctionType_TOF,                   /// TOF
    FT_PSD,    // FunctionType_TOF_PSD,               /// TOF Post Source Decay type function
    FT_TOFS,   // FunctionType_TOF_Survey,            /// QTOF MS Survey scan
    FT_TOFD,   // FunctionType_TOF_Daughter,          /// QTOF MSMS scan
    FT_MTOF,   // FunctionType_MALDI_TOF,             /// Maldi-Tof function
    FT_TOFMS,  // FunctionType_TOF_MS,                /// QTOF MS scan
    FT_TOFP,   // FunctionType_TOF_Parent,            /// QTOF Parent scan
    FT_ASPEC_VSCAN,      // FunctionType_Voltage_Scan,          /// AutoSpec Voltage Scan
    FT_ASPEC_MAGNET,     // FunctionType_Magnetic_Scan,         /// AutoSpec Magnet Scan
    FT_ASPEC_VSIR,       // FunctionType_Voltage_SIR,           /// AutoSpec Voltage SIR
    FT_ASPEC_MAGNET_SIR, // FunctionType_Magnetic_SIR,          /// AutoSpec Magnet SIR
    FT_QUAD_AUTO_DAU,    // FunctionType_Auto_Daughters,        /// Quad Automated daughter scanning
    FT_ASPEC_BE,         // FunctionType_AutoSpec_B_E_Scan,     /// AutoSpec_B_E_Scan
    FT_ASPEC_B2E,        // FunctionType_AutoSpec_B2_E_Scan,    /// AutoSpec_B2_E_Scan
    FT_ASPEC_CNL,        // FunctionType_AutoSpec_CNL_Scan,     /// AutoSpec_CNL_Scan
    FT_ASPEC_MIKES,      // FunctionType_AutoSpec_MIKES_Scan,   /// AutoSpec_MIKES_Scan
    FT_ASPEC_MRM,        // FunctionType_AutoSpec_MRM,          /// AutoSpec_MRM
    FT_ASPEC_NRMS,       // FunctionType_AutoSpec_NRMS_Scan,    /// AutoSpec_NRMS_Scan
    FT_ASPEC_MRMQ        // FunctionType_AutoSpec_Q_MRM_Quad,   /// AutoSpec_Q_MRM_Quad
};

enum IonMode {
    IM_EIP = 0,
    IM_EIM,
    IM_CIP,
    IM_CIM,
    IM_FBP,
    IM_FBM,
    IM_TSP,
    IM_TSM,
    IM_ESP,
    IM_ESM,
    IM_AIP,
    IM_AIM,
    IM_LDP,
    IM_LDM,
    IM_FIP,
    IM_FIM
};

enum DataType {
    DT_COMPRESSED = 0,  // Compressed
    DT_STANDARD,        // Standard
    DT_SIR_MRM,         // SIR or MRM
    DT_SCAN_CONTINUUM,  // Scanning Contimuum
    DT_MCA,             // MCA
    DT_MCASD,           // MCA with SD
    DT_MCB,             // MCB
    DT_MCBSD,           // MCB with SD
    DT_MOLWEIGHT,       // Molecular weight data
    DT_HIAC_CALIBRATED, // High accuracy calibrated data
    DT_SFPREC,          // Single float precision (not used)
    DT_EN_UNCAL,        // Enhanced uncalibrated data
    DT_EN_CAL,          // Enhanced calibrated data
    DT_EN_CAL_ACC       // Enhanced calibrated accurate mass data
};

class Reader
{
public:
    // Constructor:
    Reader();
    Reader(int verbose1);

    // Destructor:
    ~Reader();

    // read raw file
    bool readRawFile(const std::wstring &input_db_filename, const std::wstring &out_compressed_db_filename, ReaderType type, CentroidProcessing centroid_processing, ReadMode read_mode, bool adjust_intensity, bool sort_mz, int argc, wchar_t** argv);

    // read raw Waters file to byspec
    bool readRawFileType1ToByspec(const std::wstring &raw_dir_, const std::wstring &out_db_filename, CentroidProcessing centroid_processing, bool adjust_intensity, bool sort_mz, int argc, wchar_t** argv);

    // read chromatograms from raw Waters file to byspec
    bool readChromatogramsFromRawFileType1ToByspec(const std::wstring &raw_dir_, const std::wstring &out_db_filename, CentroidProcessing centroid_type);

    // check if raw file is MSE_type, print message to console
    bool checkMseFile(std::wstring &raw_dir);
             
    // compress file 
    bool compressType1(const std::wstring &in_filenameconst, int* mz_len1, unsigned char* compressed, unsigned int* compressed_length);

    // read UV traces
    bool readUvTraces(const std::wstring &filename, int channels, std::vector<std::vector<int>> &uv_trace, int* length);

    // read UV index
    bool readUvIdx(const std::wstring &filename, int channels, std::vector<std::vector<unsigned long long>> &uv_trace, int* length);

    // copy initial part to file
    bool copyPartToFile(const std::wstring &in_filename, int length);

    // read file
    bool read(const std::wstring &in_filename, int length);

    // read file
    bool readIdx(const std::wstring &in_filename, int length);

    // read metadata file
    std::vector<std::wstring> readMetadata(const std::wstring &meta_filename);

    // read uv metadata file
    std::vector<std::wstring> readUvMetadata(const std::wstring &raw_dir);

    // read index file
    bool readIndexFile(const std::wstring &index_filename);

    // read uv_trace
    std::vector<int> readUvTraceData(const std::wstring &in_filename);

    // extract function metadata
    std::vector<std::vector<std::wstring>> extractMetadata(const std::vector<std::wstring> &metadata_content);

    // read function type
    bool readWaterFunctionTypes(const std::wstring &filename, std::vector<int> &function_types);

    // get function type
    int getWatersFunctionType(const std::wstring &raw_dir, unsigned int function_id);

    // get functions info metadata
    bool getWatersFunctionInfo(const std::wstring &raw_filename, std::vector<FunctionRec> &function_info, int *ms_func_cnt);

    // get functions info metadata using extern.inf
    bool getWatersFunctionInfoOnlyUsingExternInf(const std::wstring raw_filename, std::vector<FunctionRec> &function_info);
        
    // test function type
    int testCompressFunctionType(const std::wstring &filename);

    // identify idx size
    int identifyIdxSize(const std::wstring &raw_dir, const std::vector<std::wstring> &functions);

    // identify function type: 0=Other(e.g., UVTrace), 1=MSx, -1=error
    int identifyFunctionType(unsigned int function, const WatersMetaReading & watersMeta); // , std::string raw_dir, std::vector<std::string> list, std::vector<std::vector<std::string>> info, std::vector<std::string> functions, int idx_size, bool ms_type_6);

    // identify ms type: 0=normal, 1=ms_type_6
    int identifyFunctionMsType(const std::wstring &raw_dir, int idx_size);

    // identify function type v3c
    int identifyFunctionType_V3C(const std::wstring &raw_dir, int func_id);

    // decode type1 intensity data
    unsigned int decodeIntensityType1(unsigned int intensity);

    // get mz index
    int8_t getMzIndex(const unsigned int mz);

    // decode type1 Mz data
    double decodeMzType1(unsigned int mz);

    // decode type1 Mz_type_6 data
    double decodeMzType1_6(unsigned int mz);

    /*
    Some notes
    Type1 is Waters
    Type1_6 / ms_type_6 is specific type of format of 6 bytes per field
    Type1_6_V3C is centroid version of ms_type_6. 3 is for 3 subfields (x,y, and z, where z is a fixed constant).
    */

    // decode type1 Mz_type_6_v3c data
    double decodeMzType1_6_V3C(unsigned int mz);

    // decode type1 intensity v3c
    double decodeIntensityType1_V3C_old(unsigned int intens, const bool show_msg);

    // decode type1 intensity V3C
    double decodeIntensityType1_V3C(unsigned int intens, const std::vector<unsigned int> &v3c_tab, const bool show_msg);

    // recode type1 intensity v3c
    unsigned int recodeIntensityType1_V3C(double dbl_intens, const bool show_msg);
        
    // recode type1 intensity v3c
    unsigned int recodeMzType1_6_V3C(unsigned int mz);

    // recode type1 Mz_type_6 data
    unsigned int recodeMzType1_6(unsigned int mz);

    // build V3C lookup table
//    std::vector<unsigned int> buildV3CLookupTable(unsigned int size);

    // build lookup tables
    void buildLookupTables(std::vector<unsigned short> &dv_tab, std::vector<unsigned int> &mz_base, std::vector<double> &mz_tab);

    // restore zeros
    bool restoreZeros(unsigned int* packed_mz, int packed_mz_length, std::vector<unsigned short> &dv_tab, std::vector<unsigned int> &mz_base, std::vector<double> &mz_tab, std::vector<unsigned int> &restored_zeros_mz);

    // read calibration data from metadata
    bool readCalibrationDataFromMetadataFile(const std::wstring &raw_dir, std::vector<std::vector<std::string>> * calib_metadata, std::vector<std::vector<std::string>> * calib_coeff, std::vector<std::string> * calib_modf);

    // parse calibration std::string
    bool parseCalibrationString(const std::string &coeff_str, int* type, std::vector<double> &coeffs);

    // write tic to file
    bool writeTicToFile(const std::vector<TicRec> tic, const std::wstring &out_filename);

    // check raw file integrity
    void checkRawFileType1Integrity(std::wstring &raw_dir, std::vector<kFileIntegrityError> & errs);

    // get file size (in bytes)
    long long getFileSize(const std::wstring &in_filename);

    // set verbose level
    void setVerboseLevel(int verbose_level1);

    // set uncertainty scaling
    void setUncertaintyScaling(UncertaintyScaling uncert_type1);

    // get uncertainty scaling
    UncertaintyScaling getUncertaintyScaling();

    // set uncertainty value
    bool setUncertaintyValue(double uncert_val1);

    // get uncertainty scaling
    double getUncertaintyValue();

    // set merge radius
    bool setMergeRadius(double merge_factor1);

    // get merge radius
    double getMergeRadius();

    // set uncertainty value merge_radius
    bool setTopK(int TopK1);

    // get uncertainty scaling
    int getTopK();

    // set intact mass file
    void setIdentifyFunctionMsTypeExpectNoZeros(bool intact_mode1);

    // get intact mass file
    bool getIdentifyFunctionMsTypeExpectNoZeros() const;

    // build V3C lookup table
    std::vector<unsigned int> buildV3CLookupTable(const unsigned int size);

    //check for MSE file
    bool checkMSE(const std::vector<std::vector<std::wstring>> info);

    // check cal offset
    bool checkCalOffset(FILE* inFile, const unsigned int mz_len);

    enum FileIntegrityResult {
        FileIntegrityResult_Sucess = 0, FileIntegrityResult_Warning, FileIntegrityResult_Error
    };

    static std::wstring toString(FileIntegrityResult val);

    // report integrity errors
    void error_message(const std::vector<kFileIntegrityError> &errs, FileIntegrityResult * result);

    // test decode intensity
    bool test_decodeIntensity_v3c(unsigned int raw_intens, std::vector<unsigned int> &v3c_tab);

    // identify function mode
    bool identifyFunctionMode(unsigned int function, int max_scan, int mz_len, bool f_type_v3c, bool ms_type_6, bool cal_offset,
        const std::wstring &raw_dir, const std::vector<unsigned int> &v3c_tab, const unsigned short *fact_tab, const unsigned char* idx_buf, int idx_size, FILE* inFile, CentroidMode *centroid_mode);

    // disable profile centroiding
    void disableProfileCentroid();

private:
    // read chro metadata
    bool readChromsMetadata(const std::wstring &raw_dir, std::vector<std::vector<std::pair<std::wstring, std::wstring>>> &chroms_metadata);

    // get reference function index
    int getReferenceFunctionIndex(const std::vector<std::vector<std::wstring>>& info);

    // check sts_override
    bool checkStsOverride(const unsigned char* pts, const int sts_count, const int sts_file_size, const bool ms_type_6);

    // recode type1 Mz_type_6 data
//    unsigned int recodeMzType1_6(unsigned int mz);

    // check index size
    bool checkIndexSize(const std::wstring &raw_dir, std::vector<int> &results);

    // find channel name
    std::size_t findChromatogramChannelName(const std::wstring &property, std::wstring * chan) const;
        
    int top_k; // = -1;
    double uncert_val; // = 0.01; 
    double merge_radius; // = 4*uncert_val
    double end_uncert_val; // = uncert_val
    UncertaintyScaling uncert_type; // = ConstantUncertainty;
    bool m_identifyFunctionMsTypeExpectNoZeros; // intact file type -- is turned on by default for any file, might want to turn off for non-intact files
    bool disable_profile_centroiding;
    int verbose;
};

inline void structured_output(const std::wstring & tag, const std::wstring & messsage) {
    std::wcerr << L"<" + tag + L">" + messsage + L"</" + tag + L">" << std::endl;
}

_PICO_END

#endif
