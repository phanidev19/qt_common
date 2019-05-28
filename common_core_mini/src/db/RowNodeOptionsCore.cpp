#include "RowNodeOptionsCore.h"
#include "RowNodeUtils.h"
#include "PmiQtCommonConstants.h"

#include <QtSqlUtils.h>
#include <TableExtract.h>
#include <QStringList>

_PMI_BEGIN

namespace rn {

PMI_COMMON_CORE_MINI_EXPORT QVariant & getOption( RowNode & node, const QString & colName, int child)
{
    static QVariant local_var;
    RowNode * root = node.getRoot();
    if (root->getChildListSize() <= child) {
        debugCoreMini() << "getRowNodeOption is meant to be called with at least enough child from the root. child=" << child;
        local_var = QVariant();
        return local_var;
    }
    RowNode * childNode = root->getChild(child);
    return childNode->getDataAt(colName);
}

PMI_COMMON_CORE_MINI_EXPORT const QVariant & getOption( const RowNode & node, const QString & colName, int child)
{
    static QVariant local_var;
    const RowNode * root = node.getRoot();
    if (root->getChildListSize() <= child) {
        debugCoreMini() << "getRowNodeOption is meant to be called with at least enough child from the root. child=" << child;
        local_var = QVariant();
        return local_var;
    }
    const RowNode * childNode = root->getChild(child);
    return childNode->getDataAt(colName);
}

PMI_COMMON_CORE_MINI_EXPORT QVariant getOptionWithDefault(const RowNode & node, const QString & colName,
                                                     const QVariant& defaultValue, int child)
{
    QVariant val = getOption(node, colName, child);
    if (!val.isValid() || val.isNull()) {
        return defaultValue;
    }
    return val;
}

PMI_COMMON_CORE_MINI_EXPORT bool contains(const RowNode & node, const QString & colName)
{
    const RowNode * root = node.getRoot();
    if (root->getChildListSize() <= 0) {
        return false;
    }
    return root->rootHasData(colName);
}

PMI_COMMON_CORE_MINI_EXPORT bool setOption(RowNode & node, const QString & colName, const QVariant & value, int child)
{
    RowNode * root = node.getRoot();
    if (child < 0) {
        return false;
    }

    while(root->getChildListSize() <= child) {
        root->addNewChildMatchRootDataSize();
        //debugCoreMini() << "adding new children for setOption() size(),child=" << root->getChildListSize()  << "," << child;
    }

    RowNode * childNode = root->getChild(child);
    if (childNode->rootHasData(colName)) {
        childNode->getDataAt(colName) = value;
    } else {
        //debugCoreMini() << "warning, setOption did not find column name of " << colName << ".  Inserting column.";
        childNode->addDataToRootAndResizeChildren(colName, QVariant());
        childNode->getDataAt(colName) = value;
    }
    return true;
}

PMI_COMMON_CORE_MINI_EXPORT bool setOptions(RowNode & node, const VariantItems &values, int child)
{
    RowNode * root = node.getRoot();
    if (root->getChildListSize() <= 0 || child > root->getChildListSize() ) {
        return false;
    }
    RowNode * firstChild = root->getChild(child);
    firstChild->getRowData() = values;
    return true;
}

PMI_COMMON_CORE_MINI_EXPORT RowNode & getOptionsChild(RowNode & node, bool * ok)
{
    static RowNode obj;
    RowNode * root = node.getRoot();
    if (root->getChildListSize() <= 0) {
        if (ok) *ok = false;
        obj = RowNode();
        return obj;
    }
    RowNode * firstChild = root->getChild(0);
    return *firstChild;
}

PMI_COMMON_CORE_MINI_EXPORT void createOptions(const VariantItems &colNames, const VariantItems & xvalues,
                                          RowNode & options_root, int numChildren)
{
    options_root.clear();
    options_root.addData(colNames);
    VariantItems localvector = xvalues;
    while (localvector.size() < colNames.size())
        localvector.push_back(QVariant());
    for (int i = 0; i < numChildren; i++)
        options_root.addChild(new RowNode(localvector));
}

PMI_COMMON_CORE_MINI_EXPORT QStringList getOptionsNames(const RowNode & node)
{
    return node.getRootDataAsStringList();
}

namespace {
//! Loads a single option (key, value) and sets it to @a option
//! @see loadOptions
Err loadOptionInternal(const QString &tableName, const QString &key, const QVariant &value,
                       RowNode *option, const QString &suffixName)
{
    //Convert 'true' 'false' to numbers.  Need to do this because e.g. QString::toInt() returns 0 for 'true'.
    QVariant val(value);
    if (val.toString().toLower() == "true") {
        val = 1;
    } else if (val.toString().toLower() == "false") {
        val = 0;
    }

    const QStringList keysplit = key.split(":", QString::KeepEmptyParts);
    if (keysplit.size() == 2) {
        if (suffixName.isEmpty()) {
            setOption(*option, keysplit[1], val);
        }
    } else if (keysplit.size() == 3) {
        int childindex = 0;
        if (suffixName.isEmpty()) {
            bool ok = false;
            childindex = keysplit[2].toInt(&ok);
            if (!ok) {
                return kNoErr; // this can mean a key with non-empty suffix
            }
        } else if (suffixName != keysplit[2]) {
            return kNoErr; // suffix does not match
        }
        setOption(*option, keysplit[1], val, childindex);
    } else {
        warningCoreMini() << "Warning, could not parse" << tableName << ":" << key;
        return kError;
    }
    return kNoErr;
}

template<typename DBHandler>
Err saveOptionsInternal(DBHandler db, const QString &tableName,
                                       const QString &prefixName, const RowNode &option,
                                       const QString &suffixName)
{
    Err e = kNoErr;
    QStringList colNames = getOptionsNames(option);
    e = db->init(QString::fromLatin1("INSERT INTO %1(Key,Value) VALUES(?,?)").arg(tableName),
                 QString::fromLatin1("UPDATE %1 SET Key=?, Value=? WHERE Id = ?").arg(tableName),
                 QString::fromLatin1("SELECT Id FROM %1 WHERE Key=?").arg(tableName)); eee;

    for (int child = 0; child < option.getChildListSize(); child++) {
        foreach(QString _key, colNames) {
            QString key = prefixName+":"+_key; //e.g. Peaks:MinPeakAreaPercent
            QVariant value = getOption(option, _key, child);

            //If more than one child, append index, e.g. Peaks:MinPeakAreaPercent:1
            if (child >= 1) {
                key = QString("%1:%2").arg(key).arg(child);
            } else if (!suffixName.isEmpty()) {
                key = QString("%1:%2").arg(key).arg(suffixName);
            }

            e = db->updateOrInsert(key, value); eee;
        }
    }

error:
    return e;
}

//! Used by saveOptions (QSql variant)
class DbHandlerForQtSql {
public:
    explicit DbHandlerForQtSql(QSqlDatabase *db) : m_db(db) {
        Q_ASSERT(m_db);
        m_q_update = makeQuery(db, true);
        m_q_insert = makeQuery(db, true);
        m_q_select = makeQuery(db, true);
    }
    ~DbHandlerForQtSql() {}

