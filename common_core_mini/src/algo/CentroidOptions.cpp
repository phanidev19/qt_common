#include "CentroidOptions.h"
#include "RowNodeOptionsCore.h"
#include "PmiQtCommonConstants.h"

#include <math_utils.h>
#include <QtSqlUtils.h>
#include <qt_string_utils.h>

_PMI_BEGIN

void CentroidOptions::initOptions(RowNode &options) {
    pmi::rn::createOptions(VariantItems() << kCentroidMethod << kCentroidSmoothWidth << kCentroidSmoothType << kCentroidKeepPeaksTopK,
                           VariantItems() << 2 << 0.02 << 0 << 1000,
                           options);
}

void CentroidOptions::saveToRowNode(RowNode & options) const
{
    pmi::rn::createOptions(VariantItems() << kCentroidMethod << kCentroidSmoothWidth << kCentroidSmoothType << kCentroidKeepPeaksTopK,
                           VariantItems() << m_method << m_smoothingWidth << m_smoothingType << m_keepPeaksTopK,
                           options);
}

void CentroidOptions::loadFromRowNode(const RowNode & options)
{
    m_method = (CentroidMethod) rn::getOptionWithDefault(options, kCentroidMethod, m_method ).toInt();
    m_smoothingType = (CentroidSmoothingType) rn::getOptionWithDefault(options, kCentroidSmoothType, m_smoothingType).toInt();
    m_smoothingWidth = rn::getOptionWithDefault(options, kCentroidSmoothWidth, m_smoothingWidth).toDouble();
    m_keepPeaksTopK = rn::getOptionWithDefault(options, kCentroidKeepPeaksTopK, m_keepPeaksTopK).toInt();
}


Err CentroidOptions::saveOptionsToDatabase(const QString &filename) const
{
    Err e = kNoErr;

    {
        QSqlDatabase db;
        e = addDatabaseAndOpen("CentroidOptions_saveOptionsToDatabase", filename, db); eee;
        e = saveOptionsToDatabase(db); eee;
    }

error:
    QSqlDatabase::removeDatabase("CentroidOptions_saveOptionsToDatabase");
    return e;
}

Err CentroidOptions::saveOptionsToDatabase(QSqlDatabase & db) const
{
    Err e = kNoErr;
    RowNode options;

    saveToRowNode(options);
    e = rn::createFilterOptionsTable(db, kProjectCreationOptions); eee;
    e = rn::saveOptions(db, kProjectCreationOptions, kCentroidingOptions, options); eee;

error:
    return e;
}

Err CentroidOptions::loadOptionsFromDatabase(QSqlDatabase & db, bool * ok)
{
    Err e = kNoErr;
    RowNode options;

    if (!containsTable(db, kProjectCreationOptions)) {
        *ok = false;
        return e;
    }

    initOptions(options);  //add default values to prevent error messages

    e = rn::loadOptions(db, kProjectCreationOptions, kCentroidingOptions, options); eee;
    loadFromRowNode(options);
    *ok = true;

error:
    return e;
}

bool CentroidOptions::matches(const CentroidOptions & obj) const
{
    return this->m_keepPeaksTopK == obj.m_keepPeaksTopK
            && this->m_method == obj.m_method
            && this->m_smoothingType == obj.m_smoothingType
            && fsame(this->m_smoothingWidth, obj.m_smoothingWidth, 0.0001);
}


Err CentroidOptions::matchesDatabase(QString filename, bool * match_val) const
{
    Err e = kNoErr;

    {
        QSqlDatabase db;
        e = addDatabaseAndOpen("CentroidOptions_matchesDatabase", filename, db); eee;
        e = matchesDatabase(db, match_val); eee;
    }

error:
    QSqlDatabase::removeDatabase("CentroidOptions_matchesDatabase");
    return e;
}

Err CentroidOptions::matchesDatabase(QSqlDatabase & db, bool * match_val) const
{
    Err e = kNoErr;
    CentroidOptions tmp;
    bool ok = false;

    *match_val = false;
    e = tmp.loadOptionsFromDatabase(db, &ok);
    if (!ok) {
        *match_val = false;
    } else {
        *match_val = this->matches(tmp);
    }

//error:
    return e;
}

QString CentroidOptions::buildMSConvertGaussFilterString() const {
    QString filter_template = QString("peakPicking gauss:%1:%2:%3 1-");
    QString gauss_width = QString("%1").arg(m_smoothingWidth, 0, 'f', 4).trimmed();

    QString filterStr = filter_template.arg(gauss_width).arg(m_smoothingType).arg(m_keepPeaksTopK);
    debugCoreMini() << "filterStr = " << filterStr;
    return filterStr;
}

QStringList CentroidOptions::buildPicoConsoleArguments() const {
    QStringList list;

    list << "-uv" << QString::number(m_smoothingWidth);

    list << "-uf";
    if (m_smoothingType == CentroidSmoothingType_Constant) {
        list << "c";
    } else if (m_smoothingType == CentroidSmoothingType_Square) {
        list << "s";
    } else {
        list << "l";
    }

    list << "-ut" << QString::number(m_keepPeaksTopK);

    if (m_keepPeaksTopK == 1001) {
        list << "-ua" << "1";
    }

    return list;
}

_PMI_END

