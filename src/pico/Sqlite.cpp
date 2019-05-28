#include <stdlib.h>
#include <string.h>
#include <stdio.h>
//#include <time.h>
#include <ctime>
//#include <locale>
//#include <ios>
#include <iostream>
#include <sstream>
#include <codecvt>
#include "sqlite3.h"
#include "Sqlite.h"
#include "Predictor.h"
#include "PicoUtil.h"
#include <sqlite_utils.h>
#include "MsCompressGitVersionAuto.h"

#define byspec_version 6

using namespace std;
using namespace pico;

enum Sqlite::SqliteTableNames
{
    UNKNOWN = 0,
    PEAKS,
    PEAKS_MS1CENTROIDED,
};

//default values called by constructor
void Sqlite::_init()
{
    LSB_FACTOR = 4;
    db = NULL;
    //pred = NULL;
    m_owns_db_handle = false;
}

// constructor
Sqlite::Sqlite(const std::wstring &db_filename)
{
    _init();
    dbfilename = db_filename;  // save the filename
    m_owns_db_handle = true;
}

Sqlite::Sqlite(sqlite3* xdb)
{
    _init();
    db = xdb;
    m_owns_db_handle = false;
}

// destructor
Sqlite::~Sqlite()
{
    //if (!pred)
    //    delete pred;
}

// get LSB_FACTOR
unsigned int Sqlite::getLsbFactor()
{
    return LSB_FACTOR;
}

// set LSB_FACTOR
bool Sqlite::setLsbFactor(unsigned int val)
{
    if (val>0 && val<8) {
        LSB_FACTOR = (unsigned int)pow(2, (float)val);
    } else {
        cout << "out of range value= " << val << " (range = 1..7): LSB_FACTOR is a power-of-two number "; return false;
    }
    return true;
}

// open database, create new if not exist
bool Sqlite::openDatabase()
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    if (!dbfilename.empty() && sqlite3_open(conv.to_bytes(dbfilename).c_str(), &db) != SQLITE_OK)
    {
        cout << "can't open sqlite database: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return false;
    }
    m_owns_db_handle = true;
    return true;
}

// get database filename
std::wstring Sqlite::get_db_filename() const
{
    return dbfilename;
}

// get database filename
sqlite3* Sqlite::getDatabase()
{
    return db;
}

// close database
void Sqlite::closeDatabase()
{
    if (m_owns_db_handle && db != NULL) {
        sqlite3_close(db); db = NULL;
    } else {
        cerr << "Cannot close database handle as this instance does not own it." << endl;
    }
}

// set mode
bool Sqlite::setMode()
{
    if (db == NULL) {
        cerr << "Database handle NULL" << endl;
        return false;
    }
/*    ostringstream cmd; cmd << "PRAGMA journal_mode=OFF;";
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    int rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't set journal mode -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "PRAGMA page_size=SQLITE_MAX_DEFAULT_PAGE_SIZE;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't set page size -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "PRAGMA synchronous=OFF;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't set sync mode -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "PRAGMA locking_mode=EXCLUSIVE;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't set lock mode -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "PRAGMA cache_size=20000;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't set cache size -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);*/
    return true;
}

// get number of blob records in peaks
int Sqlite::getPeakBlobCount()
{
    sqlite3_stmt *stmt = NULL;
    int count = -1;
    const char *sqlCmd = "Select max(rowid) FROM Peaks;";
    if (sqlite3_prepare_v2(db, sqlCmd, -1, &stmt, NULL) != SQLITE_OK){
        string val = string(sqlite3_errmsg(db));
    } else {
        //sqlite3_bind_int(st, 1, id);
        while (true) {
            if (sqlite3_step(stmt) == SQLITE_ROW){
                count = sqlite3_column_int (stmt, 0);
                break;
            } else {
                string errmsg(sqlite3_errmsg(db)); break;
                break;
            }
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    return count;
}

// get max rawid
int Sqlite::getMaxRawId(std::string tablename)
{
    sqlite3_stmt *stmt = NULL;
    int count = -1;
    ostringstream cmd; cmd << "Select max(rowid) FROM " << tablename << ";";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) != SQLITE_OK){
        string val = string(sqlite3_errmsg(db));
    } else {
        //sqlite3_bind_int(st, 1, id);
        while (true) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
                break;
            } else {
                string errmsg(sqlite3_errmsg(db)); break;
                break;
            }
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    return count;
}

bool Sqlite::getNativeIdToPeaksIdMap(std::map<std::wstring, int> * nativeIdAndIdMap) {
    sqlite3_stmt *stmt = NULL; bool success = false; int rc = 0;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    // Select only function 1 otherwise won't work with n mode 6 for some reason (n mode 6 is missing function 2 in peaks table for file 07jan for branch 1311)
    ostringstream cmd; cmd << "SELECT NativeId, PeaksId FROM Spectra WHERE NativeId LIKE 'function=%'";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                std::wstring nativeId = reinterpret_cast<const wchar_t*>(sqlite3_column_text(stmt, 0));
                int peaksId = sqlite3_column_int(stmt, 1);
                nativeIdAndIdMap->insert(std::pair<std::wstring,int>(nativeId, peaksId));
            }
            else if (rc == SQLITE_DONE) {
                success = true;
                break;
            }
            else {
                success = false;
                break;
            }
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "rc=" << rc << "can't read native id / peaks id from sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

//bool Sqlite::getScanNumber(std::string &nativeId, int * scanNumber) {
//    nativeId->clear();
//
//    sqlite3_stmt *stmt = NULL; bool success = false; int rc = 0;
//    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
//    ostringstream cmd; cmd << "Select NativeId FROM Spectra WHERE Id == '" << row << "'";
//    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
//        rc = sqlite3_step(stmt);
//        if (rc == SQLITE_ROW){
//            sqlite3_bind_int(stmt, 0, *scanNumber);
//        }
//    }
//}

bool Sqlite::getNativeIdFromPicoByspecSpectraTableForId(unsigned int id, std::wstring * nativeId)
{
    nativeId->clear();

    sqlite3_stmt *stmt = NULL;
    bool success = false;
    int returnCode = 0;
    ostringstream cmd;
    cmd << "SELECT NativeId FROM Spectra WHERE Id=" << id;
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            returnCode = sqlite3_step(stmt);
            if (returnCode == SQLITE_ROW){
                *nativeId = reinterpret_cast<const wchar_t*>(sqlite3_column_text16(stmt, 0));
            } else if (returnCode == SQLITE_DONE) {
                success = true;
                break;
            } else {
                success = false;
                break;
            }
        }
    }
    if (stmt) {
        sqlite3_finalize(stmt);
    }

    if (!success) {
        std::cerr << "returnCode" << returnCode << "couldn\'t read native id from sqlite -- "
                << string(sqlite3_errmsg(db)) << std::endl;
    }
    return success;
}

