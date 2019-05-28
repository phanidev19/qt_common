// ********************************************************************************************
// PicoUtil.cpp : A collection of utility functions
//
//      @Author: D.Kletter    (c) Copyright 2014, PMI Inc.
// 
// This class provides a collection of generic utility functions such as file and directory services.
//
//
// *******************************************************************************************

#include <iostream>
//#include <iomanip>
#include <direct.h>
#include <vector>
#include <algorithm>
#include "PicoUtil.h"
#include "Dirent.h"

#include <codecvt>
#include <string>

#define R_BUFLEN 64LL*1024LL*1024LL  // 64MB read, must be multiple of 16

using namespace std;

_PICO_BEGIN

void fclose_safe(FILE * & fp) {
    if (fp) {
        fclose(fp);
        fp = NULL;
    }
}

void free_safe(unsigned char * &ptr) {
    if (ptr) free(ptr);
    ptr = NULL;
}

void free_safe(char * &ptr) {
    if (ptr) free(ptr);
    ptr = NULL;
}

void free_safe(double * &ptr) {
    if (ptr) free(ptr);
    ptr = NULL;
}

void free_safe(float * &ptr) {
    if (ptr) free(ptr);
    ptr = NULL;
}

void free_safe(unsigned int * &ptr) {
    if (ptr) free(ptr);
    ptr = NULL;
}

// constructor
PicoUtil::PicoUtil()
{
}

PicoUtil::~PicoUtil()
{
}

std::wstring PicoUtil::getFileExtension(const std::wstring& filename) 
{
    size_t index = filename.rfind('.', filename.length());
    // not found
    if (index != string::npos) {
        // return correct extension substring
        return(filename.substr(index + 1, filename.length() - index));
    }
    return(L"");
}

std::wstring PicoUtil::removeFileExtension(const std::wstring &filename) 
{
    size_t index = filename.rfind('.', filename.length());
    // found
    if (index != string::npos) {
        // return correct substring without extension
        return(filename.substr(0, index));
    }
    return filename;
}


// gather a list of files in a directory
std::vector<wstring> PicoUtil::dirList(const wstring &directory)
{
    std::vector<wstring> list;
    _WDIR *dir;
    struct _wdirent *dent;
    if ((dir = _wopendir(directory.c_str()))!= NULL) {
        while ((dent = _wreaddir(dir)) != NULL) {
            if (wcscmp(dent->d_name, L".")!=0 && wcscmp(dent->d_name, L"..")!=0) {
                list.push_back(dent->d_name);
                //printf ("%s\n", dent->d_name);
            }
        }
        _wclosedir(dir);
    } else {
        wcerr << "can't find directory = \"" << directory << "\"" << endl;
    }
    return list;
}

// gather a list of specified extension files in a directory
std::vector<wstring> PicoUtil::dirListExtension(const wstring &directory, const wstring &extension)
{
    std::vector<wstring> list;
    _WDIR *dir;
    struct _wdirent *dent;
    if ((dir = _wopendir(directory.c_str()))!= NULL) {
        while ((dent = _wreaddir(dir)) != NULL) {
            if (wcscmp(dent->d_name, L".")!=0 && wcscmp(dent->d_name, L"..")!=0) {
                if (wcslen(dent->d_name)>extension.size()){
                    bool found = true; ptrdiff_t start = wcslen(dent->d_name)-extension.size();
                    for (size_t ii=0; ii<extension.size(); ii++){
                        if (dent->d_name[start+ii]!=extension[ii]){
                            found = false; break;
                        }
                    }
                    if (found)
                        list.push_back(dent->d_name);
                    //printf ("%s\n", dent->d_name);
                }
            }
        }
        _wclosedir(dir);
    } else {
        wcerr << "can't find directory = \"" << directory << "\"" << endl;
    }
    return list;
}

// get file name
wstring PicoUtil::getName(const wstring &filename)
{
    size_t found = filename.rfind(L"\\");
    if (found==std::string::npos)
        found = filename.rfind(L"/");
    if (found==std::string::npos){
        //cout << "can't find path char in filename" << endl; return filename;
    }
    return filename.substr(found, filename.size());
}

