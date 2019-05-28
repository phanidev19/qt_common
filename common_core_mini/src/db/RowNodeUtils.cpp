/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#include "RowNodeUtils.h"
#include "PmiQtCommonConstants.h"

#include <QChar>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>


void
InitRootRowNode(const pmi::TableExtract & extract_table, RowNode * xnode)
{
    size_t i,j;
    xnode->clear();

    VariantItems vector;
    {
        const vecqstr &t = extract_table.getColumnNames();
        vector.reserve(static_cast<int>(t.size()));
        for (i = 0; i < t.size(); i++) {
            vector << QVariant(t[i]);
        }
    }
    xnode->setup(vector, NULL);

    for (i = 0; i < extract_table.getSize(); i++) {
        const vecqstr &t = extract_table.getRow(i);
        VariantItems vector;
        vector.reserve(static_cast<int>(t.size()));
        for (j = 0; j < t.size(); j++) {
            vector << QVariant(t[j]);
        }
        RowNode * rnode = new RowNode();
        rnode->setup(vector, xnode);
        xnode->addChild(rnode);
    }
}

void
ConstructRowNodeList(const pmi::TableExtract & extract_table, RowNode* rootNode, std::vector<RowNode*> & nodeList)
{
    size_t i,j;

    if (rootNode) {
        rootNode->clear();
        const vecqstr &t = extract_table.getColumnNames();
        VariantItems vector;
        vector.reserve(static_cast<int>(t.size()));
        for (i = 0; i < t.size(); i++) {
            vector << QVariant(t[i]);
        }
        rootNode->setup(vector, NULL);
    }

    for (i = 0; i < extract_table.getSize(); i++) {
        const vecqstr &t = extract_table.getRow(i);
        VariantItems vector;
        vector.reserve(static_cast<int>(t.size()));
        for (j = 0; j < t.size(); j++) {
            vector << QVariant(t[j]);
        }
        RowNode * rnode = new RowNode();
        rnode->setup(vector, NULL);
        nodeList.push_back(rnode);
    }
}


