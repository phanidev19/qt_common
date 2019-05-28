#ifndef __WATERS_META_READING_H__
#define __WATERS_META_READING_H__

#include <qt_string_utils.h>

#include <QHash>
#include <QString>

#include <string>
#include <unordered_map>
#include <queue>

enum Err {
    kNoErr = 0
    , kError
    , kErrorFileIO
};

/*!
* From MassLynxRaw.hpp in ProteoWizard source
*/
enum PwizFunctionType
{
    FunctionType_Scan = 0,              /// Standard MS scanning function
    FunctionType_SIR,                   /// Selected ion recording
    FunctionType_Delay,                 /// No longer supported
    FunctionType_Concatenated,          /// No longer supported
    FunctionType_Off,                   /// No longer supported
    FunctionType_Parents,               /// MSMS Parent scan
    FunctionType_Daughters,             /// MSMS Daughter scan
    FunctionType_Neutral_Loss,          /// MSMS Neutral Loss
    FunctionType_Neutral_Gain,          /// MSMS Neutral Gain
    FunctionType_MRM,                   /// Multiple Reaction Monitoring
    FunctionType_Q1F,                   /// Special function used on Quattro IIs for scanning MS1 (Q1) but uses the final detector
    FunctionType_MS2,                   /// Special function used on triple quads for scanning MS2. Used for calibration experiments.
    FunctionType_Diode_Array,           /// Diode array type function
    FunctionType_TOF,                   /// TOF
    FunctionType_TOF_PSD,               /// TOF Post Source Decay type function
    FunctionType_TOF_Survey,            /// QTOF MS Survey scan
    FunctionType_TOF_Daughter,          /// QTOF MSMS scan
    FunctionType_MALDI_TOF,             /// Maldi-Tof function
    FunctionType_TOF_MS,                /// QTOF MS scan
    FunctionType_TOF_Parent,            /// QTOF Parent scan
    FunctionType_Voltage_Scan,          /// AutoSpec Voltage Scan
    FunctionType_Magnetic_Scan,         /// AutoSpec Magnet Scan
    FunctionType_Voltage_SIR,           /// AutoSpec Voltage SIR
    FunctionType_Magnetic_SIR,          /// AutoSpec Magnet SIR
    FunctionType_Auto_Daughters,        /// Quad Automated daughter scanning
    FunctionType_AutoSpec_B_E_Scan,     /// AutoSpec_B_E_Scan
    FunctionType_AutoSpec_B2_E_Scan,    /// AutoSpec_B2_E_Scan
    FunctionType_AutoSpec_CNL_Scan,     /// AutoSpec_CNL_Scan
    FunctionType_AutoSpec_MIKES_Scan,   /// AutoSpec_MIKES_Scan
    FunctionType_AutoSpec_MRM,          /// AutoSpec_MRM
    FunctionType_AutoSpec_NRMS_Scan,    /// AutoSpec_NRMS_Scan
    FunctionType_AutoSpec_Q_MRM_Quad    /// AutoSpec_Q_MRM_Quad
};

enum PwizFunctionMode {
    FunctionMode_MSN = 113,             /// 0x71
    FunctionMode_Calibrate = 241,       /// 0xF1
    FuncitonMode_UV_Trace = 36          /// 0x24
};

enum CentroidMode {
    UndeterminedData = 0,               /// not known
    ProfileData = 1,                    /// profile mode
    CentroidData = 2                    /// centroid mode
};

inline bool testMostSignificantByteBit(const unsigned char val){
    return (val & 0x80) > 0;
}

inline bool isCalibrationFunction(const unsigned char val){
    return testMostSignificantByteBit(val);
}

class WatersMetaReading {
public:

    struct FunctionInfo {
        int functionNumber = -1;
        int functionTypeRaw = -1;
        int functionModeRaw = -1;
        int MSLevel = -1;

        bool accurate = false;
        int function_type = -1;
        int ion_mode = -1;
        int data_type = -1;
        bool continuum = false;
        CentroidMode centroid_from_observation = UndeterminedData;

        QString functionName; //E.g. "TOF FAST DDA FUNCTION"
        QHash<QString, QString> m_fieldNameToFieldValue;
        std::queue<QString> trackMetaFieldsOrder;
    };

    WatersMetaReading() {
    }

    /*!
     * Read from Waters path, e.g. "D:\doron\R-BSA digest-ms2-072415.raw"
     */
    Err readFromWatersPath(const std::wstring &path);

    /*!
    * Calculate MS Level from _extern.inf and functns.inf
    */
    int getMSLevelForFunction(int func_val) const;

    /*!
    * Create database from _extern.inf
    */
    bool writeMetaToDB_forTesting(const std::wstring &filePath, const std::wstring &outSqliteFile);

    // set function type
    bool setFunctionObservationCentroidMode(unsigned int func_val, CentroidMode centroid_mode);
 
    // get function type
    CentroidMode centroidModeFromObservation(int func_val) const;

    bool isMSE() const {
        return m_isMSE;
    }

    bool isAccurate() const {
        return m_isAccurate;
    }

private:
    /*!
    * return expected MS level
    */
    int _getMSLevelForFunction(int function_type, int data_type, int functionTypeRaw, int functionModeRaw) const;

    /*!
    * Improved Waters MSE identification
    */
    void _mseCheck();

    /*!
     * @brief Make a temporary patch to extract calibrant functions
     */
    void _tempMSLevelPatchForCalibration();

private:
    std::unordered_map<int, FunctionInfo> m_functionNumberToFunctionInfo;
    bool m_isMSE = false;
    bool m_isAccurate = false;
    std::wstring m_path;
};


#endif

