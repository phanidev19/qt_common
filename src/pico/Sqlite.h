#ifndef SQLITE_H
#define SQLITE_H

#include <stdlib.h> 
#include <assert.h> 
#include <vector>
#include "sqlite3.h"
#include "Predictor.h" 

#include <map>

class Sqlite
{
public:
    char *error;

    // Constructor:
    explicit Sqlite(const std::wstring &db_filename);

    explicit Sqlite(sqlite3* xdb);

    // Destructor:
    ~Sqlite();

    // get LSB_FACTOR
    unsigned int getLsbFactor();

    // set LSB_FACTOR
    bool setLsbFactor(unsigned int val);

    // open database
    bool openDatabase();

    // get database
    sqlite3* getDatabase();

    // get database filename
    std::wstring get_db_filename() const;

    // close database
    void closeDatabase();

    // set mode
    bool setMode();

    // get number of peak blob records
    int getPeakBlobCount();

    // get max rawid
    int getMaxRawId(std::string tablename);

    bool getNativeIdToPeaksIdMap(std::map<std::wstring,int> * nativeIdAndIdMap);

    //bool getScanNumber(std::string & nativeId, int * scanNumber);

    // Note, given 
    bool getNativeIdFromPicoByspecSpectraTableForId(unsigned int id, std::wstring * nativeId);

    // Gets a vector of all Id from Spectra table of a pico byspec. Returns true = success, false = failed
    // KELSON TO-DO: Note this looks very similar to getIds in TestPeaks (definition ln 2383 TestPeaks)
    bool getIdsFromPicoByspecSpectraTable(std::vector<int> * rows);

    // Returns full filename (could be full path, could be just filename, just whatever is in the filename field of Files table)
    bool getFilenameFromFilesTableInPicoByspec(std::wstring * filenameAbsolute);

    // If there was a full/absolute path in the Filename column of Files table, this will remove the path and just return the basename
    bool getBasenameFromFilesTableInPicoByspec(std::wstring *fileBasename);

    // ex. file root of C:\dir\file.extension is file
    bool getFileRootFromFilesTableInPicoByspec(std::wstring *fileRootname);

    bool getCommentAtId(int id, std::wstring * comment);

    bool updateSpectraCommentAtId(unsigned int id, const std::wstring & newComment);

    // get peak mz data 
    bool getPeakMzData(unsigned int scan, double** data, int *size);

    // get predicted mz data 
    bool getPredictedPeakMzData(unsigned int scan, double** data, int *size); 

    // get peak intensity data 
    bool getPeakIntensityData(unsigned int scan, float** data, int *size);

    // get reduced peak intensity data
    bool getReducedPeakIntensityData(unsigned int scan, std::vector<unsigned int> &index, std::vector<unsigned int> &intensity, int* max_intensity);

    // get intensity data 
    bool getIntensityData(unsigned int scan, std::vector<unsigned short> &intensity, int* max_intensity);

    // get UV trace data 
    bool getUvTraces(int* num_channels, float** intensity, int* length, int* initial_scan);
    // get start scan of chroma peak pairs  
    int getPeaksStartScanChromaPairs();

    // create new compressed byspec file
    bool createByspecFile(const char* db_filname);

    // create a new compressed byspec file
    bool createNewCompressedByspecFile();

    // copy uncompressed metadata to compressed byspec file
    bool createCopyByspecTablesToSqlite(const std::wstring &out_db_filname, bool newTIC);

    // create new byspec file
    bool Sqlite::createNewByspecFile();

    // write compressed mz peaks blobs to peaks table
    bool writeCompressedPeaks(int scan, unsigned char** compressed, unsigned int* length, bool empty_peaks);

    // write compressed peak blobs to peaks table
    bool writeCompressedPeaksAndTIC(int scan, unsigned char** compressed, unsigned int* length, unsigned long long intensity_sum, bool empty_peaks);

    // write compressed mz & intensity peak blobs to peaks table
    bool writeCompressedPeaks(int scan, unsigned char** compressed_mz, unsigned int* compressed_mz_length, unsigned char** compressed_intensity, unsigned int* compressed_intensity_length, bool empty_peaks);