namespace pmi {

static const QString rangeMask(QString::fromLatin1("%1 - %2"));

PMI_COMMON_CORE_MINI_EXPORT QString rangeToString(const QString &minString, const QString &maxString)
{
    return rangeMask.arg(minString).arg(maxString);
}

PMI_COMMON_CORE_MINI_EXPORT QString rangeToString(const VariantItems & list, const ColumnFriendlyName::FriendlyNameStruct & obj)
{
    QString str;
    double minv;
    double maxv;
    if (getRange(list, &minv, &maxv, &QVariant::toDouble)) {
        str = rangeToString(obj.valueToString(minv), obj.valueToString(maxv));
    }
    return str;
}

PMI_COMMON_CORE_MINI_EXPORT QString rangeToString(const QVector<QVariant> &list, const ColumnFriendlyName::FriendlyNameStruct & obj)
{
    QString str;
    double minv;
    double maxv;
    if (getRange(list, &minv, &maxv, &QVariant::toDouble)) {
        str = rangeToString(obj.valueToString(minv), obj.valueToString(maxv));
    }
    return str;
}

namespace rn {

static void  buildTree(const QVariantMap & root, RowNode* rootNode );
static QVariantMap toMap(const RowNode* root);
RowNode* buildTree(const QVariantMap root);

bool parse(const QByteArray json, RowNode* root)
{
    QJsonParseError err;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(json, &err);

    if (err.error != QJsonParseError::NoError && !jsonDoc.isObject()) {
        return false;
    }

    QJsonObject jsonObject = jsonDoc.object();
    QVariantMap result = jsonObject.toVariantMap();

    root->clear();
    buildTree(result, root);

    return true;
}
//
//RowNode* parse(const QByteArray json, bool* ok)
//{
//    QJson::Parser parser;
//    QVariantMap result = parser.parse (json, ok).toMap();
//    return buildTree(result);
//}

//RowNode* buildTree(const QVariantMap root){
//
//    RowNode* rootNode = new RowNode();
//     buildTree(root, rootNode);
//     return rootNode;
//}

static void buildTree(const QVariantMap &root, RowNode *rootNode)
{
    // Need to find a solution to make a hard copy.  Problem arises because
    // QVariant is too smart; smart pointer, probably.
    for (const QVariant &v : root["data"].toList()) {
        const QString tmpStr = v.toString().toLower();
        if (tmpStr == QStringLiteral("true")) {
            rootNode->addValue(QStringLiteral("1"));
        } else if (tmpStr == QStringLiteral("false")) {
            rootNode->addValue(QStringLiteral("0"));
        } else {
            rootNode->addValue(v);
        }
    }

    for (const QVariant &child : root["children"].toList()) {
        const auto childNode = new RowNode();
        buildTree(child.toMap(), childNode);
        rootNode->addChild(childNode);
    }
}

QByteArray serialize(const RowNode* root, bool* ok)
{
    QVariantMap map = toMap(root);
    QJsonDocument jsonDoc = QJsonDocument::fromVariant(map);
    QByteArray json;

    if (jsonDoc.isNull()) {
        if (ok) {
            *ok = false;
        }
        return json;
    }

    json = jsonDoc.toJson(QJsonDocument::Indented);

    if (ok) {
        *ok = true;
    }

    return json;
}

static QVariantMap toMap(const RowNode* root){
    QVariantMap curr;
    QList<QVariant> data;
    QList<QVariant> children;

    VariantItems dataList = root->getRowData();

    foreach (const QVariant & d, dataList) {
        data<<d;
    }

    for (int i = 0; i < root->getChildListSize(); i++) {
        RowNode * child = root->getChild(i);
        QVariantMap c = toMap(child);
        children << c;
    }

    curr.insert("data", data);
    curr.insert("children", children);

    return curr;
}

static RecursionMessage addGroupNodes(RowNode *node, void * /*data*/, std::vector<RowNode *> *list)
{
    if (node->isGroupNode() && list) {
        list->push_back(node);
    }
    return RecursionContinue;
}

void GroupSummary::update(RowNode *root) const
{
    if (root == nullptr) {
        debugCoreMini() << "GroupSummary::update: root is nullptr";
        return;
    }
    // TODO 26.04.2019 Why so specific value here ?
    const int indexUseImmediateChildren = root->getRootDataAsStringList().indexOf(kXICQuantLevel);

    std::vector<RowNode*> list;
    AddChildrenRecursive(root, &list, addGroupNodes, nullptr);
    // update in reverse order so that children's group are updated, then their parents.
    std::for_each(list.rbegin(), list.rend(), [this, indexUseImmediateChildren](RowNode *node) {
        updateNode(node, indexUseImmediateChildren);
    });
}

static bool isSameValue(const VariantItems &list, bool ignoreEmptyOrNull, QVariant &valRef)
{
    if (list.isEmpty()) {
        return false;
    }
    bool refFound = false;
    for (const QVariant &value: list) {
        if (ignoreEmptyOrNull && (value.isNull() || value.toString().isEmpty())) {
            continue;
        }

        if (refFound) {
            //Note: if (val_ref.toString() != list[i].toString()) {  <--- I forget why we didn't want to do this.
            //Note#2: If one is null and the other is zero (like ValidateType column), we want them to be of equal value.
            //        However, we do not want to enforce that at this level as there will be other content that will want to make
            //        distinction between "", null, 0, and 0.0.
            if (valRef != value) {
                return false;
            }
        } else {
            valRef = value;
            refFound = true;
        }
    }
    return true;
}

static QString makeStringOfVariant(const QVariant &val,
                                   const ColumnFriendlyName::FriendlyNameStruct &obj)
{
    const QChar semicolon(';');
    const QChar comma(',');

    const int prec = obj.m_sigfig < 0 ? 0 : obj.m_sigfig;

    QString result;
    const QString valStr = val.toString();
    const bool isList = valStr.contains(semicolon) || valStr.contains(comma);
    if (isList || obj.m_type == ColumnFriendlyName::Type_String) {
        result += valStr;
    } else {
        if (obj.m_type == ColumnFriendlyName::Type_Scientific) {
            result += QString::number(val.toDouble(), 'e', prec);
        } else if (obj.m_type == ColumnFriendlyName::Type_Percent) {
            const QString percentageTemplate = QStringLiteral("%1%");
            result += percentageTemplate.arg(100 * val.toDouble(), 0, 'f', prec);
        } else {
            result += QString::number(val.toDouble(), 'f', prec);
        }
    }
    return result;
}

static QString makeConcatList(const VariantItems &list,
                              const ColumnFriendlyName::FriendlyNameStruct &obj, bool uniqueOnly)
{
    QString result;
    QHash<QString,int> usedVal;
    
    static const QString separator = QStringLiteral("; ");

    for (const QVariant &val : list) {
        //ignore empty values
        if (val.toString().isEmpty()) {
            continue;
        }
        const QString valStr = makeStringOfVariant(val, obj);
        if (uniqueOnly) {
            if (usedVal.contains(valStr)) {
                continue;
            }
            usedVal[valStr] = 1;
        }

        if (!result.isEmpty()) {
            result += separator;
        }
        result += valStr;
    }

    return result;
}

static QString makeSum(const VariantItems &list, const ColumnFriendlyName::FriendlyNameStruct &obj)
{
    const double sum = std::accumulate(list.begin(), list.end(), 0.0,
                          [] (double sum, const QVariant &value) { return sum + value.toDouble(); });

    int prec = obj.m_sigfig;
    if (prec < 0) {
        prec = 0;
    }

    if (obj.m_type == ColumnFriendlyName::Type_Scientific) {
        return QStringLiteral("%1").arg(sum, 0, 'e', prec);
    } else if (obj.m_type == ColumnFriendlyName::Type_Percent) {
        return QStringLiteral("%1%").arg(100 * sum, 0, 'f', prec);
    } else {
        return QStringLiteral("%1").arg(sum, 0, 'f', prec);
    }

    Q_UNREACHABLE();
    return QString();
}

QVariant GroupSummary::makeValue(const VariantItems &list, const QVariant &original,
                                 const ColumnFriendlyName::FriendlyNameStruct &obj) const
{
    QVariant result = original;
    QVariant referenceValue;
    const bool ignoreEmptyOrNull = (obj.m_groupMethod == ColumnFriendlyName::DashIfDifferentIgnoreEmptyOrNULL);
    const bool sameValue = isSameValue(list, ignoreEmptyOrNull, referenceValue);

    switch(obj.m_groupMethod)
    {
    case ColumnFriendlyName::DashIfDifferent:
    case ColumnFriendlyName::DashIfDifferentIgnoreEmptyOrNULL:
        if (sameValue) {
            // if same value, use any of the children as representative
            result = referenceValue.isValid() ? referenceValue : result;
        } else {
            result = QStringLiteral("--");
        }
        break;
    case ColumnFriendlyName::Counter_ShowOriginalFirst:
        // assumes the list only contains immedidate children
        result = QStringLiteral("%1 (%2)").arg(original.toString()).arg(QString::number(list.size()));
        break;
    case ColumnFriendlyName::Sum:
        result = makeSum(list,obj);
        break;
    case ColumnFriendlyName::Range:
        result = sameValue ? referenceValue : rangeToString(list, obj);
        break;
    case ColumnFriendlyName::List:
        result = sameValue ? referenceValue : makeConcatList(list, obj, true);
        break;
    case ColumnFriendlyName::ListKeepDuplicates:
        result = makeConcatList(list, obj, false);
        break;
    default:
        break;
    }

    return result;
}

void GroupSummary::updateNode(RowNode *groupNode, int indexUseImmediateChildren) const
{
    if (groupNode == nullptr) {
        debugCoreMini() << "group_node nullptr";
        return;
    }
    if (!groupNode->isGroupNode()) {
        return;
    }

    RowNode *root = groupNode->getRoot();

    //For each root column name
    for (int column = 0; column < root->getRowSize(); ++column) {
        const QString key = root->getDataAt(column).toString();
        if (m_keyType.contains(key)) {
            VariantItems values;
            if (m_avoidAddingDuplicatedBrackets && m_keyType[key].m_groupMethod == ColumnFriendlyName::Counter_ShowOriginalFirst) {
                continue;
            }
            if (m_keyType[key].m_childCollectionMethod == ColumnFriendlyName::UseChildrenImmedidateOnly) {
                for (int i = 0; i < groupNode->getChildListSize(); i++) {
                    RowNode * node = groupNode->getChild(i);
                    if (indexUseImmediateChildren >= 0) {
                        if (node->getDataAt(indexUseImmediateChildren).toInt() <= 0) {
                            continue;
                        }
                    }
                    values.push_back(node->getDataAt(column));
                }
                //If no children are found, do not make any changes.  E.g. Do not make sum changes
                if (values.isEmpty()) {
                    continue;
                }
            } else {
                //Collect values for the given column
                RowNodePtrList rowList;
                //collect leaf nodes and avoid adding group nodes within groups
                getLeafRowNodeList(*groupNode, rowList);
                for (int i = 0; i < rowList.size(); ++i) {
                    RowNode *node = rowList[i];
                    values.push_back(node->getDataAt(column));
                }
            }
            const QVariant val = makeValue(values, groupNode->getDataAt(column), m_keyType[key]);
            groupNode->getDataAt(column) = val;
        }
    }
}

Err readFromJSON(RowNode * ths, const QString & filename)
{
    Err e = kNoErr;
    QByteArray jsonContent;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        rrr(kFileOpenError);
    }
    jsonContent = file.readAll();