bool Sqlite::getIdsFromPicoByspecSpectraTable(std::vector<int> * rows)
{
    rows->clear();

    bool success = false;

    sqlite3_stmt * stmt;
    ostringstream cmd;
    cmd << "Select Id FROM Spectra";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int returnCode = sqlite3_step(stmt);
            if (returnCode == SQLITE_ROW){
                int id = sqlite3_column_int(stmt, 0);
                rows->push_back(id);
            } else if (returnCode == SQLITE_DONE) {
                success = true;
                break;
            } else {
                std::cerr << "Error on returnCode=" << returnCode << std::endl;
                break;
            }
        }
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
    return success;
}

bool Sqlite::getFilenameFromFilesTableInPicoByspec(std::wstring *filenameAbsolute)
{
    bool success = false;
    filenameAbsolute->clear();

    sqlite3_stmt *stmt = NULL;
    int returnCode = 0;

    ostringstream cmd;
    cmd << "Select Filename FROM Files WHERE Id == 1";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        returnCode = sqlite3_step(stmt);
        if (returnCode == SQLITE_ROW){
            *filenameAbsolute = static_cast<const wchar_t*>(sqlite3_column_text16(stmt, 0));
            success = true;
        }
    }

    if (stmt) {
        sqlite3_finalize(stmt);
    }
    if (!success) {
        std::cerr << "returnCode=" << returnCode << "couldn't read filenameAbsolute from Files table -- " << string(sqlite3_errmsg(db)) << std::endl;
    }
    return success;
}

bool Sqlite::getBasenameFromFilesTableInPicoByspec(std::wstring *fileBasename)
{
    std::wstring filenameAbsolute;
    bool success = this->getFilenameFromFilesTableInPicoByspec(&filenameAbsolute);

    PicoUtil utl;

    // If there is a pathChar in the end, we don't want it anymore (because there should never be a directory path here,
    // e.g. there should never be: C://example//folder//, and any ending path char is extraneous
    filenameAbsolute = utl.stripPathChar(filenameAbsolute);
    *fileBasename = utl.getNameNoSlashes(filenameAbsolute);

    return success;
}

bool Sqlite::getFileRootFromFilesTableInPicoByspec(std::wstring *fileRootname)
{
    std::wstring fileBasename;
    bool success = this->getBasenameFromFilesTableInPicoByspec(&fileBasename);

    PicoUtil utl;
    *fileRootname = utl.removeFileExtension(fileBasename);

    return success;
}

// UPDATE Spectra SET Comment="Kelson" WHERE Id = 1
bool Sqlite::updateSpectraCommentAtId(unsigned int Id, const std::wstring & newComment)
{
    bool success = false;

    sqlite3_stmt *stmt = NULL;
    int returnCode = 0;

    wostringstream cmd;
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    cmd << "UPDATE Spectra SET Comment=\'" << newComment << "\' WHERE Id= " << to_wstring(Id);
    if (returnCode = sqlite3_prepare_v2(db, conv.to_bytes(cmd.str()).c_str(), -1, &stmt, NULL) == SQLITE_OK){
        returnCode = sqlite3_step(stmt);
        if (returnCode == SQLITE_ROW || returnCode == SQLITE_DONE){
            success = true;
        }
    }

    if (stmt) {
        sqlite3_finalize(stmt);
    }
    if (!success) {
        std::cerr << "returnCode=" << returnCode << "couldn't read filenameAbsolute from Files table -- " << string(sqlite3_errmsg(db)) << std::endl;
    }
    return success;
}

// Note: implementation incredibly similar to get native id
bool Sqlite::getCommentAtId(int id, std::wstring * comment)
{
    comment->clear();

    sqlite3_stmt *stmt = NULL;
    bool success = false;
    int returnCode = 0;
    ostringstream cmd;
    cmd << "SELECT Comment FROM Spectra WHERE Id=" << id;
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            returnCode = sqlite3_step(stmt);
            if (returnCode == SQLITE_ROW){
                *comment = reinterpret_cast<const wchar_t*>(sqlite3_column_text16(stmt, 0));
            } else if (returnCode == SQLITE_DONE) {
                success = true;
                break;
            } else {
                success = false;
                break;
            }
        }
    }

    if (stmt) {
        sqlite3_finalize(stmt);
    }

    if (!success) {
        std::cerr << "returnCode" << returnCode << "couldn\'t read comment from sqlite -- "
        << string(sqlite3_errmsg(db)) << std::endl;
    }
    return success;
}


// get peak mz data
bool Sqlite::getPeakMzData(unsigned int scan, double** data, int *size)
{
    sqlite3_stmt *stmt = NULL; bool success = false; int rc = 0;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "Select PeaksMz FROM Peaks WHERE Id == '" << scan << "'";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        //while (true) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                *size = sqlite3_column_bytes(stmt, 0)/8;
                *data = (double *)malloc(*size*sizeof(double));
                char* blob = (char*)sqlite3_column_blob(stmt, 0);
                memcpy((char*)*data, blob, *size*8);
                success = true;
            }
        // end while
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "rc=" << rc << "can't read mz blob from sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// get predicted peak mz data
bool Sqlite::getPredictedPeakMzData(unsigned int scan, double** data, int *size)
{
    double* vect = (double *)calloc(11, sizeof(double));
    if (!vect){
        cerr << "vect allocation failed" << endl; return false;
    }
    double* sx = &vect[0], *sy = &vect[7];
    sqlite3_stmt *stmt=NULL; bool success = false; int rc = 0;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "Select PeaksMz FROM Peaks WHERE Id == '" << scan << "'";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            //if (sqlite3_step(stmt) == SQLITE_ROW){
            *size = sqlite3_column_bytes(stmt, 0)/8; sx[0]=*size;
            double* blob = (double*)sqlite3_column_blob(stmt, 0);
            for (int ii=0; ii<*size; ii++){
                double yi = blob[ii], xpi = 1; sy[0] += yi;
                for (int jj=1; jj<7; jj++){
                    xpi *= ii; sx[jj] += xpi;
                    if (jj<4) sy[jj] += xpi*yi;
                }
            }
            Predictor* pred1 = new Predictor(3);
            pred1->initialize(vect);
            *data = pred1->predict();
            delete pred1;
            success = true;
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    free_safe(vect);
    if (!success)
        cout << "rc=" << rc << ": can't read mz blob from sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// get peak intensity data
bool Sqlite::getPeakIntensityData(unsigned int scan, float** data, int *size)
{
    sqlite3_stmt *stmt=NULL; bool success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "Select PeaksIntensity FROM Peaks WHERE Id == '" << scan << "'";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        //while (true) {
            if (sqlite3_step(stmt) == SQLITE_ROW){
                *size = sqlite3_column_bytes(stmt, 0)/4;
                *data = (float *)malloc(*size*sizeof(float));
                char* blob = (char*)sqlite3_column_blob(stmt, 0);
                memcpy((char*)*data, blob, *size*4);
                success = true;
            }
        // end while
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't read mz blob from sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// get reduced peak intensity data
bool Sqlite::getReducedPeakIntensityData(unsigned int scan, std::vector<unsigned int> &index, std::vector<unsigned int> &intensity, int* max_intensity)
{
    index.clear(); intensity.clear(); int max = 0;
    sqlite3_stmt *stmt=NULL; bool success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "Select PeaksIntensity FROM Peaks WHERE Id == '" << scan << "'";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            int size = sqlite3_column_bytes(stmt, 0)/4;
            float* blob = (float*)sqlite3_column_blob(stmt, 0);
            for (int ii=0; ii<size; ii++){
                float yi = blob[ii];
                if (yi>0.0){
                    int val = (int)yi/LSB_FACTOR; if (val>max) max = val;
                    intensity.push_back(val); index.push_back(ii);
                }
            }
            *max_intensity = max;
            success = true;
        } else
            cout << "rc=" << rc << endl;
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't read mz blob from sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// get intensity data
bool Sqlite::getIntensityData(unsigned int scan, std::vector<unsigned short> &intensity, int* max_intensity)
{
    intensity.clear(); int max = 0;
    sqlite3_stmt *stmt=NULL; bool success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "Select PeaksIntensity FROM Peaks WHERE Id == '" << scan << "'";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            int size = sqlite3_column_bytes(stmt, 0)/4;
            float* blob = (float*)sqlite3_column_blob(stmt, 0);
            for (int ii=0; ii<size; ii++){
                float yi = blob[ii];
                //if (yi>0.0){
                    int val = (int)yi; if (val>max) max = val;
                    intensity.push_back((unsigned short)val); //index.push_back(ii);
                //}
            }
            *max_intensity = max;
            success = true;
        } else
            cout << "rc=" << rc << endl;
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't read mz blob from sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// get start scan of chroma peak pairs
int Sqlite::getPeaksStartScanChromaPairs()
{
    int start_scan = -1;
    sqlite3_stmt *stmt=NULL; bool success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "Select Id FROM Peaks WHERE PeaksCount = 2";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            start_scan = sqlite3_column_int(stmt, 0);
            success = true;
        } else
            cout << "rc=" << rc << endl;
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success){
        cout << "can't read Peaks data from sqlite -- " << string(sqlite3_errmsg(db)) << endl;
        return -1;
    }
    return start_scan;
}


