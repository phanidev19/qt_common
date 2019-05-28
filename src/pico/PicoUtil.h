// ********************************************************************************************
// PicoUtil.cpp : A collection of utility functions
//
//      @Author: D.Kletter    (c) Copyright 2014, PMI Inc.
// 
// This class provides a collection of generic utility functions such as file and directory services.
//
//
// *******************************************************************************************

#ifndef PICO_UTIL_H
#define PICO_UTIL_H

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <boost/algorithm/string.hpp>
#include "PicoCommon.h"

typedef std::vector<std::string> vecstr;

_PICO_BEGIN

class PicoUtil
{
public:
    // Constructor:
    PicoUtil();

    // Destructor:
    ~PicoUtil();

    // ex. C:\hi\file.csv -> csv
    std::wstring getFileExtension(const std::wstring & filename);

    // ex. C:\hi\file.csv -> C:\hi\file
    std::wstring removeFileExtension(const std::wstring &filename);

    // gather a list of files in a directory
    std::vector<std::wstring> dirList(const std::wstring &directory);

    // gather a list of specified extension files in a directory
    std::vector<std::wstring> dirListExtension(const std::wstring &directory, const std::wstring &extension);

    // get file name (Has path char from path) ex. C:\hi\file.csv -> \file.csv
    std::wstring getName(const std::wstring &filename);

    // ex. C:\hi\file.csv -> file.csv
    std::wstring getNameNoSlashes(const std::wstring &filename);

    // get file path ex. C:\hi\file.csv -> C:\hi
    std::wstring getPath(const std::wstring &filename);
    
    // get file path with end path char ex. 'C:\hi\file.csv' -> 'C:\hi\'
    std::wstring getPathWithEndSlashes(const std::wstring &filename);

    // ex. getPathWithPrefixOnFilename("C:\hi\file.csv", "prefix_") -> "C:\hi\prefix_file.csv"
    std::wstring getPathWithPrefixOnFilename(const std::wstring &fullPathFilename, const std::wstring &prefix);

    // strip last path char
    std::wstring stripPathChar(const std::wstring &dir);

    // check if file exists
    bool fileExists(const std::wstring &filename);

    // check if directory exists
    bool dirExists(const std::wstring &dir);

    // create directory with all necessary sub-dirs
    bool createDir(const std::wstring &dir);

    // remove directory and all files
    bool rmdir_r(const std::wstring &dir);

    // copy file to a directory
    bool fileCopy(const std::wstring &filename, const std::wstring &to_dir);

    // write filename to file
    bool writeFileName(const std::wstring &name, const std::wstring &out_filename);

    // read filename from file
    std::wstring readFileName(const std::wstring &filename);

    // read csv data from file
    std::vector<unsigned int> readCsvFileUInt(const std::wstring &filename);

    // read double csv data from file
    std::vector<double> readCsvFileDouble(const std::wstring &filename);

    // write double mzs to file
    bool writeDoubleMzsToFile(const std::vector<double> &mzs, const std::wstring &out_filename);

    // write double vect to file
    bool writeDoubleVectToFile(const std::vector<double> &mzs, const std::string &out_filename);

    // write uint mzs to file
    bool writeMzsToFile(const std::vector<unsigned int> &mzs, const std::wstring &out_filename);

    // write ull mzs to file
    bool writeULLMzsToFile(const std::vector<unsigned long long> &mzs, const std::wstring &out_filename);

    // write uint mzs to file
    bool writeULMzsToFile(const std::vector<unsigned int> &mzs, const std::wstring &out_filename);

    // write int to file
    bool writeIntToFile(const std::vector<int> &mzs, const std::wstring &out_filename);

    // write uint to file 
    bool writeUIntToFile(const std::vector<unsigned int> &mzs, const std::string &out_filename);

    // write strings to file
    bool writeStringsToFile(const std::vector<std::string> &mzs, const std::wstring &out_filename);

    // append std::string to file
    bool appendStringToFile(const std::wstring &str, const std::wstring &out_filename);

    // read double+int data from csv file
    std::vector<double> readDoubleDoubleFromCsvFile(const std::wstring &filename);

    // check if path is a directory
    int isDirectory(const std::wstring &path);

    // remove folder
    bool deleteFolder(const std::wstring &directory_name);

    // get file size (in bytes)
    long long getFileSize(const std::wstring &in_filename);

    // get directory size (in bytes)
    long long getDirSize(const std::wstring &dir);

    //find std::string in list
    int findStringInList(const std::wstring &name, const std::vector<std::wstring> &list);

    // commas
    std::string commas(unsigned long long n);