wstring PicoUtil::getNameNoSlashes(const std::wstring &filename) {
    size_t found = filename.rfind(L"\\");
    if (found == std::wstring::npos) {
        found = filename.rfind(L"/");
        if (found == std::wstring::npos) {
            //cout << "can't find path char in filename" << endl;
            return filename;
        }
        else {
            // skip begin slash
            return filename.substr(found + 1, filename.size());
        }
    }
    // skip begin slash
    return filename.substr(found + 1, filename.size());
}

// get file path
wstring PicoUtil::getPath(const wstring &filename)
{
    size_t found = filename.rfind(L"\\");
    if (found==std::wstring::npos)
        found = filename.rfind(L"/");
    if (found==std::wstring::npos){
        cout << "can't find path char in filename" << endl;
        return wstring();
    }
    return filename.substr(0, found);
}

std::wstring PicoUtil::getPathWithEndSlashes(const std::wstring &filename) {
    size_t found = filename.rfind(L"\\");
    if (found == std::wstring::npos) {
        found = filename.rfind(L"/");
    }
    if (found == std::wstring::npos){
        cout << "can't find path char in filename" << endl; 
        return wstring();
    }
    // include the path chars at the end
    return filename.substr(0, found + 1);
}

std::wstring PicoUtil::getPathWithPrefixOnFilename(const std::wstring &fullPathFilename, const std::wstring &prefix) {
    std::wstring justFilename = getNameNoSlashes(fullPathFilename);
    std::wstring justPath = getPathWithEndSlashes(fullPathFilename);
    std::wstring newName = justPath + prefix + justFilename;
    return newName;
}

// strip last path char
wstring PicoUtil::stripPathChar(const wstring &dir)
{
    size_t found = dir.rfind(L"\\");
    if (found==std::wstring::npos)
        found = dir.rfind(L"/");
    if (found!=std::wstring::npos && found==dir.size()-1)
        return dir.substr(0, dir.size()-1);
    return dir;
}

// check if file exists
bool PicoUtil::fileExists(const wstring &file)
{
    wstring file1 = stripPathChar(file);
    //std::replace( file1.begin(), file1.end(), '\\', '/');
    struct _stat64 buf;
/*    int err = stat(dir1.c_str(), &info);
    if (err == -1){
    //    return false; 
    }
    return (info.st_mode & S_IFDIR)?false:true;*/
    return (_wstat64(file1.c_str(), &buf) == 0);
}

// check if directory exists
bool PicoUtil::dirExists(const wstring &dir)
{
    wstring dir1 = stripPathChar(dir);
    std::replace( dir1.begin(), dir1.end(), L'\\', L'/'); 
    struct _stat64 info;
    int err = _wstat64(dir1.c_str(), &info);
    if (err == -1){
        if (errno == ENOENT)
            return false;  // path not exist
        else 
            return false;  // error
    } else {
        if (S_ISDIR(info.st_mode))
            return true;  // directory exists
        else
            return false; // exists but not dir
    }
}

// create directory with all necessary sub-dirs
bool PicoUtil::createDir(const wstring &dir)
{
    std::vector<wstring> list;
    wstring fdir = stripPathChar(dir);
    while (true){
        size_t found = fdir.rfind(L"\\");
        if (found==std::wstring::npos)
            found = fdir.rfind(L"/");
        if (found==std::wstring::npos)
            break;
        wstring cur = fdir.substr(found+1, fdir.size());
        fdir = fdir.substr(0, found);
        list.push_back(cur);
    }
    list.push_back(fdir); //char the_path[256]; //_getcwd(the_path, 255);

    wstring name;
    for (int ii=(int)list.size()-1; ii>=0; ii--){
        wstring str = list[ii];
        if (ii==(int)list.size()-1){
            name = str;
            name.append(L"/");
        } else {
            if (ii<(int)list.size()-2)
                name.append(L"/");
            name.append(str);
        }
        
        if (!dirExists(name)){
            if (_wmkdir(name.c_str())!=0){
                wcerr << "can't make directory = " << name << endl;
                return false;
            }
        }
        if (_wchdir(name.c_str())!=0){
            wcerr << "can't make directory = " << name << endl;
            return false;
        }
    }
    return true;
}