    pmi::rn::parse(jsonContent, ths);

    return e;
}

Err writeByteArray(const QString & filename, QByteArray & jsonContent)
{
    Err e = kNoErr;
    QFile f( filename );
    if (!f.open( QIODevice::WriteOnly)) {
        rrr(kFileOpenError);
    }
    f.write(jsonContent);
    f.close();

    return e;
}

Err writeToJSON(RowNode * ths, const QString & filename)
{
    Err e = kNoErr;
//#ifndef _DEBUG  //TODO: Qt Creator / VS doesn't recognize this flag...
    QByteArray jsonContent = rn::serialize(ths);
    e = writeByteArray(filename, jsonContent); ree;
//#endif

    return e;
}

} //rn

Err writeToJSON_RowNodeList(const QList<const RowNode*> & rowList, const QString & filename)
{
    Err e = kNoErr;
    RowNode finalRoot;
    finalRoot.addValue(kGroupedRowNodeList);
    for (int i = 0; i < rowList.size(); i++) {
        RowNode * childRoot = new RowNode;
        childRoot->deepCopy(rowList[i]);
        finalRoot.addChild(childRoot);
    }
    e = rn::writeToJSON(&finalRoot, filename); ree;

    return e;
}

Err readFromJSON_RowNodeList(const QString & filename, QList<RowNode> & rowList)
{
    Err e = kNoErr;
    rowList.clear();
    RowNode finalRoot;
    e = rn::readFromJSON(&finalRoot, filename); ree;

    if (finalRoot.getRowData().size() == 1 && finalRoot.getRowData()[0].toString() == kGroupedRowNodeList) {
        for (int i = 0; i < finalRoot.getChildListSize(); i++) {
            rowList.push_back(RowNode());
            RowNode & childRoot = rowList.back();
            childRoot.deepCopy(finalRoot.getChild(i));
        }
    } else {
        rowList.push_back(RowNode());
        RowNode & childRoot = rowList.back();
        childRoot.deepCopy(&finalRoot);
    }

    return e;
}

