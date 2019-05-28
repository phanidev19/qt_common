#ifndef PICO_DECOMPRESS_QTUSE_H
#define PICO_DECOMPRESS_QTUSE_H

#include "PicoLocalDecompress.h"
#include "config-pmi_qt_common.h"
#include "pmi_common_ms_export.h"
#include "CompressionInfo.h"
#include "QtSqlUtils.h" 
#include "common_errors.h"
#include "string_utils.h"

_PICO_BEGIN

// parse calibration linebool
inline bool parseCalibrationLine(const std::string & text, WatersCalibration::Coefficents * coef) {
    std::vector<double> & coeffients = coef->coeffients;
    coeffients.clear();
    WatersCalibration::CoefficentsType & type = coef->type;

    std::vector<std::string> vstr = pmi::split(text, ",", true);

    for (int ii = (int)vstr.size() - 1; ii >= 0; ii--){
        std::string sti = vstr[ii];
        if (sti.size() > 1){
            if (sti[0] == 'T'){
                if (sti[1] == '0')
                    type = WatersCalibration::CoefficentsType::CoefficentsType_T0;
                else if (sti[1] == '1')
                    type = WatersCalibration::CoefficentsType::CoefficentsType_T1;
                else
                    type = WatersCalibration::CoefficentsType::CoefficentsType_None;
            }
            vstr.erase(vstr.begin() + ii); break;
        }
    }

    if (vstr.size() > 6){
        std::cout << "too many calib coefficients" << std::endl; return false;
    }

    for (unsigned int ii = 0; ii < vstr.size(); ii++)
        coeffients.push_back(atof(vstr[ii].c_str()));
    return true;
}

inline void getCoefficientInformation(const QString & functionNumber, const QSqlDatabase & db, WatersCalibration::Coefficents * coef) {
    if (functionNumber.isEmpty()) {
        return;
    }
    QSqlQuery q = pmi::makeQuery(db, true);
    QString rawCmd = "SELECT k.Key, k.Value FROM FilesInfo k WHERE k.Key == 'Cal Modification " + functionNumber + "'";
    pmi::QEXEC_CMD(q, rawCmd); //eee;
    QString mod_text = "";
    if(!q.next()) {
        DEBUG_WARNING_LIMIT(qDebug() << "Coefficient information couldn't be retrieved for function " << functionNumber, 5);
    } else {
        mod_text = q.value(1).toString();
    }
    pico::parseCalibrationLine(mod_text.toStdString(), coef);
}

// change to not return qstring (through *)
inline QString getFunctionNumberFromNativeId(const QString & nativeId) {
    int firstEqualsIndex = nativeId.indexOf("=");
    int firstSpaceIndex = nativeId.indexOf(" ");
    int characterLenOfFunctionNum = firstSpaceIndex - firstEqualsIndex - 1;
    QString functionNumberSubStr = nativeId.mid(firstEqualsIndex + 1, characterLenOfFunctionNum);
    return functionNumberSubStr;
}

class PMI_COMMON_MS_EXPORT PicoLocalDecompressQtUse : public PicoLocalDecompress
{
public:
    // Constructor:
    PicoLocalDecompressQtUse() {
    }

    ~PicoLocalDecompressQtUse() {
    }

    static CompressionType getCompressionType(QString compressionProperty, QString compressionVersion);

    bool decompress(const WatersCalibration::Coefficents & coef, const CompressionInfo* compressionInfo, const QByteArray &compressedMzByteArray, const QByteArray &compressedIntensityByteArray,
                                         double** restored_mz, float** restored_intensity, int* length) const
    {
        return decompress(coef, compressionInfo->GetProperty(), compressionInfo->GetVersion(), compressedMzByteArray, compressedIntensityByteArray, restored_mz,
                          restored_intensity, length);
    }

    bool decompress(const WatersCalibration::Coefficents & coef, QString compressionProperty, QString compressionVersion, const QByteArray &compressedMzByteArray,
                                         const QByteArray &compressedIntensityByteArray, double** restored_mz, float** restored_intensity, int* length) const
    {
        CompressionType compressionType = getCompressionType(compressionProperty, compressionVersion);

        return decompress(coef, compressionType, compressedMzByteArray, compressedIntensityByteArray, restored_mz, restored_intensity, length);
    }

    bool decompress(const WatersCalibration::Coefficents & coef, const CompressionType compressionType, const QByteArray &compressedMzByteArray, const QByteArray &compressedIntensityByteArray,
                                         double** restored_mz, float** restored_intensity, int* length) const
    {
        unsigned char* compressedMz = (unsigned char*)compressedMzByteArray.data();
        unsigned char* compressedIntensity = (unsigned char*)compressedIntensityByteArray.data();

        return decompress(coef, compressionType, &compressedMz, &compressedIntensity, restored_mz, restored_intensity, length);
    }

    bool decompress(const WatersCalibration::Coefficents & coef, const CompressionType compressionType, unsigned char** compressedMz, unsigned char** compressedIntensity,
                                         double** restored_mz, float** restored_intensity, int* length) const
    {
        bool restore_zero_peaks = true;
        bool file_decompress = false;  ///!!!! double check
        switch(compressionType) {
            case Unknown:
                return false;
            case Bruker_Type0:
                return decompressSpectraType0(compressedMz, restored_mz, restored_intensity, length);
            case Centroid_Type1:
                return decompressSpectraCentroidedType1(compressedMz, compressedIntensity, restored_mz, restored_intensity, length);
            case Waters_Type1:
                return decompressRawType1_N(*compressedMz, &coef, restored_mz, restored_intensity, length, restore_zero_peaks, file_decompress);
        }
        return false;
    }
};

_PICO_END

#endif