    Err init(const QString &insertSQL, const QString &updateSQL, const QString &selectSQL) {
        Err e = kNoErr;
        e = QPREPARE(m_q_insert, insertSQL); eee;
        e = QPREPARE(m_q_update, updateSQL); eee;
        e = QPREPARE(m_q_select, selectSQL); eee;
    error:
        return e;
    }

    Err updateOrInsert(const QString &key, const QVariant &value) {
        Err e = kNoErr;
        m_q_select.bindValue(0,key);

        //[tag:double]
        //We have an issue whenr reading JSON file because it reads numbers as double
        //See buildTree(const QVariantMap &root, RowNode *rootNode) for details.

        //Example of what happens:
        //Before with buildTree()
        //JSON 2:number --> QJson 2:double --> Rownode "2":QString --> Sqlite "2":TEXT --> RowNode "2":QString --> toInt() --> 2:int
        //Now with buildTree()
        //JSON 2:number --> QJson 2:double --> Rownode  2 :double --> Sqlite "2.0":TEXT --> RowNode "2.0":QString --> toInt() --> 0:int
        //The step where 2:double is converted into sqlite "2.0":TEXT is happening because of this function by directly taking
        //double and saving it to the database.
        //We will call toString() so that 2.0:double will be converted to "2":QString.

        //Also, FYI, the value in the comment is the actual output and not what was assumed by Yong
        //qWarning() << QVariant("2.0").toInt(); // 0  ***** Not what Yong was expecting
        //qWarning() << QVariant("2").toInt(); // 2  OK
        //qWarning() << QVariant("0.0").toBool(); // true **** Not what Yong was expecting
        //qWarning() << QVariant(0.0).toBool(); // false  OK
        //qWarning() << QVariant(1).toDouble(); // 1.0 OK

        e = QEXEC_NOARG(m_q_select); eee;
        if (m_q_select.next()) {
            qlonglong id = m_q_select.value(0).toLongLong();
            m_q_update.bindValue(0,key);
            m_q_update.bindValue(1,value.toString());  //See [tag:double]
            m_q_update.bindValue(2,id);
            e = QEXEC_NOARG(m_q_update); eee;
        } else {
            m_q_insert.bindValue(0,key);
            m_q_insert.bindValue(1,value.toString());  //See [tag:double]
            e = QEXEC_NOARG(m_q_insert); eee;
        }
    error:
        return e;
    }

private:
    QSqlQuery m_q_update;
    QSqlQuery m_q_insert;
    QSqlQuery m_q_select;
    QSqlDatabase *m_db;
};

//! Used by saveOptions (sqlite3 variant)
class DbHandlerForSqlite3 {
public:
    explicit DbHandlerForSqlite3(sqlite3 *db) : m_db(db) {
        Q_ASSERT(m_db);
    }
    ~DbHandlerForSqlite3() {
        my_sqlite3_finalize(m_stmt_insert, m_db);
        my_sqlite3_finalize(m_stmt_update, m_db);
        my_sqlite3_finalize(m_stmt_select, m_db);
    }

