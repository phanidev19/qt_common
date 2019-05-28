/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef _ROWNODE_OPTIONS_CORE_H_
#define _ROWNODE_OPTIONS_CORE_H_
/*!
 * This set of functions and class abstracts the RowNode structure to handle options,
 * which contain a set of variables and values.
 */

#include "RowNode.h"
struct sqlite3;

_PMI_BEGIN

namespace rn {

////////////////////////////
// Convenience functions to wrap RowNode for containing options:
// A root and one or more children.  Does not work if RowNode has
// nested children (e.g. child within child).
////////////////////////////

//! get options
PMI_COMMON_CORE_MINI_EXPORT QVariant & getOption(RowNode & node, const QString & colName, int child=0);

PMI_COMMON_CORE_MINI_EXPORT const QVariant & getOption(const RowNode & node, const QString & colName, int child=0);

PMI_COMMON_CORE_MINI_EXPORT QVariant getOptionWithDefault(const RowNode & node, const QString & colName, const QVariant& defaultValue, int child = 0);


PMI_COMMON_CORE_MINI_EXPORT bool contains(const RowNode & node, const QString & colName);

//! set one option
PMI_COMMON_CORE_MINI_EXPORT bool setOption(RowNode & node, const QString & colName, const QVariant & value, int child = 0);

//! set all options. be careful using this function. make sure the values.size() matches the root's column size.
PMI_COMMON_CORE_MINI_EXPORT bool setOptions(RowNode & node, const VariantItems &values, int child);

PMI_COMMON_CORE_MINI_EXPORT void createOptions(const VariantItems &colNames, const VariantItems &values, RowNode & options_root, int numChildren = 1);

/*!
 * \brief This will take either a root or a child node.  Then create option based on that.  This will only construct one-level options
 * \param node is either a root or a child node.
 * \param options output
 * \return Err error if improper input
 */
PMI_COMMON_CORE_MINI_EXPORT Err createOptionsFromRootNodeOrChildNode_OneLevelOnly(const RowNode * node, RowNode & options);

//! get the rownode with actual content (and the root node); this function exists to handle old-style/direct interfacing options RowNode structures
PMI_COMMON_CORE_MINI_EXPORT RowNode & getOptionsChild(RowNode & root, bool * ok = NULL);

PMI_COMMON_CORE_MINI_EXPORT QStringList getOptionsNames(const RowNode & node);

//! Loads all options from @a db and @a tableName table into @a option
//!
//! Options are matches based on keys:
//! - <prefixName>:* if there is one child
//! - <prefixName>:*:<integer> if there is more than one child
//! - <prefixName>:*:<suffixName> if @a suffixName is not empty and there is one child
//!
//! @note @a tableName, @a prefixName and @a suffixName should not have whitespaces or ':';
//!       these assumptions made it easier to implement.
//! @note @a suffixName can be empty but should not contain numbers
PMI_COMMON_CORE_MINI_EXPORT Err loadOptions(QSqlDatabase &db, const QString &tableName,
                                       const QString &prefixName, RowNode & option,
                                       const QString &suffixName = QString());

//! @overload
PMI_COMMON_CORE_MINI_EXPORT Err loadOptions(sqlite3* db, QString tableName, QString prefixName, RowNode & option,
                                            const QString &suffixName = QString());

//! Saves all options from @a db and @a tableName table that have keys matching
//! See loadOptions(QSqlDatabase &db, const QString &tableName, const QString &prefixName, RowNode & option,
//! const QString &suffixName) for information about format of keys.
PMI_COMMON_CORE_MINI_EXPORT Err saveOptions(QSqlDatabase &db, const QString &tableName,
                                       const QString &prefixName, const RowNode &option,
                                       const QString &suffixName = QString());

//! @overload
PMI_COMMON_CORE_MINI_EXPORT Err saveOptions(sqlite3* db, QString tableName, QString prefixName, const RowNode & option,
                                            const QString &suffixName = QString());

PMI_COMMON_CORE_MINI_EXPORT Err createFilterOptionsTable(QSqlDatabase & db, QString tableName = "FilterOptions");

PMI_COMMON_CORE_MINI_EXPORT Err createFilterOptionsTable(sqlite3 * db, QString tableName);

PMI_COMMON_CORE_MINI_EXPORT Err createDocumentRenderingOptionsTable(QSqlDatabase & db);
PMI_COMMON_CORE_MINI_EXPORT Err loadDocumentRenderingOption(QSqlDatabase & db, const QString &key, QString &value);
PMI_COMMON_CORE_MINI_EXPORT Err saveDocumentRenderingOption(QSqlDatabase & db, const QString &key, const QString &value);

PMI_COMMON_CORE_MINI_EXPORT Err createDocumentRenderingOptionsTable(sqlite3 * db);
PMI_COMMON_CORE_MINI_EXPORT Err loadDocumentRenderingOption(sqlite3 * db, const QString &key, QString &value);
PMI_COMMON_CORE_MINI_EXPORT Err saveDocumentRenderingOption(sqlite3 * db, const QString &key, const QString &value);

PMI_COMMON_CORE_MINI_EXPORT Err loadOptions_as_JSON(sqlite3* db, QString tableName, QString prefixName, RowNode & option);
PMI_COMMON_CORE_MINI_EXPORT Err saveOptions_as_JSON(sqlite3* db, QString tableName, QString prefixName, const RowNode & option);

/*!
 * \brief copyOptions_KeepDestinationNamesAndSourceNames_OneLevel copies content from source to destination,
 *  and preserved destination and adds source names and content.  Note that this only copies over first child (level 1).
 *  The intention is to make sure newly created options are preserved with default values while source will overwrite them.
 *  We also copy over source names just in case the destination is not already initialized.
 * \param src Options Rownode
 * \param dest Optinos Rownode
 */
PMI_COMMON_CORE_MINI_EXPORT void copyOptions_KeepDestinationNamesAndSourceNames_LevelOneOnly(const RowNode * src, RowNode * dest);

} //rn

_PMI_END

#endif
