#ifndef __PICO_COMMON_H__
#define __PICO_COMMON_H__

#define _PICO_BEGIN namespace pico {
#define _PICO_END }

_PICO_BEGIN

enum CompressType {
    Type_Bruker1 = 0,
    Type_Waters1 = 1,
    Type_Bruker2 = 2,
    Type_Centroided1 = 3,
    Type_ABSciex1 = 4
};

#define BRUKER_PROFILE_INFO_ID 1 // bruker profile tag
#define BRUKER_CENTROID_INFO_ID 2 // bruker centroided tag
#define BRUKER_CENTROID_NO_COMPRESSION_ID 3 // bruker generic no-centroid-compression tag
#define WATERS_READER_INFO_ID 101 // waters reader tag
#define WATERS_PWIZ_INFO_ID 102 // waters reader tag
#define WATERS_READER_CENTROIDED_INFO_ID 2 // waters reader centroided tag -- same as bruker centroided
#define WATERS_CENTROID_NO_COMPRESSION_ID 3 // waters generic no-centroid-compression tag -- same as bruker generic no-centroid-compression tag

_PICO_END

#endif

