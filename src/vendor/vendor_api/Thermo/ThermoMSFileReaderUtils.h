/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef __THERMO_MSFileReaderUtils_H__
#define __THERMO_MSFileReaderUtils_H__

#include "ThermoAPITools.h"
#include <common_errors.h>
#include <common_types.h>
#include <QtSqlUtils.h>

_PMI_BEGIN

struct PrecursorInfo;

_MSREADER_BEGIN

struct XICWindow;

struct LabelValueString
{
    std::string label;
    std::string value;
};

typedef void * RawFileHandle;

enum ThermoControllerType {
     ThermoControllerMS = 0
    ,ThermoControllerAnalog
    ,ThermoControllerADCard
    ,ThermoControllerPDA
    ,ThermoControllerUV
};

enum ThermoMSChroType {
     ThermoMSMassRange=0
    ,ThermoMSTIC
    ,ThermoMSBasePeak
};

enum ThermoPDAChroType {
     ThermoPDAChroWavelengthRange=0
    ,ThermoPDAChroTotalScan
    ,ThermoPDAChroSpectrumMaximum
};

enum ThermoUVChroType {
     ThermoUVChannelA=0
    ,ThermoUVChannelB
    ,ThermoUVChannelC
    ,ThermoUVChannelD
};

enum ThermoAnalogType {
     ThermoAnalog1=0
    ,ThermoAnalog2
    ,ThermoAnalog3
    ,ThermoAnalog4
};

//Analog to digital converter
enum ThermoADCardType {
     ThermoADCardCh1=0
    ,ThermoADCardCh2
    ,ThermoADCardCh3
    ,ThermoADCardCh4
};

Err CreateRawInstanceAndOpenFile(const std::string & fileName, RawFileHandle * rawHandle, long nControllerType, long nControllerNumber);
Err CloseAndDestroyRawInstance(RawFileHandle & rawHandle);
Err getRawScanNumberToPointListSortByX(RawFileHandle rawHandle, long scanNumber, std::vector<point2d> & pointList);
Err getChroPoints(RawFileHandle rawHandle, long channel, std::vector<point2d> & pointList, bool sortByX);
Err SetRawCurrentController(RawFileHandle rawHandle, long nControllerType, long nControllerNumber);
Err getTICPointList_AutoSetCurrentController(RawFileHandle rawHandle, std::vector<point2d> & pointList, bool sortByX);
Err GetNumberOfControllersOfType(RawFileHandle rawHandle, long nControllerType, long * nControllers);

Err getRawScanNumberToPointListSortByX_centroid(RawFileHandle rawHandle, thermo::InstrumentModelType modelType, long scanNumber, std::vector<point2d> & pointList);

Err extractRAWXIC(RawFileHandle rawHandle, const XICWindow & xnfo, std::vector<point2d> & pointList);
Err getPrecursorInfo(RawFileHandle rawHandle, long scan_num, PrecursorInfo & xpreInfo);
Err getMSLevel(RawFileHandle rawHandle, long scan_num, int * level, std::string * precursorMassStr, std::string * peakModeStr, std::string * scanMethodStr);
Err getRT(RawFileHandle rawHandle, long scan_num, double* dRT);
Err RAW_getNumberOfSpectra(RawFileHandle rawHandle, long * totalNumber, long * startScan, long * endScan);
Err RAW_getFragmentType(RawFileHandle rawHandle, long scan_num, long level, QString &fragmentTypeStr);
Err getInstrumentModel(RawFileHandle rawHandle, thermo::InstrumentModelType & modelType, std::string & instModel);
Err GetTuneData(RawFileHandle rawHandle, std::vector<LabelValueString> & values);
Err getScanNumberFromRT(RawFileHandle rawHandle, double retTime, long * scanNumber);

//long nControllerType=0, nControllerNumber=1; //For RAW MS1/MS2/TIC reading
//long nControllerType=4, nControllerNumber=1; //For UV reading; need to understand this better later.

extern const long ticControllerType;
extern const long ticControllerNumber;

extern const long uvControllerType;
extern const long uvControllerNumber;

QString makeThermoNativeId(long scanNumber);

_MSREADER_END
_PMI_END

#endif
