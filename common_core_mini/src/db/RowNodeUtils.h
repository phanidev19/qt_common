/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef ROWNODEUTIL_H
#define ROWNODEUTIL_H

#ifdef _WIN32
// Windows.h define min/max so this macro supresses so that numeric_limits can expose min/max
#undef min
#undef max
#endif

#include "RowNode.h"
#include "EntryDefMap.h"
#include "RowNodeOptionsCore.h"
#include <QVariantMap>
#include <TableExtract.h>

/*!
 * @brief Given a specificially formatted input, this will construct a RowNode.
 *
 * @param tableRowEntries contains row values.
 * @param columnNames contains fieldNames.
 * @param node pointer to an RowNode instance
 */
PMI_COMMON_CORE_MINI_EXPORT void InitRootRowNode(const pmi::TableExtract & extract_table, RowNode * xnode);

/*!
 * @brief Construct root and children RowNodes, but do not add to root's children.  Useful when placinging children elsewhere.
 * Note that nodeList are also all root (m_parentPtr == NULL).
 *
 * @param rootNode will get column names assigned, if not NULL
 * @param nodeList will get a list of newly constructed row contents (parentPtr is NULL).
 */
PMI_COMMON_CORE_MINI_EXPORT void ConstructRowNodeList(const pmi::TableExtract & extract_table, RowNode* rootNode, std::vector<RowNode*> & nodeList);

//RowNode utils
namespace pmi{
//! write several RowNode to a single JSON file. Useful if the RowNodes contain different root column information (e.g. two different TreeTable content)
PMI_COMMON_CORE_MINI_EXPORT Err writeToJSON_RowNodeList(const QList<const RowNode*> & rowList, const QString & filename);

//! read JSON file into several RowNow list. Useful if the RowNodes contain different root column information (e.g. two different TreeTable content)
PMI_COMMON_CORE_MINI_EXPORT Err readFromJSON_RowNodeList(const QString & filename, QList<RowNode> & rowList);

PMI_COMMON_CORE_MINI_EXPORT Err writeToJSON_RowNodeSet(const QMap<QString, RowNode> & rowSet, const QString & filename);

PMI_COMMON_CORE_MINI_EXPORT Err readFromJSON_RowNodeSet(const QString & filename, QMap<QString, RowNode> * rowSet);

PMI_COMMON_CORE_MINI_EXPORT QByteArray convertToJSON_RowNodeSet(const QMap<QString, RowNode> & rowSet);
PMI_COMMON_CORE_MINI_EXPORT Err convertFromJSON_RowNodeSet(const QByteArray & jsonContent, QMap<QString, RowNode> * rowSet);

PMI_COMMON_CORE_MINI_EXPORT QString rangeToString(const QString &minString, const QString &maxString);

PMI_COMMON_CORE_MINI_EXPORT QString rangeToString(const VariantItems &list, const ColumnFriendlyName::FriendlyNameStruct & obj);

PMI_COMMON_CORE_MINI_EXPORT QString rangeToString(const QVector<QVariant> &list, const ColumnFriendlyName::FriendlyNameStruct & obj);

/*!
 * Sets variables pointed by @a vmin and @a vmax to minimum and maximum values obatined from @a container.
 * Conversion from container element is performed by @a convert.
 * @return @c true if container is not empty. @a vmin and @a vmax are only changed when the container
 * is not empty.
 */
template<typename T, class C, class CItem>
bool getRange(const C &container, T *vmin, T *vmax, T (CItem::*convert)(bool*) const) {
    Q_ASSERT(vmin);
    Q_ASSERT(vmax);
    if (container.isEmpty()) {
        return false;
    }

    auto isEmpty = [](const CItem &v) -> bool {
        return v.toString().isEmpty();
    };

    // return false if all values in the container are empty
    if (std::find_if_not(container.begin(), container.end(), isEmpty) == container.end()) {
        return false;
    }

    *vmin = std::numeric_limits<T>::max();
    *vmax = std::numeric_limits<T>::lowest();
    for (const CItem &v : container) {
        // ignore empty values
        if (isEmpty(v)) {
            continue;
        }
        const T value = (v.*convert)(nullptr);
        if (*vmin > value) {
            *vmin = value;
        }
        if (*vmax < value) {
            *vmax = value;
        }
    }

    return true;
}


namespace rn {

PMI_COMMON_CORE_MINI_EXPORT Err readFromJSON(RowNode * ths, const QString & filename);
PMI_COMMON_CORE_MINI_EXPORT Err writeToJSON(RowNode * ths, const QString & filename);

PMI_COMMON_CORE_MINI_EXPORT Err writeByteArray(const QString & filename, QByteArray & jsonContent);

////////////////////////////
//JSON utils
////////////////////////////
PMI_COMMON_CORE_MINI_EXPORT bool parse(const QByteArray json, RowNode* root);
PMI_COMMON_CORE_MINI_EXPORT QByteArray serialize(const RowNode* root, bool* ok = 0);


//number of items within a group (count), range within a group, avg & std dev., list, -- if different, sum of values.

////////////////////////////
// Group row node re-assignment
////////////////////////////
class PMI_COMMON_CORE_MINI_EXPORT GroupSummary {
public:
    GroupSummary()
    {
        m_avoidAddingDuplicatedBrackets = false;
    }

    ~GroupSummary() {
    }

    void set(const QString & key, const ColumnFriendlyName::FriendlyNameStruct & obj)
    {
        m_keyType[key] = obj;
    }

    int remove(const QString & key)
    {
        return m_keyType.remove(key);
    }

    void update(RowNode * node) const;

    void setAvoidAddingDuplicatedBracketsEnabled(bool set)
    {
        m_avoidAddingDuplicatedBrackets = set;
    }

    QVariant makeValue(const VariantItems &list, const QVariant &orginal,
                       const ColumnFriendlyName::FriendlyNameStruct &obj) const;

private:
    void updateNode(RowNode *group_node, int indexUseImmediateChildren) const;

    QMap<QString,ColumnFriendlyName::FriendlyNameStruct> m_keyType;
    bool m_avoidAddingDuplicatedBrackets;
};


} //rn
} //pmi

#endif // ROWNODEUTIL_H