/*
CREATE TABLE Files(Id INTEGER PRIMARY KEY,Filename TEXT,Location TEXT,Type TEXT,Signature TEXT)
CREATE TABLE FilesInfo(Id INTEGER PRIMARY KEY, FilesId INT, Key TEXT,Value TEXT, FOREIGN KEY(FilesId) REFERENCES Files(Id))
CREATE TABLE Info(Id INTEGER PRIMARY KEY, Key TEXT, Value TEXT)
CREATE TABLE Peaks(Id INTEGER PRIMARY KEY, PeaksMz BLOB, PeaksIntensity BLOB, SpectraIdList TEXT, PeaksCount INT, MetaText TEXT, Comment TEXT)
CREATE TABLE Spectra(\
    Id INTEGER PRIMARY KEY, FilesId INT, MSLevel INT, ObservedMz REAL, IsolationWindowLowerOffset REAL, IsolationWindowUpperOffset REAL, RetentionTime REAL, \
    ScanNumber INT, NativeId TEXT,\
    ChargeList TEXT, PeaksId INT, PrecursorIntensity REAL, FragmentationType TEXT, \
    ParentScanNumber INT, ParentNativeId TEXT, \
    Comment TEXT, MetaText TEXT, DebugText TEXT,\
    FOREIGN KEY(FilesId) REFERENCES Files(Id),\
    FOREIGN KEY(PeaksId) REFERENCES Peaks(Id))

Updated schema with compression:

CREATE TABLE Peaks(Id INTEGER PRIMARY KEY, PeaksMz BLOB, PeaksIntensity BLOB, SpectraIdList TEXT, PeaksCount INT, MetaText TEXT, Comment TEXT, PeaksDictionaryId INT);

CREATE TABLE PeaksDictionary(Id INTEGER PRIMARY KEY, DictX BLOB, DictY BLOB, MetaText TEXT);

Add any meta info to FilesInfo for compression flavors, E.g.:
FilesId=1, Key=CompressionFlavor, Value=Flavor_xyz_123

PICO VERIFY code to read compressed and compare to uncompressed
*/