// remove directory and all files
bool PicoUtil::rmdir_r(const wstring &dir)
{
    if (_wrmdir(dir.c_str()) != 0)
        return false;
    else
        return true;
}

// copy file to a directory
bool PicoUtil::fileCopy(const wstring &filename, const wstring &to_dir)
{
    if (to_dir.size()==0){
        cout << "empty to_dir" << endl;
        return false;
    }
    wstring to_dir2 = stripPathChar(to_dir);
    wstring fname = getName(filename);
    wstring out_filename = to_dir2;
    out_filename.append(fname);

    long long remain_count = getFileSize(filename);

    FILE* inFile = NULL;
    errno_t err = _wfopen_s(&inFile, filename.c_str(), L"rbS");
    if (err != 0){
        wcerr << "can't open file = " << filename << endl;
        return false;
    }

    FILE* outFile = NULL;
    err = _wfopen_s(&outFile, out_filename.c_str(), L"wbS");
    if (err != 0){
        wcerr << "can't open file = " << out_filename << endl; 
        fclose_safe(inFile); 
        return false;
    }

    size_t size = (size_t)min(R_BUFLEN, remain_count);
    unsigned char* buf = (unsigned char*)malloc(size*sizeof(unsigned long long)); 
    if (buf == NULL){
        cout << "copy buf alloc failed" << endl; 
        fclose_safe(inFile);
        fclose_safe(outFile);
        return false;
    }

    size_t count;
    while (count = fread_s(buf, size*sizeof(unsigned long long), 1, size, inFile))
        fwrite(buf, 1, count, outFile);

    free_safe(buf); fclose_safe(inFile); fclose_safe(outFile);
    return true;
}

// write filename to file
bool PicoUtil::writeFileName(const wstring &name, const wstring &out_filename)
{
    FILE* outFile = NULL;
    errno_t err = _wfopen_s(&outFile, out_filename.c_str(), L"wS");
    if (err != 0){
        wcerr << "can't open file = " << out_filename << endl;
        return false;
    }
    fwprintf_s(outFile, L"%s", name.c_str());
    fflush(outFile); fclose_safe(outFile);
    return true;
}

// read filename from file
wstring PicoUtil::readFileName(const wstring &filename)
{
    FILE* inFile = NULL;
    errno_t err = _wfopen_s(&inFile, filename.c_str(), L"rS");
    if (err != 0){
        wcerr << "can't open file = " << filename << endl;
        return wstring();
    }
    wchar_t line[256];
    fgetws(line, sizeof(line), inFile);
    fclose_safe(inFile);
    return line;
}

// read unsigned int csv data from file
std::vector<unsigned int> PicoUtil::readCsvFileUInt(const wstring &filename)
{
    std::vector<unsigned int> vals;
    FILE* inFile = NULL;
    errno_t err = _wfopen_s(&inFile, filename.c_str(), L"r");
    if (err != 0){
        wcerr << "can't open file = " << filename << endl;
        if (err==13) {
            cout << "is it open in Excel?" << endl;
        }
        return vals;
    }
    unsigned int val = 0;
    while (!feof (inFile)){
        fscanf_s(inFile, "%d", &val); vals.push_back(val);
    }
    fclose_safe(inFile); 
    return vals;
}

// read double csv data from file
std::vector<double> PicoUtil::readCsvFileDouble(const wstring &filename)
{
    std::vector<double> vals;
    FILE* inFile = NULL;
    errno_t err = _wfopen_s(&inFile, filename.c_str(), L"r");
    if (err != 0){
        wcerr << "can't open file = " << filename << endl;
        if (err==13) {
            cout << "is it open in Excel?" << endl;
        }
        return vals;
    }
    double val = 0; char line[64];
    while( fgets(line, 64, inFile) ){
        if( 1==sscanf_s(line, "%lf", &val) )
            vals.push_back(val);
    }
    fclose_safe(inFile); 
    return vals;
}

