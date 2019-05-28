#ifndef __CENTROID_OPTIONS_H__
#define __CENTROID_OPTIONS_H__

#include <pmi_core_defs.h>
#include "RowNodeOptionsCore.h"

class QSqlDatabase;

_PMI_BEGIN

class PMI_COMMON_CORE_MINI_EXPORT CentroidOptions {
public:
    enum CentroidMethod {
         CentroidMethod_PreferVendor = 0
        ,CentroidMethod_UseCustom
        ,CentroidMethod_Auto
    };

    enum CentroidSmoothingType {
         CentroidSmoothingType_Constant = 0
        ,CentroidSmoothingType_Square
        ,CentroidSmoothingType_Linear
    };

    CentroidOptions() {
        initWithDefault();
    }

    void set(CentroidMethod method, CentroidSmoothingType smoothingType, double smoothingWidth, int topK) {
        m_method = method;
        m_smoothingType = smoothingType;
        m_smoothingWidth = smoothingWidth;
        m_keepPeaksTopK = topK;
    }
    
    CentroidMethod getCentroidMethod() const {
        return m_method;
    }

    CentroidSmoothingType getSmoothingType() const {
        return m_smoothingType ;
    }

    double getSmoothingWidth() const {
        return m_smoothingWidth;
    }

    int getKeepPeaksTopK() const {
        return m_keepPeaksTopK;
    }

    static void initOptions(RowNode & options);

    void loadFromRowNode(const RowNode & options);

    void saveToRowNode(RowNode & options) const;

    void initWithDefault() {
        m_method = CentroidMethod_Auto;
        m_smoothingType = CentroidSmoothingType_Constant;
        m_smoothingWidth = 0.02;
        m_keepPeaksTopK = 1000;
    }

    QStringList buildPicoConsoleArguments() const;

    QString buildMSConvertGaussFilterString() const;

    Err saveOptionsToDatabase(const QString &filename) const;

    Err saveOptionsToDatabase(QSqlDatabase & db) const;
    Err loadOptionsFromDatabase(QSqlDatabase & db, bool * ok);

    Err matchesDatabase(QString filename, bool * match_val) const;
    Err matchesDatabase(QSqlDatabase & db, bool * match_val) const;

    bool matches(const CentroidOptions & obj) const;

private:
    CentroidMethod m_method = CentroidMethod_Auto;
    CentroidSmoothingType m_smoothingType = CentroidSmoothingType_Constant;
    double m_smoothingWidth = 0.02;
    int m_keepPeaksTopK = 1000;
};

_PMI_END

#endif