// create new compressed byspec file
//Note(Yong to Doron): db_filname is not used anywhere.
//What's the proper interface for this function?
bool Sqlite::createByspecFile(const char* /*db_filname*/)
{
    ostringstream cmd; cmd << "CREATE TABLE IF NOT EXISTS Files(Id INTEGER PRIMARY KEY,Filename TEXT,Location TEXT,Type TEXT,Signature TEXT);";
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    int rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'Files' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS FilesInfo(Id INTEGER PRIMARY KEY, FilesId INT, Key TEXT,Value TEXT, FOREIGN KEY(FilesId) REFERENCES Files(Id));";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'FilesInfo' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS Info(Id INTEGER PRIMARY KEY, Key TEXT, Value TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'Info' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS Peaks(Id INTEGER PRIMARY KEY, PeaksMz BLOB, PeaksIntensity BLOB, SpectraIdList TEXT, PeaksCount INT, MetaText TEXT, Comment TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'Peaks' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS Spectra(Id INTEGER PRIMARY KEY, FilesId INT, MSLevel INT, ObservedMz REAL, IsolationWindowLowerOffset REAL, IsolationWindowUpperOffset REAL," \
        "RetentionTime REAL, ScanNumber INT, NativeId TEXT, ChargeList TEXT, PeaksId INT, PrecursorIntensity REAL, FragmentationType TEXT," \
        "ParentScanNumber INT, ParentNativeId TEXT, Comment TEXT, MetaText TEXT, DebugText TEXT, FOREIGN KEY(FilesId) REFERENCES Files(Id),FOREIGN KEY(PeaksId) REFERENCES Peaks(Id));";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'Spectra' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
/*    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS PeaksDictionary(Id INTEGER PRIMARY KEY, MzDict BLOB, IntensityDict BLOB, MetaText TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'PeaksDictionary' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }*/
    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS CompressionInfo(Id INTEGER PRIMARY KEY, MzDict BLOB, IntensityDict BLOB, Property TEXT, Version TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create CompressionInfo table in sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}

// write compressed peak blobs to peaks table
bool Sqlite::writeCompressedPeaks(int scan, unsigned char** compressed, unsigned int* length, bool empty_peaks)
{
    //ostringstream cmd; cmd << "INSERT or REPLACE into Peaks (Id, PeaksMz, PeaksIntensity, SpectraIdList, PeaksCount, MetaText, Comment) values( ,?,?,)";
    ostringstream cmd;
    if (empty_peaks) cmd << "INSERT into Peaks(PeaksMz) values (?)";
    else cmd << "UPDATE Peaks SET PeaksMz=? WHERE Id == '" << scan << "'";
    sqlite3_stmt *stmt=NULL; bool success = false; int rc;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    if ((rc=sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL)) == SQLITE_OK){
        sqlite3_bind_blob(stmt, 1, (unsigned char*)*compressed, (int)*length, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            success = true;
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't write compressed Peaks table to sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// write compressed peak blobs to peaks table
bool Sqlite::writeCompressedPeaksAndTIC(int scan, unsigned char** compressed, unsigned int* length, unsigned long long intensity_sum, bool empty_peaks)
{
    //ostringstream cmd; cmd << "INSERT or REPLACE into Peaks (Id, PeaksMz, PeaksIntensity, SpectraIdList, PeaksCount, MetaText, Comment) values( ,?,?,)";
    ostringstream cmd;
    if (empty_peaks) cmd << "INSERT into Peaks(PeaksMz) values (?)";
    else cmd << "UPDATE Peaks SET PeaksMz=? WHERE Id == '" << scan << "'";
    sqlite3_stmt *stmt=NULL; bool success = false; int rc;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    if ((rc=sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL)) == SQLITE_OK){
        sqlite3_bind_blob(stmt, 1, (unsigned char*)*compressed, (int)*length, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            success = true;
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success){
        cout << "can't write compressed Peaks table to sqlite -- " << string(sqlite3_errmsg(db)) << endl; return false;
    }

    ostringstream cmd1;
    if (empty_peaks) cmd1 << "INSERT into TIC(IntensitySum) values('" << intensity_sum << "')";
    else cmd << "UPDATE TIC SET IntensitySum='" << intensity_sum << "') WHERE Id == '" << scan << "'";

    sqlite3_stmt *stmt1=NULL; success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    if ((rc=sqlite3_prepare_v2(db, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
        rc = sqlite3_step(stmt1);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            success = true;
        }
    }
    if (stmt1) sqlite3_finalize(stmt1);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't write TIC table to sqlite -- " << string(sqlite3_errmsg(db)) << endl;

    return success;
}

// write compressed peak blobs to peaks table
bool Sqlite::writeCompressedPeaks(int scan, unsigned char** compressed_mz, unsigned int* compressed_mz_length, unsigned char** compressed_intensity, unsigned int* compressed_intensity_length, bool empty_peaks)
{
    //ostringstream cmd; cmd << "INSERT or REPLACE into Peaks (Id, PeaksMz, PeaksIntensity, SpectraIdList, PeaksCount, MetaText, Comment) values( ,?,?,)";
    ostringstream cmd;
    if (empty_peaks) cmd << "INSERT into Peaks(PeaksMz,PeaksIntensity) values (?,?)";
    else cmd << "UPDATE Peaks SET PeaksMz=?, PeaksIntensity=? WHERE Id == '" << scan << "'";

    sqlite3_stmt *stmt=NULL; bool success = false; int rc;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    if ((rc=sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL)) == SQLITE_OK){
        sqlite3_bind_blob(stmt, 1, (unsigned char*)*compressed_mz, (int)*compressed_mz_length, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 2, (unsigned char*)*compressed_intensity, (int)*compressed_intensity_length, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            success = true;
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't write compressed Peaks table to sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// write compressed peak blobs to peaks table
bool Sqlite::writeCompressedPeaksAndTIC(int scan, unsigned char** compressed_mz, unsigned int* compressed_mz_length, unsigned char** compressed_intensity, unsigned int* compressed_intensity_length,
                    unsigned long long intensity_sum, bool empty_peaks)
{
    //ostringstream cmd; cmd << "INSERT or REPLACE into Peaks (Id, PeaksMz, PeaksIntensity, SpectraIdList, PeaksCount, MetaText, Comment) values( ,?,?,)";
    ostringstream cmd;
    if (empty_peaks) cmd << "INSERT into Peaks(PeaksMz,PeaksIntensity) values (?,?)";
    else cmd << "UPDATE Peaks SET PeaksMz=?, PeaksIntensity=? WHERE Id == '" << scan << "'";

    sqlite3_stmt *stmt=NULL; bool success = false; int rc;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    if ((rc=sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL)) == SQLITE_OK){
        sqlite3_bind_blob(stmt, 1, (unsigned char*)*compressed_mz, (int)*compressed_mz_length, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 2, (unsigned char*)*compressed_intensity, (int)*compressed_intensity_length, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            success = true;
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success){
        cout << "can't write compressed Peaks table to sqlite -- " << string(sqlite3_errmsg(db)) << endl; return false;
    }

    ostringstream cmd1;
    if (empty_peaks) cmd1 << "INSERT into TIC(IntensitySum) values('" << intensity_sum << "')";
    else cmd << "UPDATE TIC SET IntensitySum='" << intensity_sum << "') WHERE Id == '" << scan << "'";

    sqlite3_stmt *stmt1=NULL; success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    if ((rc=sqlite3_prepare_v2(db, cmd1.str().c_str(), -1, &stmt1, NULL)) == SQLITE_OK){
        rc = sqlite3_step(stmt1);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            success = true;
        }
    }
    if (stmt1) sqlite3_finalize(stmt1);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't write TIC table to sqlite -- " << string(sqlite3_errmsg(db)) << endl;

    return success;
}

// write umcompressed peak blobs to peaks table
bool Sqlite::writeUncompressedPeaksToSqlite(int scan, double** restored_mz, float** restored_intensity, unsigned int* length, bool empty_peaks)
{
    //ostringstream cmd; cmd << "INSERT or REPLACE into Peaks (Id, PeaksMz, PeaksIntensity, SpectraIdList, PeaksCount, MetaText, Comment) values( ,?,?,)";
    ostringstream cmd;
    if (empty_peaks) cmd << "INSERT into Peaks(PeaksMz,PeaksIntensity) values (?,?)";
    else cmd << "UPDATE Peaks SET PeaksMz=?, PeaksIntensity=? WHERE Id == '" << scan << "'";
    sqlite3_stmt *stmt=NULL; bool success = false; int rc;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    if ((rc=sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL)) == SQLITE_OK){
        sqlite3_bind_blob(stmt, 1, (unsigned char*)*restored_mz, (int)((*length)*8), SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 2, (unsigned char*)*restored_intensity, (int)((*length)*4), SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            success = true;
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't write uncompressed Peaks table to sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// copy uncompressed metadata to compressed byspec file
bool Sqlite::createCopyByspecTablesToSqlite(const std::wstring &another_db_filname, bool newTIC)
{
    ostringstream cmd; cmd << "DROP TABLE IF EXISTS Files ;";
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    int rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Files' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS FilesInfo;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'FilesInfo' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS Info;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Info' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS Peaks;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Peaks' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS Spectra;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Spectra' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS PeaksDictionary;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'PeaksDictionary' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS CompressionInfo;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'CompressionInfo' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS TIC;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'TIC' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);

    cmd.str(""); cmd << "CREATE TABLE Peaks(Id INTEGER PRIMARY KEY, PeaksMz BLOB, PeaksIntensity BLOB, SpectraIdList TEXT, PeaksCount INT, MetaText TEXT, Comment TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'Peaks' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }

    // attach original database
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    cmd.str(""); cmd << "ATTACH '" << conv.to_bytes(another_db_filname).c_str() << "' AS aDB";  //'c://data//Pfizer_NIST_Pepmap.byspec2.sqlite' AS aDB;";  c://data//ecoli-0500-r001_d_out_byspec2_sqlite'
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't attach sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }

    // copy other Peaks content from original database  <===============  requires going through huge Peaks table ===========
/*    cmd.str(""); cmd << "INSERT into main.Peaks(Id, SpectraIdList, PeaksCount, MetaText, Comment) SELECT Id, SpectraIdList, PeaksCount, MetaText, Comment FROM aDB.Peaks;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'Peaks' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
*/
    cmd.str(""); cmd << "CREATE TABLE Files AS SELECT * FROM aDB.Files;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't copy Files table into sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE FilesInfo AS SELECT * FROM aDB.FilesInfo;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't copy FilesInfo table into sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE Info AS SELECT * FROM aDB.Info;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't copy Info table into sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE Spectra AS SELECT * FROM aDB.Spectra;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't copy Spectra table into sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }

    if (newTIC){
        cmd.str(""); cmd << "CREATE TABLE TIC(Id INTEGER PRIMARY KEY, IntensitySum INTEGER);";  //<======================
        rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
        if (rc) {
            cout << "can't create TIC table in sqlite database -- " << sqlite3_errmsg(db) << endl;
            sqlite3_free(error); return false;
        }
    } else {
        cmd.str(""); cmd << "SELECT 1 FROM aDB.TIC LIMIT 1";
        rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
        if (rc==0) {
            cmd.str(""); cmd << "CREATE TABLE IF EXISTS aDB.TIC TIC AS SELECT * FROM aDB.TIC;";
            rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
            if (rc) {
                cout << "can't copy TIC table into sqlite database -- " << sqlite3_errmsg(db) << endl;
                sqlite3_free(error); return false;
            }
        }
    }

    // detach original database
    cmd.str(""); cmd << "DETACH DATABASE 'aDB'";  //'c://data//Pfizer_NIST_Pepmap.byspec2.sqlite' AS aDB;";  c://data//ecoli-0500-r001_d_out_byspec2_sqlite'
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't detach sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }

/*    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS PeaksDictionary(Id INTEGER PRIMARY KEY, MzDict BLOB, IntensityDict BLOB, Property TEXT, Value TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create PeaksDictionary table in sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }*/
    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS CompressionInfo(Id INTEGER PRIMARY KEY, MzDict BLOB, IntensityDict BLOB, Property TEXT, Version TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create CompressionInfo table in sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}

// create a new compressed byspec file
bool Sqlite::createNewCompressedByspecFile()
{
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
/*    ostringstream cmd; cmd << "DROP TABLE IF EXISTS Files ;";
    int rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Files' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS FilesInfo;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'FilesInfo' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS Info;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Info' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
*/
    ostringstream cmd; cmd << "DROP TABLE IF EXISTS Peaks;";
    int rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Peaks' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
/*    cmd.str(""); cmd << "DROP TABLE IF EXISTS Spectra;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Spectra' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }*/
    cmd.str(""); cmd << "DROP TABLE IF EXISTS PeaksDictionary;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'PeaksDictionary' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS CompressionInfo;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'CompressionInfo' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);

    cmd.str(""); cmd << "CREATE TABLE Peaks(Id INTEGER PRIMARY KEY, PeaksMz BLOB, PeaksIntensity BLOB, SpectraIdList TEXT, PeaksCount INT, MetaText TEXT, Comment TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'Peaks' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }

/*    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS PeaksDictionary(Id INTEGER PRIMARY KEY, MzDict BLOB, IntensityDict BLOB, Property TEXT, Value TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create PeaksDictionary table in sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }*/
    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS CompressionInfo(Id INTEGER PRIMARY KEY, MzDict BLOB, IntensityDict BLOB, Property TEXT, Version TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create CompressionInfo table in sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS TIC(Id INTEGER PRIMARY KEY, IntensitySum INTEGER);";  //<======================
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create TIC table in sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}


// create new byspec file
bool Sqlite::createNewByspecFile()
{
    ostringstream cmd; cmd << "DROP TABLE IF EXISTS Files ;";
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    int rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Files' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS FilesInfo;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'FilesInfo' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS Info;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Info' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS Peaks;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Peaks' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS Spectra;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Spectra' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS PeaksDictionary;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'PeaksDictionary' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS CompressionInfo;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'CompressionInfo' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
/*    cmd.str(""); cmd << "DROP TABLE IF EXISTS TIC;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'TIC' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    } */
    cmd.str(""); cmd << "DROP TABLE IF EXISTS Chromatogram;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Chromatogram' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }

    cmd.str(""); cmd << "CREATE TABLE Chromatogram(Id INTEGER PRIMARY KEY,FilesId INT,Identifier TEXT,ChromatogramType TEXT,DataX BLOB,DataY BLOB,DataCount INT,MetaText TEXT,DebugText TEXT,FOREIGN KEY(FilesId) REFERENCES Files(Id));";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't copy Files table into sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE Files(Id INTEGER PRIMARY KEY,Filename TEXT,Location TEXT,Type TEXT,Signature TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't copy Files table into sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE FilesInfo(Id INTEGER PRIMARY KEY, FilesId INT, Key TEXT,Value TEXT, FOREIGN KEY(FilesId) REFERENCES Files(Id));";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't copy FilesInfo table into sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE Info(Id INTEGER PRIMARY KEY, Key TEXT, Value TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't copy Info table into sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE Spectra(Id INTEGER PRIMARY KEY, FilesId INT, MSLevel INT, ObservedMz REAL, IsolationWindowLowerOffset REAL, IsolationWindowUpperOffset REAL, RetentionTime REAL, ScanNumber INT, NativeId TEXT, ChargeList TEXT, PeaksId INT, PrecursorIntensity REAL, FragmentationType TEXT, ParentScanNumber INT, ParentNativeId TEXT, Comment TEXT, MetaText TEXT, DebugText TEXT, FOREIGN KEY(FilesId) REFERENCES Files(Id), FOREIGN KEY(PeaksId) REFERENCES Peaks(Id));";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't copy Spectra table into sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }

    cmd.str(""); cmd << "CREATE TABLE Peaks(Id INTEGER PRIMARY KEY, PeaksMz BLOB, PeaksIntensity BLOB, SpectraIdList TEXT, PeaksCount INT, MetaText TEXT, Comment TEXT, IntensitySum REAL, CompressionInfoId INT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'Peaks' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }

/*    cmd.str(""); cmd << "CREATE TABLE TIC(Id INTEGER PRIMARY KEY, IntensitySum INTEGER);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create TIC table in sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }*/

/*    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS PeaksDictionary(Id INTEGER PRIMARY KEY, MzDict BLOB, IntensityDict BLOB, Property TEXT, Value TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create PeaksDictionary table in sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }*/
    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS CompressionInfo(Id INTEGER PRIMARY KEY, MzDict BLOB, IntensityDict BLOB, Property TEXT, Version TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create CompressionInfo table in sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}

// add PeaksMS1Centroided table to byspec file
bool Sqlite::addPeaksMS1CentroidedTableToByspecFile()
{
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "DROP TABLE IF EXISTS Peaks_MS1Centroided;";
    int rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Peaks_MS1Centroided' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE Peaks_MS1Centroided(Id INTEGER PRIMARY KEY, PeaksMz BLOB, PeaksIntensity BLOB, SpectraIdList TEXT, PeaksCount INT, MetaText TEXT, Comment TEXT, IntensitySum REAL, CompressionInfoId INT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'Peaks_MS1Centroided' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}

// add centroided columns to Peaks table
bool Sqlite::addCentroidedColumnsToPeaksTable()
{
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "ALTER TABLE Peaks ADD COLUMN CentroidPeaksMz Blob;";
    int rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't add CentroidPeaksMz column to 'Peaks' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "ALTER TABLE Peaks ADD COLUMN CentroidPeaksIntensity Blob;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't add CentroidPeaksIntensity column to 'Peaks' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "ALTER TABLE Peaks ADD COLUMN CentroidPeaksCount Integer;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't add CentroidPeaksCount column to 'Peaks' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "ALTER TABLE Peaks ADD COLUMN CentroidIntensitySum Real;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't add CentroidIntensitySum column to 'Peaks' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "ALTER TABLE Peaks ADD COLUMN CentroidCompressionInfoId Integer;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't add CentroidCompressionInfoId column to 'Peaks' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}

// read compressed peak data
bool Sqlite::readCompressedPeakData(unsigned int scan, unsigned char** data, unsigned int *size)
{
    sqlite3_stmt *stmt=NULL; bool success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "Select PeaksMz FROM Peaks WHERE Id == '" << scan << "'";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        //while (true) {
            if (sqlite3_step(stmt) == SQLITE_ROW){
                *size = sqlite3_column_bytes(stmt, 0);
                *data = (unsigned char *)malloc(*size*sizeof(double));
                unsigned char* blob = (unsigned char*)sqlite3_column_blob(stmt, 0);
                memcpy((unsigned char*)*data, blob, *size);
                success = true;
            }
        // end while
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't read mz blob from sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// read compressed peak data
bool Sqlite::readCompressedPeakData(unsigned int scan, unsigned char** mz_data, unsigned int *mz_size, unsigned char** intensity_data, unsigned int *intensity_size)
{
    sqlite3_stmt *stmt=NULL; bool success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "Select PeaksMz,PeaksIntensity FROM Peaks WHERE Id == '" << scan << "'";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        //while (true) {
            if (sqlite3_step(stmt) == SQLITE_ROW){
                *mz_size = sqlite3_column_bytes(stmt, 0);
                *mz_data = (unsigned char *)malloc(*mz_size*sizeof(double));
                unsigned char* blob = (unsigned char*)sqlite3_column_blob(stmt, 0);
                memcpy((unsigned char*)*mz_data, blob, *mz_size);

                *intensity_size = sqlite3_column_bytes(stmt, 1);
                *intensity_data = (unsigned char *)malloc(*intensity_size*sizeof(double));
                unsigned char* blob1 = (unsigned char*)sqlite3_column_blob(stmt, 1);
                memcpy((unsigned char*)*intensity_data, blob1, *intensity_size);
                success = true;
            }
        // end while
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't read mz blob from sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// read compressed peak data
bool Sqlite::readCompressedPeakData(unsigned int scan, unsigned char** mz_data, unsigned int *mz_size, unsigned char** intensity_data, unsigned int *intensity_size, int* compressionInfoId)
{
    return Sqlite::readCompressedData(Sqlite::SqliteTableNames::PEAKS, scan, mz_data, mz_size, intensity_data, intensity_size, compressionInfoId);
}

bool Sqlite::readCompressedPeaks_MS1CentroidedData(unsigned int scan, unsigned char** mz_data, unsigned int *mz_size, unsigned char** intensity_data, unsigned int *intensity_size, int* compressionInfoId)
{
    return Sqlite::readCompressedData(Sqlite::SqliteTableNames::PEAKS_MS1CENTROIDED, scan, mz_data, mz_size, intensity_data, intensity_size, compressionInfoId);
}

bool Sqlite::readCompressedData(Sqlite::SqliteTableNames tableName, unsigned int scan, unsigned char** mz_data, unsigned int *mz_size, unsigned char** intensity_data, unsigned int *intensity_size, int *compressionInfoId)
{
    // Use "Peaks" by default
    std::string tableNameStr = "Peaks";
    if (tableName == Sqlite::SqliteTableNames::PEAKS_MS1CENTROIDED) {
        tableNameStr = "Peaks_MS1Centroided";
    }

    sqlite3_stmt *stmt = NULL; bool success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "Select PeaksMz,PeaksIntensity,CompressionInfoId FROM " << tableNameStr << " WHERE Id == '" << scan << "'";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        if (sqlite3_step(stmt) == SQLITE_ROW){
            *mz_size = sqlite3_column_bytes(stmt, 0);
            *mz_data = (unsigned char *)malloc(*mz_size*sizeof(double));
            unsigned char* blob = (unsigned char*)sqlite3_column_blob(stmt, 0);
            memcpy((unsigned char*)*mz_data, blob, *mz_size);

            *intensity_size = sqlite3_column_bytes(stmt, 1);
            *intensity_data = (unsigned char *)malloc(*intensity_size*sizeof(double));
            unsigned char* blob1 = (unsigned char*)sqlite3_column_blob(stmt, 1);
            memcpy((unsigned char*)*intensity_data, blob1, *intensity_size);
            *compressionInfoId = sqlite3_column_int(stmt, 2);
            success = true;
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't read mz blob from sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// add property value pair to peaks dictionary
bool Sqlite::addPropertyValueToPeaksDictionary(const char* property, const char* value)
{
    sqlite3_stmt *stmt=NULL; bool success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "INSERT into PeaksDictionary (Property, Value) values ('" << property << "','" << value << "')";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        //while (true) {
            if (sqlite3_step(stmt) == SQLITE_ROW){
                success = true;
            }
        // end while
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't add property/value to PeaksDictionary in sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// prepare file for read append
bool Sqlite::prepareReadAppendByspecFile()
{
    ostringstream cmd; cmd << "DROP TABLE IF EXISTS Peaks;";
    int rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Peaks' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS Spectra;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Spectra' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "DROP TABLE IF EXISTS CompressionInfo;";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'CompressionInfo' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE Spectra(Id INTEGER PRIMARY KEY, FilesId INT, MSLevel INT, ObservedMz REAL, IsolationWindowLowerOffset REAL, IsolationWindowUpperOffset REAL, RetentionTime REAL, ScanNumber INT, NativeId TEXT, ChargeList TEXT, PeaksId INT, PrecursorIntensity REAL, FragmentationType TEXT, ParentScanNumber INT, ParentNativeId TEXT, Comment TEXT, MetaText TEXT, DebugText TEXT, FOREIGN KEY(FilesId) REFERENCES Files(Id), FOREIGN KEY(PeaksId) REFERENCES Peaks(Id));";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't copy Spectra table into sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE Peaks(Id INTEGER PRIMARY KEY, PeaksMz BLOB, PeaksIntensity BLOB, SpectraIdList TEXT, PeaksCount INT, MetaText TEXT, Comment TEXT, IntensitySum REAL, CompressionInfoId INT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'Peaks' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE IF NOT EXISTS CompressionInfo(Id INTEGER PRIMARY KEY, MzDict BLOB, IntensityDict BLOB, Property TEXT, Version TEXT);";
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create CompressionInfo table in sqlite database -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}

// add Calibration table to byspec file
bool Sqlite::addCalibrationTableToByspecFile()
{
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "DROP TABLE IF EXISTS Calibration;";
    int rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't drop 'Calibration' table from sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    cmd.str(""); cmd << "CREATE TABLE Calibration(Id INTEGER PRIMARY KEY, MzCoeffs TEXT, MetaText TEXT);"; //comma delimited
    rc = sqlite3_exec(db, cmd.str().c_str(), NULL, NULL, &error);
    if (rc) {
        cout << "can't create 'Calibration' table in sqlite -- " << sqlite3_errmsg(db) << endl;
        sqlite3_free(error); return false;
    }
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}

// append calibration info to byspec file
bool Sqlite::appendCalibrationInfoToByspecFile(int files_id, const std::vector<std::string> &calib_mod)
{
    std::string base = "Cal Modification ";
    for (unsigned int ii = 0; ii < calib_mod.size(); ii++){
        std::string calib_mod_i = calib_mod[ii], tag;
        std::string mod_i = split_tag(calib_mod_i, &tag);
        if (calib_mod_i.size() == 0)
            continue;
        std::string key = base + to_string(ii);
        if (!addKeyValueToFilesInfo(files_id, key.c_str(), mod_i.c_str())){
            cout << "can't add cal modification to FilesInfo in sqlite -- " << string(sqlite3_errmsg(db)) << endl; return false;
        }
    }
    return true;
}

// get key value property from FilesInfo
bool Sqlite::getKeyValuePropertyFromFilesInfo(const std::string &key, std::string * value){
    sqlite3_stmt *stmt = NULL; bool success = false;// int rc = 0;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "SELECT Value from FilesInfo WHERE Key='" << key << "'";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        //while (true) {
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            char *text = (char *)sqlite3_column_text(stmt, 0);
            if (text == NULL)
                *value = "";
            else
                *value = text;
            success = true;
        }
        // end while
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't get property value from FilesInfo in sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// given a Value, get the Id from files info
bool Sqlite::getFilesInfoIdFromValue(const std::string &value, int * id) {
    *id = -1;

    sqlite3_stmt *stmt = NULL; 
    bool success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "SELECT Id from FilesInfo WHERE Value='" << value << "'";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        //while (true) {
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            *id = sqlite3_column_int(stmt, 0);
            success = true;
        }
        // end while
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't get id of value from FilesInfo in sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// get key value property from FilesInfo
bool Sqlite::getValueFromFilesInfoId(int id, std::string * value) {
    sqlite3_stmt *stmt = NULL; 
    bool success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "SELECT Value from FilesInfo WHERE Id='" << id << "'";

    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        //while (true) {
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW || rc == SQLITE_DONE){
            char *text = (char *)sqlite3_column_text(stmt, 0);
            if (text == NULL)
                *value = "";
            else
                *value = text;
            success = true;
        }
        // end while
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't get property value from FilesInfo in sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// get max_scan_id from sqlite
bool Sqlite::getMaxScanIdFunctionFromSqlite(std::vector<int> * max_scan_id){
    max_scan_id->clear();
    sqlite3_stmt *stmt = NULL; bool success = false;// int rc = 0;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "SELECT Id,NativeId FROM Spectra";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW){
                int id = sqlite3_column_int(stmt, 0);
                char *text = (char *)sqlite3_column_text(stmt, 1);
                if (text != NULL){
                    string str(text);
                    const char *sptr = strstr(str.c_str(), "function=");
                    if (sptr != NULL){
                        int function = atoi(sptr + 9);
                        if (function >= (int)max_scan_id->size()) max_scan_id->resize(function);
                        int max = (*max_scan_id)[function - 1];
                        if (id > max){
                            (*max_scan_id)[function - 1] = id;
                        }
                    }
                }
                success = true;
            } else if (rc == SQLITE_DONE){
                success = true; break;
            } else {
                success = false; break;
            }
        } // end while
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    for (int ii = 0; ii < (int)max_scan_id->size(); ii++)
        (*max_scan_id)[ii]++;
    if (!success)
        cout << "can't get property value from FilesInfo in sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// strip html tag
std::string Sqlite::split_tag(const std::string &str, std::string * tag){
    tag->clear();
    std::size_t found = str.find(">");
    if (found == std::string::npos)
        return str;
    *tag = str.substr(1, found);
    std::string sti = str.substr(found + 1, str.length());
    found = sti.find(",<");
    if (found == std::string::npos)
        return sti;
    return sti.substr(0, found);
}

// write Files table to database
bool Sqlite::writeFilesTableToSqlite(const std::wstring& raw_dir, const std::vector<wstring>& list, int files_id)
{
    PicoUtil* utl = new PicoUtil();
    bool success = false;
    pmi::Err e = pmi::Err::kNoErr;
    for (unsigned int ii=0; ii<list.size(); ii++){
        wstring name = utl->stripPathChar(list[ii]);
        wstring location = L"file://" + raw_dir; // + name;
        success = addKeyValueToFilesInfo(files_id, name.c_str(), location.c_str());
        if (!success){
            cout << "can't add file/location to Files in sqlite -- " << string(sqlite3_errmsg(db)) << endl; break;
        }
    }

    sqlite3_stmt *stmt=NULL;
    int rc = 0;
    success = insertKeyValToFilesInfoTable(files_id, L"Version", byspec_version);

    if (!success)
        cout << "can't add version to FilesInfo in sqlite -- " << string(sqlite3_errmsg(db)) << endl;

    time_t now = time(0); // add convert time
    struct tm tstruct; wchar_t tdate[64], ttime[64];
    if (localtime_s(&tstruct, &now)!=0){
        cout << "can't get local time" << endl; return false;
    }
    wcsftime(tdate, sizeof(tdate), L"%b %d %Y", &tstruct); wcsftime(ttime, sizeof(ttime), L"%H:%M:%S", &tstruct);
    rc = 0;
    success = false;
    success = addKeyValueToFilesInfo(files_id, L"ConvertDate", tdate);
    if (!success)
        cout << "can't add date to FilesInfo in sqlite -- " << string(sqlite3_errmsg(db)) << endl;

    rc = 0;
    success = false;
    success = addKeyValueToFilesInfo(files_id, L"ConvertTime", ttime);
    if (!success)
        cout << "can't add time to FilesInfo in sqlite -- " << string(sqlite3_errmsg(db)) << endl;

    wostringstream date_time;
    date_time << __DATE__ << " | " << __TIME__;
    std::wstring date_time_str = date_time.str();
    const wchar_t * date_time_c_str = date_time_str.c_str();
    success = addKeyValueToFilesInfo(files_id, L"CompileTime", date_time_c_str);

    success = addKeyValueToFilesInfo(files_id, "RepoBranch", mscompress::VCS_BRANCH.c_str());

    string git_version = mscompress::VCS_TAG + "." + toString(mscompress::VCS_TICK) + "-" + mscompress::VCS_SHORT_HASH;
    success = addKeyValueToFilesInfo(files_id, "RepoVersion", git_version.c_str());

    stmt=NULL;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL); rc = 0;
    if (pmi::my_sqlite3_prepare(db, "INSERT INTO Files (Filename) VALUES (?)", &stmt) == pmi::Err::kNoErr){
        success = true;
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        e = pmi::my_sqlite3_bind_text(db, stmt, 1, conv.to_bytes(raw_dir).c_str());
        if (e != pmi::Err::kNoErr){
            success = false;
        }
        e = pmi::my_sqlite3_step(db, stmt, pmi::MySqliteStepOption_Reset);
        if (e != pmi::Err::kNoErr){
            success = false;
        }
    }
    if (stmt) pmi::my_sqlite3_finalize(stmt, db);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't add version to FilesInfo in sqlite -- " << string(sqlite3_errmsg(db)) << endl;

    delete utl;
    return success;
}

// add key value pair to Files info
bool Sqlite::addKeyValueToFilesInfo(int files_id, const wchar_t* key, const wchar_t* value) const
{
    return insertKeyValToFilesInfoTable(files_id, key, value);
}

// add key value pair to Files info
bool Sqlite::addKeyValueToFilesInfo(int files_id, const char * key, const char * value) const
{
    return insertKeyValToFilesInfoTable(files_id, key, value);
}

// add key value pair to Info table
bool Sqlite::addKeyValueToInfo(const wchar_t* key, const wchar_t* value) const
{
    return insertKeyValToFilesInfoTable(NULL, key, value);
}

bool Sqlite::addKeyValueToFilesInfo(int files_id, const wchar_t* key, int value) const {
    return insertKeyValToFilesInfoTable(files_id, key, value);
}

bool Sqlite::insertKeyValToFilesInfoTable(int files_id, const char * key, const char * value) const {
    sqlite3_stmt *stmt = NULL; bool success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    pmi::Err e = pmi::Err::kNoErr;
    if (pmi::my_sqlite3_prepare(db, "INSERT INTO FilesInfo (FilesId, Key, Value) VALUES (?,?,?)",
                                &stmt) == pmi::Err::kNoErr)
    {
        if (files_id == NULL) {
            e = pmi::my_sqlite3_bind_null(db, stmt, 1); eee;
        }
        else {
            e = pmi::my_sqlite3_bind_int(db, stmt, 1, files_id); eee;
        }
        e = pmi::my_sqlite3_bind_text(db, stmt, 2, key); eee;
        e = pmi::my_sqlite3_bind_text(db, stmt, 3, value); eee;
        e = pmi::my_sqlite3_step(db, stmt, pmi::MySqliteStepOption_Reset); eee;
    }
error:
    if (stmt) pmi::my_sqlite3_finalize(stmt, db);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (e == pmi::Err::kNoErr) {
        success = true;
    }
    else {
        cout << "can't add property/value to PeaksDictionary in sqlite -- " << string(sqlite3_errmsg(db)) << endl;
        success = false;
    }
    return success;
}

bool Sqlite::insertKeyValToFilesInfoTable(int files_id, const wchar_t* key, const wchar_t* value) const {
    sqlite3_stmt *stmt = NULL; bool success = false;
    wstring val(value);
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    pmi::Err e = pmi::Err::kNoErr;
    if (pmi::my_sqlite3_prepare(db, "INSERT INTO FilesInfo (FilesId, Key, Value) VALUES (?,?,?)",
                                &stmt) == pmi::Err::kNoErr)
    {
        if (files_id == NULL) {
            e = pmi::my_sqlite3_bind_null(db, stmt, 1); eee;
        }
        else {
            e = pmi::my_sqlite3_bind_int(db, stmt, 1, files_id); eee;
        }
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        e = pmi::my_sqlite3_bind_text(db, stmt, 2, conv.to_bytes(key).c_str()); eee;
        e = pmi::my_sqlite3_bind_text(db, stmt, 3, conv.to_bytes(val).c_str()); eee;
        e = pmi::my_sqlite3_step(db, stmt, pmi::MySqliteStepOption_Reset); eee;
    }
error:
    if (stmt) pmi::my_sqlite3_finalize(stmt, db);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (e == pmi::Err::kNoErr) {
        success = true;
    }
    else {
        cout << "can't add property/value to PeaksDictionary in sqlite -- " << string(sqlite3_errmsg(db)) << endl;
        success = false;
    }
    return success;
}

bool Sqlite::insertKeyValToFilesInfoTable(int files_id, const wchar_t* key, int value) const {
    sqlite3_stmt *stmt = NULL; bool success = false;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    pmi::Err e = pmi::Err::kNoErr;
    if (pmi::my_sqlite3_prepare(db, "INSERT INTO FilesInfo (FilesId, Key, Value) VALUES (?,?,?)",
                                &stmt) == pmi::Err::kNoErr)
    {
        if (files_id == NULL) {
            e = pmi::my_sqlite3_bind_null(db, stmt, 1); eee;
        }
        else {
            e = pmi::my_sqlite3_bind_int(db, stmt, 1, files_id); eee;
        }
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        e = pmi::my_sqlite3_bind_text(db, stmt, 2, conv.to_bytes(key).c_str()); eee;
        e = pmi::my_sqlite3_bind_int(db, stmt, 3, value); eee;
        e = pmi::my_sqlite3_step(db, stmt, pmi::MySqliteStepOption_Reset); eee;
    }

error:
    if (stmt) pmi::my_sqlite3_finalize(stmt, db);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (e == pmi::Err::kNoErr) {
        success = true;
    }
    else {
        cout << "can't add property/value to PeaksDictionary in sqlite -- " << string(sqlite3_errmsg(db)) << endl;
        success = false;
    }
    return success;
}

// add key value to a given table
bool Sqlite::addPropertyValueToCompressionInfo(int id, const char* property, const char* value)
{
    sqlite3_stmt *stmt=NULL; bool success = false;// int rc = 0;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "INSERT into CompressionInfo (Id, Property, Version) values ('" << id << "','" << property << "','" << value << "')";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        //while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW || rc == SQLITE_DONE){
                success = true;
            }
        // end while
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't add property/value to CompressionInfo in sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}

// add key value to a given table
bool Sqlite::getPropertyValueFromCompressionInfo(const char* property, string &value)
{
    sqlite3_stmt *stmt=NULL; bool success = false;// int rc = 0;
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    ostringstream cmd; cmd << "SELECT Version from CompressionInfo WHERE Property='" << property << "'";
    if (sqlite3_prepare_v2(db, cmd.str().c_str(), -1, &stmt, NULL) == SQLITE_OK){
        //while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW || rc == SQLITE_DONE){
                value.insert(0, (char *)sqlite3_column_text(stmt, 0));
                success = true;
            }
        // end while
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (!success)
        cout << "can't get property value from CompressionInfo in sqlite -- " << string(sqlite3_errmsg(db)) << endl;
    return success;
}