// write double mzs to file
bool PicoUtil::writeDoubleMzsToFile(const std::vector<double> &mzs, const wstring &out_filename)
{
    FILE* outFile = NULL;
    errno_t err = _wfopen_s(&outFile, out_filename.c_str(), L"w");
    if (err != 0){
        wcerr << "can't open file = " << out_filename << endl;
        return false;
    }
    for (unsigned int ii=0; ii<mzs.size(); ii++){
        double val = mzs[ii];    
        unsigned long long vali = (unsigned long long&)val;
        fprintf_s(outFile, "0x%llx\n", vali);
    }
    fflush(outFile); fclose_safe(outFile);
    return true;
}

// write double vect to file
bool PicoUtil::writeDoubleVectToFile(const std::vector<double> &mzs, const string &out_filename)
{
    FILE* outFile = NULL;
    errno_t err = fopen_s(&outFile, out_filename.c_str(), "w");
    if (err != 0){
        cout << "can't open file = " << out_filename << endl; return false;
    }
    for (unsigned int ii = 0; ii<mzs.size(); ii++){
        double val = mzs[ii];
        fprintf(outFile, "%17.12f\n", val);
    }
    fflush(outFile); fclose_safe(outFile);
    return true;
}

// write uint mzs to file
bool PicoUtil::writeMzsToFile(const std::vector<unsigned int> &mzs, const wstring &out_filename)
{
    FILE* outFile = NULL;
    errno_t err = _wfopen_s(&outFile, out_filename.c_str(), L"w");
    if (err != 0){
        wcerr << "can't open file = " << out_filename << endl;
        return false;
    }
    for (unsigned int ii=0; ii<mzs.size(); ii++)
        fprintf_s(outFile, "%d\n", int(mzs[ii]));
    fflush(outFile); fclose_safe(outFile);
    return true;
}

// write ull mzs to file
bool PicoUtil::writeULLMzsToFile(const std::vector<unsigned long long> &mzs, const wstring &out_filename)
{
    FILE* outFile = NULL;
    errno_t err = _wfopen_s(&outFile, out_filename.c_str(), L"w");
    if (err != 0){
        wcerr << "can't open file = " << out_filename << endl;
        return false;
    }
    for (unsigned int ii=0; ii<mzs.size(); ii++)
        fprintf_s(outFile, "\"%llx\"\n", mzs[ii]);
        //fprintf(outFile, "0x%llx\n", mzs[ii]);
    fflush(outFile); fclose_safe(outFile);
    return true;
}

// write uint mzs to file
bool PicoUtil::writeULMzsToFile(const std::vector<unsigned int> &mzs, const wstring &out_filename)
{
    FILE* outFile = NULL;
    errno_t err = _wfopen_s(&outFile, out_filename.c_str(), L"w");
    if (err != 0){
        wcerr << "can't open file = " << out_filename << endl;
        return false;
    }
    for (unsigned int ii=0; ii<mzs.size(); ii++)
        fprintf(outFile, "0x%x\n", mzs[ii]);
    fflush(outFile); fclose_safe(outFile);
    return true;
}

// write int to file
bool PicoUtil::writeIntToFile(const std::vector<int> &mzs, const wstring &out_filename)
{
    FILE* outFile = NULL;
    errno_t err = _wfopen_s(&outFile, out_filename.c_str(), L"w");
    if (err != 0){
        wcerr << "can't open file = " << out_filename << endl; 
        return false;
    }
    for (unsigned int ii=0; ii<mzs.size(); ii++)
        fprintf(outFile, "%d\n", mzs[ii]);
    fflush(outFile); fclose_safe(outFile);
    return true;
}

// write int to file 
bool PicoUtil::writeUIntToFile(const std::vector<unsigned int> &mzs, const string &out_filename)
{
    FILE* outFile = NULL;
    errno_t err = fopen_s(&outFile, out_filename.c_str(), "w");
    if (err != 0){
        cout << "can't open file = " << out_filename << endl; return false;
    }
    for (unsigned int ii = 0; ii<mzs.size(); ii++)
        fprintf(outFile, "%d\n", mzs[ii]);
    fflush(outFile); fclose_safe(outFile);
    return true;
}