    Err init(const QString &insertSQL, const QString &updateSQL, const QString &selectSQL) {
        Err e = kNoErr;
        e = my_sqlite3_prepare(m_db, insertSQL, &m_stmt_insert); eee;
        e = my_sqlite3_prepare(m_db, updateSQL, &m_stmt_update); eee;
        e = my_sqlite3_prepare(m_db, selectSQL, &m_stmt_select); eee;
    error:
        return e;
    }

    Err updateOrInsert(const QString &key, const QVariant &value) {
        Err e = my_sqlite3_bind_text(m_db, m_stmt_select, 1, key); eee;
        if (sqlite3_step(m_stmt_select) == SQLITE_ROW) {
            int id = sqlite3_column_int(m_stmt_select, 0);
            e = my_sqlite3_bind_text(m_db, m_stmt_update, 1, key); eee;
            e = my_sqlite3_bind_text(m_db, m_stmt_update, 2, value.toString()); eee;
            e = my_sqlite3_bind_int(m_db, m_stmt_update, 3, id); eee;
            e = my_sqlite3_step(m_db, m_stmt_update, MySqliteStepOption_Reset); eee;
        } else {
            e = my_sqlite3_bind_text(m_db, m_stmt_insert, 1, key); eee;
            e = my_sqlite3_bind_text(m_db, m_stmt_insert, 2, value.toString()); eee;

            e = my_sqlite3_step(m_db, m_stmt_insert, MySqliteStepOption_Reset); eee;
            sqlite3_reset(m_stmt_insert);
        }
        sqlite3_reset(m_stmt_select);
    error:
        return e;
    }

private:
    sqlite3_stmt *m_stmt_insert = nullptr;
    sqlite3_stmt *m_stmt_update = nullptr;
    sqlite3_stmt *m_stmt_select = nullptr;
    sqlite3 *m_db;
};
}

PMI_COMMON_CORE_MINI_EXPORT Err loadOptions(QSqlDatabase &db, const QString &tableName,
                                       const QString &prefixName, RowNode & option,
                                       const QString &suffixName)
{
    Err e = kNoErr;
    QSqlQuery q_select = makeQuery(db, true);
    QString cmd = QString("SELECT Key,Value FROM %1 WHERE Key LIKE ?").arg(tableName);

    e = QPREPARE(q_select, cmd); eee;
    q_select.bindValue(0, QString("%1:%").arg(prefixName)); // LIKE 'Peaks:%'
    e = QEXEC_NOARG(q_select); eee;
    option.clearChildren();

    while(q_select.next()) {
        (void)loadOptionInternal(tableName,
                                 q_select.value(0).toString(), q_select.value(1), //E.g. Peaks:bla
                                 &option, suffixName);
    }

error:
    return e;
}


PMI_COMMON_CORE_MINI_EXPORT Err saveOptions(QSqlDatabase &db, const QString &tableName,
                                            const QString &prefixName, const RowNode &option,
                                            const QString &suffixName)
{
    DbHandlerForQtSql handler(&db);
    Err e = saveOptionsInternal(&handler, tableName, prefixName, option, suffixName); eee;
error:
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err createDocumentRenderingOptionsTable(QSqlDatabase & db)
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(db, true);
    e = QEXEC_CMD(q, QString("CREATE TABLE IF NOT EXISTS %1(Id INTEGER PRIMARY KEY, Key TEXT, Value TEXT)").arg(kDocumentRenderingOptions));
    if (e != kNoErr) {
        warningCoreMini() << "Warning, could not create document rendering options table";
    }

    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err createDocumentRenderingOptionsTable(sqlite3 *db)
{
    QString cmd = QString("CREATE TABLE IF NOT EXISTS %1(Id INTEGER PRIMARY KEY, Key TEXT, Value TEXT)").arg(kDocumentRenderingOptions);
    Err e = sql_exec(db, cmd); eee;
error:
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err createFilterOptionsTable(QSqlDatabase & db, QString tableName)
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(db, true);
    e = QEXEC_CMD(q, QString("CREATE TABLE IF NOT EXISTS %1(Id INTEGER PRIMARY KEY, Key TEXT, Value TEXT)").arg(tableName)); eee;
error:
    return e;
}

Err createFilterOptionsTable(sqlite3 * db, QString tableName) {
    Err e = kNoErr;
    QString cmd = QString("CREATE TABLE IF NOT EXISTS %1(Id INTEGER PRIMARY KEY, Key TEXT, Value TEXT)").arg(tableName);
    e = sql_exec(db, cmd); eee;
error:
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err loadOptions(sqlite3* db, QString tableName, QString prefixName,
                                            RowNode & option, const QString &suffixName)
{
    Err e = kNoErr;
    TableExtract tx;
    QString cmd = QString("SELECT Key,Value FROM %1 WHERE Key LIKE '%2'").arg(tableName).arg(prefixName+":%");

    e = tx.sql_extract(db, cmd); eee;

    //Note: keep original content as they are and overwrite from database.
    //This allows us to introduce new variables.  I don't actually recall why I wanted to
    //clear out the content before reading from db.
    //option.clearChildren();

    for (unsigned int i = 0; i < tx.getSize(); i++) {
        (void)loadOptionInternal(tableName,
                                 tx.getValueString(i, kKey),
                                 tx.getValueString(i, kValue), //E.g. Peaks:bla
                                 &option, suffixName);
    }

error:
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err saveOptions(sqlite3* db, QString tableName, QString prefixName,
                                            const RowNode & option, const QString &suffixName)
{
    DbHandlerForSqlite3 handler(db);
    Err e = saveOptionsInternal(&handler, tableName, prefixName, option, suffixName); eee;
error:
    return e;
}

Err loadOptions_as_JSON(sqlite3* db, QString tableName, QString prefixName, RowNode & option)
{
    Err e = kNoErr;
    TableExtract tx;
    QString cmd = QString("SELECT Key,Value FROM %1 WHERE Key = '%2'").arg(tableName).arg(prefixName);

    e = tx.sql_extract(db, cmd); eee;
    if (tx.getSize() != 1) {
        debugCoreMini() << "Warning, prefixName found count is:" << tx.getSize() << " Should be 1";
    }

    for (unsigned int i = 0; i < tx.getSize(); i++) {
        //QString _key = tx.getValueString(i, kKey).c_str();  //E.g. Peaks
        const QByteArray valBA = tx.getValueString(i, kValue).toUtf8();

        if (pmi::rn::parse(valBA, &option)) {
            break;
        } else {
            e = kBadParameterError; eee;
        }
    }

error:
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err saveOptions_as_JSON(sqlite3* db, QString tableName, QString prefixName, const RowNode & option)
{
    Err e = kNoErr;
    bool ok = false;
    sqlite3_stmt *stmt_insert = NULL;
    sqlite3_stmt *stmt_update = NULL;
    sqlite3_stmt *stmt_select = NULL;

    e = my_sqlite3_prepare(db, QString("INSERT INTO %1(Key,Value) VALUES(?,?)").arg(tableName), &stmt_insert); eee;
    e = my_sqlite3_prepare(db, QString("UPDATE %1 SET Key=?, Value=? WHERE Id = ?").arg(tableName), &stmt_update); eee;
    e = my_sqlite3_prepare(db, QString("SELECT Id FROM %1 WHERE Key=?").arg(tableName), &stmt_select); eee;

    if (1) {
        QString key = prefixName; //e.g. Peaks
        QByteArray value = serialize(&option, &ok);
        QString valueStr = QString(value); //Note: QJSON already encodes 'weird' characters, etc... sample - Copy - \u9ad8\u5174 - gl\u00fccklich
        if (!ok) {
            e = kBadParameterError; eee;
        }

        e = my_sqlite3_bind_text(db, stmt_select, 1, key); eee;

        if (sqlite3_step(stmt_select) == SQLITE_ROW ) {
            int id = sqlite3_column_int(stmt_select, 0);
            e = my_sqlite3_bind_text(db, stmt_update, 1, key); eee;
            e = my_sqlite3_bind_text(db, stmt_update, 2, valueStr); eee;
            e = my_sqlite3_bind_int(db, stmt_update, 3, id); eee;
            e = my_sqlite3_step(db, stmt_update, MySqliteStepOption_Reset); eee;
        } else {
            e = my_sqlite3_bind_text(db, stmt_insert, 1, key); eee;
            e = my_sqlite3_bind_text(db, stmt_insert, 2, valueStr); eee;

            e = my_sqlite3_step(db, stmt_insert, MySqliteStepOption_Reset); eee;
        }
        sqlite3_reset(stmt_select);
    }

error:
    my_sqlite3_finalize(stmt_insert,db);
    my_sqlite3_finalize(stmt_update,db);
    my_sqlite3_finalize(stmt_select,db);
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err createOptionsFromRootNodeOrChildNode_OneLevelOnly(const RowNode * _node, RowNode & options)
{
    Err e = kNoErr;
    const RowNode * root = _node->getRoot();
    if (root->getChildListSize() <= 0) {
        e = kBadParameterError; ree;
    }
    const RowNode * node = _node->isRoot() ? root->getChild(0) : _node;
    createOptions(root->getRowData(), node->getRowData(), options, 1);
//error:
    return e;
}

void copyOptions_KeepDestinationNamesAndSourceNames_LevelOneOnly(const RowNode * src, RowNode * dest)
{
    QStringList destNames = dest->getRootDataAsStringList();
    QStringList srcNames = src->getRootDataAsStringList();

    int child = 0;  //only consider level one.
    for (const QString & name : destNames) {
        if (srcNames.contains(name)) {
            setOption(*dest, name, getOption(*src, name, child), child);
        }
    }

    for (const QString & name: srcNames) {
        if (!destNames.contains(name)) {
            //names not in dest will be added automatically
            setOption(*dest, name, getOption(*src, name, child), child);
        }
    }
}

//! Loads the value for the given key from DocumentRenderingOptions table
PMI_COMMON_CORE_MINI_EXPORT Err loadDocumentRenderingOption(QSqlDatabase &db, const QString& key, QString &value)
{
    QSqlQuery q_select = makeQuery(db, true);

    Err e = QPREPARE(q_select, QString("SELECT Value FROM %1 WHERE Key=?").arg(kDocumentRenderingOptions)); eee;
    q_select.bindValue(0, key);
    e = QEXEC_NOARG(q_select); eee;

    if (q_select.next()) {
        value = q_select.value(0).toString();
    }

error:
    return e;
}

Err saveDocumentRenderingOption(QSqlDatabase &db, const QString &key, const QString &value)
{
    QSqlQuery q_update = makeQuery(db, true);
    QSqlQuery q_insert = makeQuery(db, true);

    Err e = QPREPARE(q_update, QString("UPDATE %1 SET Value=? WHERE KEY=?").arg(kDocumentRenderingOptions)); eee;
    q_update.bindValue(0, value);
    q_update.bindValue(1, key);
    e = QEXEC_NOARG(q_update); eee;

    if (q_update.numRowsAffected() == 0) {
        e = QPREPARE(q_insert, QString("INSERT INTO %1(Key, Value) VALUES(?,?)").arg(kDocumentRenderingOptions)); eee;
        q_insert.bindValue(0, key);
        q_insert.bindValue(1, value);
        e = QEXEC_NOARG(q_insert); eee;
    }

error:
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err loadDocumentRenderingOption(sqlite3 * db, const QString &key, QString &value)
{
    Err e = kNoErr;
    TableExtract tx;
    QString cmd = QString("SELECT Value FROM %1 WHERE Key = '%2'").arg(kDocumentRenderingOptions).arg(key);

    e = tx.sql_extract(db, cmd); eee;
    if (tx.getSize() != 1) {
        debugCoreMini() << "Warning, prefixName found count is:" << tx.getSize() << " Should be 1";
    }
    else {
        value = QString(tx.getValueString(0, kValue));
    }

error:
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err saveDocumentRenderingOption(sqlite3 * db, const QString &key, const QString &value)
{
    Err e = kNoErr;

    sqlite3_stmt *stmt_insert = nullptr;
    sqlite3_stmt *stmt_update = nullptr;
    sqlite3_stmt *stmt_select = nullptr;

    e = my_sqlite3_prepare(db, QString("INSERT INTO %1(Key,Value) VALUES(?,?)").arg(kDocumentRenderingOptions), &stmt_insert); eee;
    e = my_sqlite3_prepare(db, QString("UPDATE %1 SET Key=?, Value=? WHERE Id = ?").arg(kDocumentRenderingOptions), &stmt_update); eee;
    e = my_sqlite3_prepare(db, QString("SELECT Id FROM %1 WHERE Key=?").arg(kDocumentRenderingOptions), &stmt_select); eee;

    e = my_sqlite3_bind_text(db, stmt_select, 1, key); eee;

    if (sqlite3_step(stmt_select) == SQLITE_ROW ) {
        int id = sqlite3_column_int(stmt_select, 0);
        e = my_sqlite3_bind_text(db, stmt_update, 1, key); eee;
        e = my_sqlite3_bind_text(db, stmt_update, 2, value); eee;
        e = my_sqlite3_bind_int(db, stmt_update, 3, id); eee;
        e = my_sqlite3_step(db, stmt_update, MySqliteStepOption_Reset); eee;
    } else {
        e = my_sqlite3_bind_text(db, stmt_insert, 1, key); eee;
        e = my_sqlite3_bind_text(db, stmt_insert, 2, value); eee;
        e = my_sqlite3_step(db, stmt_insert, MySqliteStepOption_Reset); eee;
    }
    sqlite3_reset(stmt_select);

error:
    my_sqlite3_finalize(stmt_insert,db);
    my_sqlite3_finalize(stmt_update,db);
    my_sqlite3_finalize(stmt_select,db);

    return e;
}


} //rn

_PMI_END
