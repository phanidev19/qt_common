/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */


#include "EntryDefMap.h"
#include "pmi_common_core_mini_debug.h"

#include <QTextStream>
#include <QFile>
#include <QStringList>

/*!
 * \brief ColumnFriendlyName::m_names
 */
QMap<QString,ColumnFriendlyName::FriendlyNameStruct> ColumnFriendlyName::m_names;

//TODO: remove this one? 2014-08-24; didn't really check
QMap<QString,ColumnFriendlyName::FriendlyNameStruct> ColumnFriendlyName::m_rev_names;

ColumnFriendlyName::DocumentType ColumnFriendlyName::m_current_doc_type = ColumnFriendlyName::Unknown;

QMap<ColumnFriendlyName::DocumentType, ColumnFriendlyName::Setter> ColumnFriendlyName::m_setters;

bool ColumnFriendlyName::m_isIntactDocument;

void ColumnFriendlyName::saveToCSV(QString filename)
{
    QFile file(filename);
    if(!file.open(QIODevice::WriteOnly)) {
        debugCoreMini() << "Could not write to file:" << filename;
        return;
    }

    QTextStream outf(&file);

    QStringList header;
    header << "InternalName" << "Name(no new line)" << "Details (no new line)" << "Name (with newline)" << "Details (with newline)" << "GroupMethod" << "ChildrenCollectionMethod";
    outf << header.join(",") << endl;

    QStringList keys = m_names.keys();
    foreach(const QString key, keys) {
        ColumnFriendlyName::FriendlyNameStruct obj = m_names[key];
        outf << key;
        outf << "," << "\"" << obj.m_name.replace("\"","").replace("\n"," ") << "\"";
        outf << "," << "\"" << obj.m_detail.replace("\"","'").replace("\n"," ") << "\"";
        outf << "," << "\"" << obj.m_name << "\"";
        outf << "," << "\"" << obj.m_detail.replace("\"", "'") << "\"";
        outf << "," << "\"" << obj.m_groupMethod << "\"";
        outf << "," << "\"" << obj.m_childCollectionMethod << "\"";
        outf << endl;
    }
}

QVariant ColumnFriendlyName::FriendlyNameStruct::stringToValue(const QString &string, bool *ok) const
{
    QVariant result;
    switch(m_type) {
    case Type_Decimal:
    case Type_Scientific: {
        bool success;
        result = string.toDouble(&success);
        if (ok) {
            *ok = success;
        }
        if (!success) {
            return QVariant();
        }
        break;
    }
    case Type_Percent: {
        bool success;
        QString stringWithoutPercent(string);
        if (stringWithoutPercent.endsWith(QLatin1Char('%'))) {
            stringWithoutPercent.chop(1);
        }
        result = stringWithoutPercent.toDouble(&success) * 100.0;
        if (ok) {
            *ok = success;
        }
        if (!ok) {
            return QVariant();
        }
        break;
    }
    default:
        result = string;
        if (ok) {
            *ok = true;
        }
    }
    return result;
}