// write strings to file
bool PicoUtil::writeStringsToFile(const std::vector<string> &mzs, const wstring &out_filename)
{
    FILE* outFile = NULL;
    errno_t err = _wfopen_s(&outFile, out_filename.c_str(), L"w");
    if (err != 0){
        wcerr << "can't open file = " << out_filename << endl;
        return false;
    }
    for (unsigned int ii=0; ii<mzs.size(); ii++)
        fprintf(outFile, "%s\n", mzs[ii].c_str());
    fflush(outFile); fclose_safe(outFile);
    return true;
}


// append string to file
bool PicoUtil::appendStringToFile(const wstring &str, const wstring &out_filename)
{
    FILE* outFile = NULL;
    errno_t err = _wfopen_s(&outFile, out_filename.c_str(), L"a");
    if (err != 0){
        wcerr << "can't open file = " << out_filename << endl;
        return false;
    }
    fprintf(outFile, "%ls\n", str.c_str());
    fflush(outFile); fclose_safe(outFile);
    return true;
}

// read double+int data from csv file
std::vector<double> PicoUtil::readDoubleDoubleFromCsvFile(const wstring &filename)
{
    std::vector<double> vals;
    FILE* inFile = NULL;
    errno_t err = _wfopen_s(&inFile, filename.c_str(), L"r");
    if (err != 0){
        wcerr << "can't open file = " << filename << endl;
        if (err==13) {
            cout << "is it open in Excel?" << endl;
        }
        return vals;
    }
    double val = 0; int vali = 0; char line[128];
    while( fgets(line, 128, inFile) ){
        char * ptr = strchr(line,',');
        if(1==sscanf_s(line, "%lf", &val) )
            vals.push_back(val); 
        
        if(1==sscanf_s(++ptr, "%d", &vali) )
            vals.push_back((double)vali);
    }
    fclose_safe(inFile); 
    return vals;
}

// remove folder
bool PicoUtil::deleteFolder (const wstring &directory_name) {
    struct _wdirent* ep;
    WCHAR p_buf[512] = {0};
    _WDIR* dp = _wopendir(directory_name.c_str());
    if (!dp){
        return false;
    }
    while ((ep = _wreaddir(dp)) != NULL) {
        //sprintf(p_buf, "%s/%s", directory_name, ep->d_name);
        if (isDirectory(p_buf))
            deleteFolder(p_buf);
        else
            if (_wunlink(p_buf) != 0){
                wclosedir(dp); 
                return false;
            }                
    }
    _wclosedir(dp);
    if (_wrmdir(directory_name.c_str())!=0)
        return false;
    return true;
}


// check if path is a directory
int PicoUtil::isDirectory (const std::wstring &path) {
    struct _stat64 s_buf;
    if (_wstat64(path.c_str(), &s_buf))
        return 0;
    return S_ISDIR(s_buf.st_mode);
}

// get file size (in bytes)
long long PicoUtil::getFileSize(const std::wstring &in_filename)
{
    struct _stat64 stat;
    if (_wstat64(std::wstring(in_filename.begin(), in_filename.end()).c_str(), &stat) < 0){
        wcerr << "can't find input file = " << in_filename << endl;
        return -1;
    }
    return stat.st_size;
}

long long PicoUtil::getDirSize(const std::wstring &dir)
{
    //namespace bf=boost::filesystem;
    long long size=0;
    std::vector<wstring> list = dirList(dir);
    for (unsigned int ii=0; ii<list.size(); ii++){
        wstring filename = dir + L"\\" + list[ii];
        size += getFileSize(filename);
    }

   // for(bf::recursive_directory_iterator it("path"); it!=bf::recursive_directory_iterator(); ++it){
   //     if(!is_directory(*it))
  //          size += (size_t)bf::file_size(*it);
  //  }
    return size;
}

// find string in list
int PicoUtil::findStringInList(const std::wstring &name, const std::vector<wstring> &list){
    //bool found = false;
    for (unsigned int jj=0; jj<list.size(); jj++){
        if (list[jj].find(name)!=std::string::npos)
            return jj;
    }
    return -1;
}

// commas
string PicoUtil::commas(unsigned long long n)
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

_PICO_END