    // from string_utils.h
    inline void
        tokenize(const std::string& str, vecstr& tokens, const std::string& delimiters = " ", bool trimEmpty = false) {
        std::string::size_type pos, lastPos = 0;
        tokens.clear();

        //bugfix: when an empty string gets passed, token has one entry.
        //411: consider more generic fix or other potential problems with this fix.
        if (str.length() <= 0)
            return;

        while (true) {
            pos = str.find_first_of(delimiters, lastPos);
            if (pos == std::string::npos) {
                pos = str.length();

                if (pos != lastPos || !trimEmpty)
                    tokens.push_back(vecstr::value_type(str.data() + lastPos,
                    (vecstr::value_type::size_type)pos - lastPos));
                break;
            }
            else {
                if (pos != lastPos || !trimEmpty)
                    tokens.push_back(vecstr::value_type(str.data() + lastPos,
                    (vecstr::value_type::size_type)pos - lastPos));
            }
            lastPos = pos + 1;
        }
    }

    inline vecstr
        split(const std::string & str, const std::string& delimiters = " ", bool trimEmpty = false) {
        vecstr strlist;
        tokenize(str, strlist, delimiters, trimEmpty);
        return strlist;
    }



private:

};

void fclose_safe(FILE* & fp);
void free_safe(unsigned char * & ptr);
void free_safe(double * & ptr);
void free_safe(float * & ptr);
void free_safe(char * & ptr);
void free_safe(unsigned int * & ptr);

// toString Conversion code
inline std::string toString(double number, int precision = 16)
{
    char format[128], buff[1028];
#ifdef _WIN32
    sprintf_s(format, 128, "%%.%df", precision);
    sprintf_s(buff, 1028, format, number);
#else
    snprintf(format, 128, "%%.%df", precision);
    snprintf(buff, 1028, format, number);
#endif
    return std::string(buff);
}

inline std::string toString(float number, int precision = 7)
{
    char format[128], buff[1028];
#ifdef _WIN32
    sprintf_s(format, 128, "%%.%df", precision);
    sprintf_s(buff, 1028, format, number);
#else
    snprintf(format, 128, "%%.%df", precision);
    snprintf(buff, 1028, format, number);
#endif
    return std::string(buff);
}


inline std::string toString(int number)
{
    std::stringstream ss;
    ss << number;
    return ss.str();
}

inline std::string toString(long number)
{
    std::stringstream ss;
    ss << number;
    return ss.str();
}

// toString Conversion code
inline std::wstring toWString(double number, int precision = 16)
{
    wchar_t format[128], buff[1028];
#ifdef _WIN32
    swprintf_s(format, 128, L"%%.%df", precision);
    swprintf_s(buff, 1028, format, number);
#else
    snwprintf(format, 128, L"%%.%df", precision);
    snwprintf(buff, 1028, format, number);
#endif
    return std::wstring(buff);
}

inline std::wstring toWString(float number, int precision = 7) {
    wchar_t format[128], buff[1028];
#ifdef _WIN32
    swprintf_s(format, 128, L"%%.%df", precision);
    swprintf_s(buff, 1028, format, number);
#else
    snwprintf(format, 128, L"%%.%df", precision);
    snwprintf(buff, 1028, format, number);
#endif
    return std::wstring(buff);
}

inline std::wstring toWString(int number)
{
    std::wstringstream ss;
    ss << number;
    return ss.str();
}

inline std::wstring toWString(long number)
{
    std::wstringstream ss;
    ss << number;
    return ss.str();
}

inline std::wstring replaceLastCharWithNum(const std::wstring & str, unsigned int num) {
    std::wstringstream ss; ss << num;
    std::wstring new_str = str.substr(0, str.size() - wcslen(ss.str().c_str()));
    new_str.append(ss.str());
    return new_str;
}

inline std::wstring appendZeroPaddedNumber(const std::wstring & str, unsigned int numberWidth, int num) {
    std::wstringstream ss; 
    ss << std::setw(numberWidth) << std::setfill(L'0') << num;
    return str + ss.str();
}

inline std::wstring appendZeroPaddedNumberForWatersFile(const std::wstring & prefix, int num) {
    std::wstringstream ss;
    std::wstring new_str = L"";
    if (num < 100) {
        new_str = appendZeroPaddedNumber(prefix, 3, num);
    } else {
        // denotes the exponent when placed into scientific notation
        int exponent = (int) log10(num);
        int widthOfNumber = exponent + 1;
        int widthNeededToPadWithOneExtraLeadingZero = widthOfNumber + 1;
        new_str = appendZeroPaddedNumber(prefix, widthNeededToPadWithOneExtraLeadingZero, num);
    }
    return new_str;
}

inline bool iequals(const std::wstring & strA, const std::wstring & strB) {
    return boost::iequals(strA, strB);
}

_PICO_END

#endif