    // write compressed peak blobs to peaks table
    bool writeCompressedPeaksAndTIC(int scan, unsigned char** compressed_mz, unsigned int* compressed_mz_length, unsigned char** compressed_intensity, unsigned int* compressed_intensity_length, unsigned long long intensity_sum, bool empty_peaks);

    // write umcompressed peak blobs to peaks table
    bool writeUncompressedPeaksToSqlite(int scan, double** restored_mz, float** restored_intensity, unsigned int* length, bool empty_peaks);

    // read compressed peak data 
    bool readCompressedPeakData(unsigned int scan, unsigned char** data, unsigned int *size);

    // read compressed peak data with mz and intensity
    bool readCompressedPeakData(unsigned int scan, unsigned char** mz_data, unsigned int *mz_size, unsigned char** intensity_data, unsigned int *intensity_size);

    // read compressed peak data with mz, intensity, and CompressionInfoId
    bool readCompressedPeakData(unsigned int scan, unsigned char** mz_data, unsigned int *mz_size, unsigned char** intensity_data, unsigned int *intensity_size, int *compressionInfoId);

    bool readCompressedPeaks_MS1CentroidedData(unsigned int scan, unsigned char** mz_data, unsigned int *mz_size, unsigned char** intensity_data, unsigned int *intensity_size, int *compressionInfoId);

    // add property value pair to peaks dictionary 
    bool addPropertyValueToPeaksDictionary(const char* property, const char* value);

    // write Files table to database
    bool writeFilesTableToSqlite(const std::wstring& raw_dir, const std::vector<std::wstring>& list, int files_id);

    // add key value pair to files info 
    bool addKeyValueToFilesInfo(int files_id, const wchar_t* key, const wchar_t* value) const;

    bool addKeyValueToFilesInfo(int files_id, const char * key, const char * value) const;

    // add key value pair to files info
    bool addKeyValueToFilesInfo(const char* key, const wchar_t* value) const;

    // add key value pair to files info
    bool addKeyValueToFilesInfo(int files_id, const wchar_t* key, int value) const;

    // append calibration info to byspec file
    bool appendCalibrationInfoToByspecFile(int files_id, const std::vector<std::string> &calib_mod);

    // get key value property from FilesInfo
    bool getKeyValuePropertyFromFilesInfo(const std::string &key, std::string * value);

    // given a Value, get the Id from FilesInfo
    bool getFilesInfoIdFromValue(const std::string &value, int * id);

    // given an Id, get the value from FilesInfo
    bool getValueFromFilesInfoId(int id, std::string * value);

    // get max_scan_id from sqlite 
    bool getMaxScanIdFunctionFromSqlite(std::vector<int> * max_scan_id);

    // add key value to a given table
    bool addPropertyValueToCompressionInfo(int files_id, const char* key, const char* value);

    // add key value pair to Info table
    bool addKeyValueToInfo(const wchar_t* key, const wchar_t* value) const;

    // add key value to a given table
    bool getPropertyValueFromCompressionInfo(const char* property, std::string &value);

    // add PeaksMS1Centroided table to byspec file
    bool addPeaksMS1CentroidedTableToByspecFile();

    // add Calibration table to byspec file
    bool addCalibrationTableToByspecFile();

    // add centroided columns to Peaks table
    bool addCentroidedColumnsToPeaksTable();

    // prepare file for read append
    bool prepareReadAppendByspecFile();


private:
    enum SqliteTableNames;

    bool readCompressedData(Sqlite::SqliteTableNames tableName, unsigned int scan, unsigned char** mz_data, unsigned int *mz_size, unsigned char** intensity_data, unsigned int *intensity_size, int *compressionInfoId);

    void _init();

    // private vars
    sqlite3* db;
    std::wstring dbfilename;
    unsigned int LSB_FACTOR;
    //pico::Predictor* pred;

    bool m_owns_db_handle;
    // private Functions
    bool insertKeyValToFilesInfoTable(int files_id, const wchar_t* key, const wchar_t* value) const;
    bool insertKeyValToFilesInfoTable(int files_id, const char * key, const char * value) const;
    bool insertKeyValToFilesInfoTable(int files_id, const wchar_t* key, int value) const;

    // strip html tag
    std::string split_tag(const std::string &str, std::string * tag);
};

#endif
