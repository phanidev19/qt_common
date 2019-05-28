/*
 * Copyright (C) 2012-2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */


#ifndef ENTRYDEFMAP_H
#define ENTRYDEFMAP_H

#include <QSet>
#include <QString>
#include <QMap>
#include <QHash>
#include <QVariant>

#include <pmi_common_core_mini_export.h>

/*!
 * @brief The column field m_names, without this function, shows the internal table field m_names.
 * This will show m_names that users will find more 'friendly'
 *
 * @param fieldName
 * @return QString
 */

#define kDefaultMassSigFig 4
#define kDefaultTimeSigFig 2
#define kScanTimeSigFig 4

class PMI_COMMON_CORE_MINI_EXPORT ColumnFriendlyName
{
public:
    //Note: When adding non-real enum value, we need to update the logic in SortFilterProxyModel::updateColumnTypes(); double check this function.
    //Note: All string with numbers will be sorted as if they are numbers if kUnknown is not used. E.g. '123,34,23' or '123-23' or '23.234;321;123'
    enum Type
    {
         Type_String
        ,Type_Decimal
        ,Type_Scientific
        ,Type_Integer
        ,Type_Percent
    };

    //! @return true if @a type belong to a real type category: decimal, scientific, integer or percent
    inline static bool isTypeReal(ColumnFriendlyName::Type type)
    {
        if (Type_String == type)
            return false;

        return true;
    }

    enum DocumentType
    {
        Unknown,
        Batcher,
        ByonicViewer,
        Byologic,
        Byomap,
        ByomapIntactPeakTableNamesRefresh,
        ByomapPeakTableNamesRefresh,
        Issa,
        Oxi,
        Intact,  //Yong added this ML-464 (2015-01-14), but not used anywhere.
        Byosphere,
        Base
    };

    enum GroupingMethod {
         DoNothing  //leave alone
        ,DashIfDifferent //"--" for "(1,2,3) or (1,2,,)
        ,DashIfDifferentIgnoreEmptyOrNULL //"--" for (1,2,3) but "1" for (1,1,,)
        ,Counter_ShowOriginalFirst  //1 (3)
        ,Sum  //3
        ,Range   //12-15
        ,List    //12; 13; 15
        ,ListKeepDuplicates   //12; 12; 13; 13; 15
    };

    enum ChildrenCollectionMethod {
         UseChildrenImmedidateOnly
        ,UseChildrenAllRecursive
    };

    struct PMI_COMMON_CORE_MINI_EXPORT FriendlyNameStruct
    {
        QString m_name;
        QString m_detail;
        Type    m_type;
        int     m_sigfig;
        GroupingMethod m_groupMethod;
        ChildrenCollectionMethod    m_childCollectionMethod;

        FriendlyNameStruct()
        {
            m_type = Type_String;
            m_sigfig = -1;
            m_groupMethod = DashIfDifferent;
            m_childCollectionMethod = UseChildrenAllRecursive;
        }

        //Note: I've mistakenly added this enum for int sigfig; compiler doesn't catch it.
        //Either stop using default parameter, or come up with better solution.
        FriendlyNameStruct(const QString& name,
                           const QString& detail,
                           const Type type,
                           int sigfig,
                           const GroupingMethod method = DoNothing,
                           const ChildrenCollectionMethod useImmediateChildrenOnly = UseChildrenAllRecursive)
        {
            m_name = name;
            m_detail = detail;
            m_type = type;
            m_sigfig = sigfig;
            m_groupMethod = method;
            m_childCollectionMethod = useImmediateChildrenOnly;
        }
        QString typeToString() const {
            switch(m_type) {
                case Type_String:
                    return "unknown";
                case Type_Decimal:
                    return "decimal";
                case Type_Scientific:
                    return "scientific";
                case Type_Integer:
                    return "integer";
                case Type_Percent:
                    return "percent";
            }
            return "unknown";
        }

        inline QString valueToString(const QVariant &value) const {
            QString result;
            bool ok = true;
            switch(m_type) {
            case Type_Decimal:
                result = QString::number(value.toDouble(&ok), 'f', m_sigfig);
                break;
            case Type_Scientific:
                result = QString::number(value.toDouble(&ok), 'e', m_sigfig);
                break;
            case Type_Percent:
                result = QString::number(value.toDouble(&ok) * 100.0, 'f', m_sigfig)
                    + QLatin1Char('%');
                break;
            default:
                result = value.toString();
            }
            return ok ? result : value.toString();
        }

