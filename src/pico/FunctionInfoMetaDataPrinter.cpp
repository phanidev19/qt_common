#include "FunctionInfoMetaDataPrinter.h"


FunctionInfoMetaDataPrinter::FunctionInfoMetaDataPrinter()
{
}

void FunctionInfoMetaDataPrinter::pushFunctionInfoMetaData(const FunctionRec & rec) {
    functionInfoMetaData.push_back(rec);
}

void FunctionInfoMetaDataPrinter::pushCalibrationInfo(int function, bool info) {
    for (int i = 0; i < (int)functionInfoMetaData.size(); i++) {
        if (functionInfoMetaData[i].function == function) {
            functionInfoMetaData[i].calibration = info;
        }
    }
}

bool FunctionInfoMetaDataPrinter::writeToFile(const std::wstring &filename) {
    std::wifstream in_file(filename);
    // Check if file is pre-existing to decide whether or not to write header
    bool writeHeader = !in_file.good();
    if (in_file.good()) {
        in_file.close();
    }
    std::wofstream file;
    file.open(filename, std::wofstream::app);
    if (file.is_open()) {
        // Header
        if (writeHeader) {
            file << "filename,function,hex1,hex2,continuum,function_type,ion_mode,data_type,calibration\n";
        }
        for (int i = 0; i < (int)functionInfoMetaData.size(); i++) {
            file << functionInfoMetaData[i].filename << "," << functionInfoMetaData[i].function << "," 
            << std::hex << functionInfoMetaData[i].type << std::dec << ","
            << std::hex << functionInfoMetaData[i].mode << std::dec << ","
            << functionInfoMetaData[i].continuum << "," << functionInfoMetaData[i].function_type << ","
            << functionInfoMetaData[i].ion_mode << "," << functionInfoMetaData[i].data_type << ","
            << functionInfoMetaData[i].calibration << "\n";
        }
    }
    file.close();
    return true;
}

FunctionInfoMetaDataPrinter::~FunctionInfoMetaDataPrinter()
{
}