Err writeToJSON_RowNodeSet(const QMap<QString, RowNode>& rowSet, const QString & filename)
{
    Err e = kNoErr;
    QByteArray jsonContent = convertToJSON_RowNodeSet(rowSet);
    e = rn::writeByteArray(filename, jsonContent); ree;

    return e;
}

Err readFromJSON_RowNodeSet(const QString & filename, QMap<QString, RowNode>* rowSet)
{
    Err e = kNoErr;
    QMap<QString, RowNode>& tmpRowSet = *rowSet;
    tmpRowSet.clear();

    RowNode finalRoot;
    e = rn::readFromJSON(&finalRoot, filename); ree;

    if (finalRoot.getChildListSize() != finalRoot.getRowDataToStringList().size()) {
        rrr(kBadParameterError);
    }

    for (int i = 0; i < finalRoot.getChildListSize(); i++) {
        QString key = finalRoot.getRowData()[i].toString();
        RowNode *childRoot = finalRoot.getChild(i);
        if (!tmpRowSet.contains(key)) {
            tmpRowSet.insert(key, *childRoot);
        } else {
            tmpRowSet[key] = *childRoot;
        }
    }

    return e;
}

QByteArray convertToJSON_RowNodeSet(const QMap<QString, RowNode>& rowSet)
{
    RowNode finalRoot;

    QMap<QString, RowNode>::const_iterator i;
    for (i = rowSet.begin(); i != rowSet.end(); i++) {
        RowNode *childRoot = new RowNode(i.value());

        finalRoot.addData(QStringList() << i.key());
        finalRoot.addChild(childRoot);
    }

    QByteArray jsonContent = rn::serialize(&finalRoot);
    return jsonContent;
}

Err convertFromJSON_RowNodeSet(const QByteArray & jsonContent, QMap<QString, RowNode>* rowSet)
{
    Err e = kNoErr;
    QMap<QString, RowNode>& tmpRowSet = *rowSet;
    tmpRowSet.clear();

    RowNode finalRoot;
    rn::parse(jsonContent, &finalRoot);

    if (finalRoot.getChildListSize() != finalRoot.getRowDataToStringList().size()) {
        rrr(kBadParameterError);
    }

    for (int i = 0; i < finalRoot.getChildListSize(); i++) {
        QString key = finalRoot.getRowData()[i].toString();
        RowNode *childRoot = finalRoot.getChild(i);
        if (!tmpRowSet.contains(key)) {
            tmpRowSet.insert(key, *childRoot);
        } else {
            tmpRowSet[key] = *childRoot;
        }
    }

    return e;
}

} //pmi