        //! @return value converted from string based on type of the column
        //! On failed conversion returns invalid variant and sets *ok to false.
        //! On success sets *ok to true.
        inline QVariant stringToValue(const QString &string, bool *ok = nullptr) const;
    };

    //! A signature for setter function that updates the names map for given document type
    //! @see addSetter() initialize()
    typedef void(*Setter)(QMap<QString,FriendlyNameStruct>*);

private:
    static DocumentType m_current_doc_type;
    static QMap<DocumentType, ColumnFriendlyName::Setter> m_setters;
    static QMap<QString,FriendlyNameStruct> m_names;
    static QMap<QString,FriendlyNameStruct> m_rev_names;  //TODO: remove? seems to be
    static bool m_isIntactDocument;

public:

    static void addSetter(DocumentType type, ColumnFriendlyName::Setter setter) {
        m_setters.insert(type, setter);
    }

    static QString fieldName(const QString & fieldName) {
        construct();
        auto iterator = m_names.constFind(fieldName);
        if (iterator != m_names.constEnd()) {
            return iterator.value().m_name;
        }
        return fieldName;
    }

    static int sigFigs(const QString &fieldName)
    {
        auto iterator = m_names.constFind(fieldName);
        if (iterator != m_names.constEnd()) {
            return iterator.value().m_sigfig;
        }
        return 0;
    }

    static bool editionOfSigFigsAvailable(const QString &fieldName)
    {
        auto iterator = m_names.constFind(fieldName);
        if (iterator != m_names.constEnd()) {
            return (iterator.value().m_type == Type_Decimal || iterator.value().m_type == Type_Scientific || iterator.value().m_type == Type_Percent);
        }
        return false;
    }

    static void setSigFigs(const QString &fieldName, int newValue)
    {
        auto iterator = m_names.find(fieldName);
        if (iterator != m_names.end()) {
            iterator.value().m_sigfig = newValue;
        }
    }

    inline static const QString valueToString(const QString & fieldName, const QVariant &value) {
        auto iterator = m_names.constFind(fieldName);
        return iterator == m_names.constEnd() ? value.toString() : iterator.value().valueToString(value);
    }

    static QString toolTip(const QString & fieldName) {
        construct();
        auto iterator = m_names.constFind(fieldName);
        if (iterator != m_names.constEnd()) {
            return iterator.value().m_detail;
        }
        return fieldName;
    }

    static const FriendlyNameStruct& field(const QString & fieldName) {
        construct();

        auto iterator = m_names.constFind(fieldName);
        if (iterator != m_names.constEnd()) {
            return iterator.value();
        }
        iterator = m_rev_names.constFind(fieldName);
        if (iterator != m_rev_names.constEnd()) {
            return iterator.value();
        }

        static FriendlyNameStruct dummy;
        return dummy;
    }

    static const QMap<QString,FriendlyNameStruct> & getColumnFriendlyNames()
    {
        construct();
        return m_names;
    }

    static const QMap<QString,FriendlyNameStruct> & getColumnFriendlyNamesNoConstruct()
    {
        return m_names;
    }

    static void changeDocumentType(DocumentType newDocumentType)
    {
        if (m_current_doc_type != newDocumentType) {
            m_current_doc_type = newDocumentType;
            m_rev_names.clear();
            m_names.clear();
            construct();
        }
    }

    static void setByomapDocumentType(DocumentType newDocumentType)
    {
        m_isIntactDocument = (newDocumentType == ColumnFriendlyName::Intact);

        initialize(m_isIntactDocument ? ByomapIntactPeakTableNamesRefresh : ByomapPeakTableNamesRefresh);
    }

    static void saveToCSV(QString filename);

private:
    static void initialize(DocumentType type)
    {
        ColumnFriendlyName::Setter setter = m_setters[type];
        if (setter) {
            setter(&m_names);
        }
    }

    static void construct()
    {
        if (!m_names.isEmpty()) {
            return;
        }
        initialize(m_current_doc_type);
        initialize(Byosphere);
        initialize(Base);

        /// create reverse mapping for cases when header is known but
        /// key is not known..
        QMap<QString,FriendlyNameStruct>::iterator itr = m_names.begin();
        for(;itr != m_names.end(); ++itr)
          m_rev_names[itr->m_name] = *itr;
    }
};

#endif
