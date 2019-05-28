/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#include "RowNode.h"
#include "common_errors.h"
#include "EntryDefMap.h"
#include "PmiQtCommonConstants.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <QtGlobal>
#include <QFile>
#include <QDomElement>
#include <QDomDocument>
#include <QStringList>

#include <QtSqlUtils.h>
#include <csv_parser.hpp>
#include <qt_string_utils.h>

using namespace std;
using namespace pmi;

namespace pmi {
PMI_COMMON_CORE_MINI_EXPORT QVariant g_dummy_variant(QLatin1String("Dummy Variant"));
}

static void
indent(ostream & os, int w)
{
    for (int i = 0; i < w; i++)
        os << " ";
}

colIdx RowNode::getColumnIndexFromName(const QString & fieldName, bool showWarning) const
{
    const RowNode * root = getRoot();
    colIdx cindex = root->m_rowData.indexOf(fieldName);

    if(showWarning && cindex < 0) {
        static int count = 0;
        if (count++ < 50) {
            if (cindex < 0) {
                warningCoreMini() << "getColumnIndexFromName() returns cindex = " << (int)cindex << ","
                           << "input value = " << fieldName << endl;
            }
        }
    }

    return cindex;
}

QString RowNode::getNameFromColumnIndex(colIdx index) const
{
    const RowNode * root = getRoot();
    QVariant columnData = root->getDataAt(index);
    return columnData.toString();
}

void
RowNode::_print_data(ostream & os, int w, const QString & seperator, bool showDash) const
{
    int i;
    indent(os, w);
    for (i = 0; i < m_rowData.size(); i++) {
        if (i != 0) {
            os << qPrintable(seperator); //"|";
        }
        if (showDash)
            os << i << "-";
        os << qPrintable(m_rowData[i].toString());
    }
    os << endl;
}

void
RowNode::_print_this_with_root(ostream & os, const QString & seperator, bool showDash) const
{
//    if (!show_debug_level(1))
//        return;

    const RowNode * root = getRoot();
    root->_print_data(os,0,seperator, showDash);
    this->_print_data(os,3,seperator, showDash);
}

void
RowNode::print(ostream & os, int w, const QString & seperator, bool showDash) const
{
//    if (force_print == 0 && !show_debug_level(1))
//        return;

    if (m_rowData.size() <= 0) {
        os << "empty" << endl;
        return;
    }

    _print_data(os,w, seperator, showDash);

    for (int i = 0; i < m_childPtrList.size(); i++) {
        m_childPtrList[i]->print(os, w + 3, seperator, showDash);
    }
}

void RowNode::print(const QString & filename) const
{
    std::ofstream outf(filename.toStdWString().c_str());
    if (outf.is_open()) {
        debugCoreMini() << "Writing RowNode::print() to file=" << filename;
        print(outf);
    }
}

RowNode*
RowNode::hasSearchRecursive(colIdx index, const QVariant & val)
{
//    cout << "index = " << index << ". rowData[index]" << qPrintable(rowData[index].toString()) << " val =" << qPrintable(val.toString()) << endl;
    if (m_rowData[index] == val) {
        return this;
    }

    for (int i = 0; i < m_childPtrList.size(); i++) {
        RowNode * child = m_childPtrList[i]->hasSearchRecursive(index, val);
        if (child)
            return child;
    }
    return NULL;
}

RowNode*
RowNode::hasSearchRecursive2(colIdx index, const QVariant & val, colIdx index2, const QVariant & val2)
{
    if (m_rowData[index] == val && m_rowData[index2] == val2) {
        return this;
    }

    for (int i = 0; i < m_childPtrList.size(); i++) {
        RowNode * child = m_childPtrList[i]->hasSearchRecursive2(index, val, index2, val2);
        if (child)
            return child;
    }
    return NULL;
}

//"1" or "3.1" or ...
QString RowNode::getRowNumberAsString() const {
    QString rowNumber = "";

    RowNode * ptr = (RowNode*) this;
    if(ptr->parent() ==NULL){
        rowNumber = "Row no.";
        return rowNumber;
    }
    while(ptr->parent()){
        if (rowNumber.size() > 0)
            rowNumber =  "." + rowNumber;
        RowNode * parentNode = ptr->parent();
        rowNumber = QString( "%1" ).arg(parentNode->getChildIndex(ptr)+1) +rowNumber ;
        ptr = ptr->parent();
    }
    // remove first dot
    //rowNumber.remove(0,1);
    return rowNumber;
}

void
RowNode::delinkChildrenRecursive(int call_level)
{
    if (call_level == 0) {
        _clearRowNodePointerHashRoot();
    }
    for (int i = 0; i < m_childPtrList.size(); i++) {
        RowNode * child = m_childPtrList[i];
        child->delinkChildrenRecursive(call_level+1);
    }
    m_childPtrList.clear();

//    static const bool delink_parent_pointers = true;
//    if (delink_parent_pointers)
    m_parentPtr = NULL;
}

PMI_COMMON_CORE_MINI_EXPORT QVariant getDataWithFieldName(const RowNode *ths, const QString &fieldName, bool *ok)
{
    if (ths == ths->getRoot()) {
        debugCoreMini() << "getDataWithFieldName called on root node.  Are you sure you want to do this?" << endl;
    }
    if (ok) {
        *ok = false;
    }
    const RowNode *root = ths->getRoot();
    colIdx cindex = root->getDataIndexOf(fieldName);
    Q_ASSERT(cindex < ths->getRowSize() && cindex >= 0);
    if (ths->getRowSize() > 0 ) {
        if (ok) {
            *ok = true;
        }
        return ths->getDataAt(cindex);
    }
    return QVariant();
}

QVariant& RowNode::getDataAt(const QString & fieldName, bool *ok, bool ignoreRootWarning)
{
    if (!ignoreRootWarning && this == this->getRoot()) {
        debugCoreMini() << "getDataAt called on root node.  Are you sure you want to do this?" << endl;
    }
    if (ok) {
        *ok = false;
    }
    const RowNode * root = this->getRoot();
    if (!ignoreRootWarning && root == this) {
        cout << "This is root node.  Did you really mean to call getDataAt?" << endl;
        cerr << "This is root node.  Did you really mean to call getDataAt?" << endl;
    }
    colIdx cindex = getColumnIndexFromName(fieldName, !ignoreRootWarning);
    if (cindex >= 0 && cindex < this->getRowSize()) {
        if (ok) {
            *ok = true;
        }
        return this->getDataAt(cindex);
    }
    g_dummy_variant = QVariant();
    return g_dummy_variant;
}

const QVariant &
RowNode::getDataAt(const QString & fieldName, bool *ok, bool ignoreRootWarning) const
{
    if (ok)
        *ok = false;
    const RowNode * root = this->getRoot();
    if (!ignoreRootWarning && root == this) {
        cout << "This is root node.  Did you really mean to call getDataAt?" << endl;
        cerr << "This is root node.  Did you really mean to call getDataAt?" << endl;
    }

    colIdx cindex = getColumnIndexFromName(fieldName);
    if (cindex >= 0 && cindex < this->getRowSize()) {
        if (ok) *ok = true;
        return this->getDataAt(cindex);
    }
    g_dummy_variant = QVariant();
    return g_dummy_variant;
}

/*!
 * @brief Recursively copies the data from one RowNode to another. Helper Function to Copy.
 * Note: this function does not clear dest before assignment.  This is a recursive function, so be careful how this is used.
 * @param rownode The source from which to copy
 * @param dest The destination to copy
 */
static void
DeepCopy(RowNode* dest, const RowNode* rownode, bool recursive = true)
{
    dest->addData(rownode->getRowData());
    if (recursive) {
        for(int i = 0; i < rownode->getChildListSize(); ++i)
        {
            RowNode* child = new RowNode();
            dest->addChild(child);
            DeepCopy(child,rownode->getChild(i), recursive);
        }
    }
}

static void
DeepCopyPreserveDestRoot_Slow(RowNode* dest, const RowNode* rownode, const QStringList & rootNames, bool recursive = true)
{
    //Note: This implementation is slow since we need to find 'name' within each loop; consider upgrading by providing src/dest index values instead of names.
    if (dest->isRoot()) {
        //Don't do copy of root as this function is trying to preseve root content and there's no need to duplicate work.
    } else {
        foreach(const QString & name, rootNames) {
            dest->getDataAt(name) = rownode->getDataAt(name);
        }
    }
    if (recursive) {
        for(int i = 0; i < rownode->getChildListSize(); ++i)
        {
            RowNode * child = dest->addNewChildMatchRootDataSize();
            DeepCopyPreserveDestRoot_Slow(child,rownode->getChild(i), rootNames, recursive);
        }
    }
}

/*!
 * @brief Recursively copies the data from one RowNode to another. Helper Function to Copy.
 * Note: this function does not clear dest before assignment.  This is a recursive function, so be careful how this is used.
 * @param rownode The source from which to copy
 * @param dest The destination to copy
 */
static void
DeepCopySelective(RowNode* dest, const RowNode* rownode, const QVector<colIdx> & colIndexList, bool recursive = true)
{
    for(int i = 0; i < colIndexList.size(); ++i)
    {
        colIdx idx = colIndexList[i];
        dest->addValue(rownode->getDataAt(idx));
    }
    if (recursive) {
        for(int i = 0; i < rownode->getChildListSize(); ++i)
        {
            RowNode* child = new RowNode();
            dest->addChild(child);
            DeepCopySelective(child,rownode->getChild(i), colIndexList, recursive);
        }
    }
}

/*!
 * @brief Recursively copies the data from one RowNode to another; keeps header structure at root node.
 * @param src The source from which to copy
 */
static Err
ClearAndCopyRowWithHeader(RowNode* dest, const RowNode* srcNode, const QStringList & headersOfInterest, bool recursive)
{
    Err e = kNoErr;
    QVector<colIdx> colIndexList;
    for (int i = 0; i < headersOfInterest.size(); i++) {
        const QString & header = headersOfInterest[i];
        colIdx idx = srcNode->getRootDataIndexOf(header);
        if (idx < 0) {
            cerr << "error, column header<"<<header.toStdString() <<"> does not exists" << endl;
            e = kBadParameterError; ree;
        }
        colIndexList.push_back(idx);
    }

    dest->clear();
    if (srcNode->isRoot()) {
        //If root and recurisve is false, then just copy the header/root; we might decide to change this to immedidate children instead for convenience.
        //If root and recurisve is true, then copy the header/root and all its children.
        DeepCopySelective(dest, srcNode, colIndexList, recursive);
    } else {
        //If not root, then make a copy of the root and the srcNode
        const RowNode * root = srcNode->getRoot();
        DeepCopySelective(dest, root, colIndexList, false);

        //copy content of srcNode
        RowNode* child = new RowNode();
        dest->addChild(child);
        DeepCopySelective(child, srcNode, colIndexList, recursive);
    }

    return e;
}

/*!
 * @brief Recursively sets a column data, except the root node, which would contain the header data.
 * @param dest the row to edit
 * @param idx the column index to edit
 * @param value the value to assign
 * @param recursive to change @dest children
 */
static void
setValueColumnIndexExceptRoot(RowNode* dest, colIdx idx, const QVariant & value, bool recursive)
{
    if (!dest->isRoot()) {
        dest->getDataAt(idx) = value;
    }
    if (recursive) {
        for(int i = 0; i < dest->getChildListSize(); ++i) {
            setValueColumnIndexExceptRoot(dest->getChild(i), idx, value, recursive);
        }
    }
}

pmi::Err
RowNode::copyContentWithHeaderClearFirst(const RowNode* src, const QStringList & headersOfInterest, bool recursive)
{
    Err e = kNoErr;
    if (!this->isRoot()) {
        e = kBadParameterError; ree;
    }
    //remove all your contents..
    this->clear();
    if (headersOfInterest.size() <= 0) {
        e = ::ClearAndCopyRowWithHeader(this, src, src->getRootDataAsStringList(), recursive); eee;
    } else {
        e = ::ClearAndCopyRowWithHeader(this, src, headersOfInterest, recursive); eee;
    }
error:
    return e;
}

pmi::Err
RowNode::setContentValuesExceptHeader(const QString & header, const QVariant & value, bool recursive)
{
    pmi::Err e = pmi::kNoErr;
    colIdx idx = this->getRootDataIndexOf(header);
    if (idx < 0) {
        cerr << "error, column header<"<<header.toStdString() <<"> does not exists" << endl;
        e = kBadParameterError; ree;
    }
    setValueColumnIndexExceptRoot(this, idx, value, recursive);
    return e;
}

pmi::Err
RowNode::setContentValuesExceptHeader(const QStringList & headerList, const VariantItems &valueList, bool recursive)
{
    pmi::Err e = pmi::kNoErr;
    if (headerList.size() != valueList.size()) {
        e = kBadParameterError; eee;
    }
    for (int i = 0; i < headerList.size(); i++) {
        e = setContentValuesExceptHeader(headerList[i], valueList[i], recursive); eee;
    }
error:
    return e;
}

void
RowNode::deepCopy(const RowNode* src, DeepCopyType type)
{
    if (type == DeepCopyMatchSource) {
        //remove all your contents..
        this->clear();
        ::DeepCopy(this,src);
        return;
    }
    QStringList destNames = this->getRowDataToStringList();
    QStringList srcNames = src->getRootDataAsStringList();

    QStringList namesOfInterest;
    if (type == DeepCopyPreserveDest_slow) {
        //make sure the source contains the names of interest before we try and copy them.
        foreach(QString srcname, srcNames) {
            if (destNames.contains(srcname)) {
                namesOfInterest << srcname;
            }
        }
    } else if (type == DeepCopyPreserveDestAppendSource_slow) {
        //only copy content if src has it; otherwise it's a waste of time as we are always creating new child, which would be empty.
        foreach(QString destname, destNames) {
            if (srcNames.contains(destname)) {
                namesOfInterest << destname;
            }
        }

        //if dest doesn't contain of the source names, then add them and also add to root data.
        foreach(QString srcname, srcNames) {
            if (!destNames.contains(srcname)) {
                this->addValue(srcname);
                namesOfInterest << srcname;
            }
        }
    }

    this->clearChildren();
    ::DeepCopyPreserveDestRoot_Slow(this, src, namesOfInterest, true);
}

RowNode::RowNode(const RowNode & obj) {
    m_parentPtr = NULL; //Note: do we need this?
    ::DeepCopy(this,&obj);
}

RowNode& RowNode::operator= (const RowNode &cSource) {
    if (this == &cSource)
        return *this;

    this->deepCopy(&cSource);
    return *this;
}

/// QDataStream read and write functionality for RowNode
void
RowNode::writeToQDataStream(QDataStream& out) const
{
    writeToQDataStream(out,this);
}

void
RowNode::writeToQDataStream(QDataStream& out, const RowNode* row) const
{
    out << row->getRowSize();
    for (int i = 0; i < row->getRowSize(); ++i) {
        if (row->getDataAt(i).isValid()) {
            out << row->getDataAt(i);
        } else {
            //ML-2378: necessary to avoid problem with serialization of data
            out << QVariant("");
        }
    }

    out << row->getChildListSize();
    for (int i = 0; i < row->getChildListSize(); ++i) {
        writeToQDataStream(out,row->getChild(i));
    }
}

Err
RowNode::readFromQDataStream(QDataStream &in)
{
    clear();
    return readFromQDataStream(in,this);
}

Err
RowNode::readFromQDataStream(QDataStream& in,RowNode* row)
{
    int rowSize = 0;
    QVariant data;

    if (in.status() == QDataStream::ReadCorruptData) {
        return kBadParameterError;
    }
    in >> rowSize;
    for(int i = 0; i < rowSize; ++i)
    {
        if (in.status() == QDataStream::ReadCorruptData) {
            return kBadParameterError;
        }
        in >> data;
        row->addValue(data);
    }

    if (in.status() == QDataStream::ReadCorruptData) {
        return kBadParameterError;
    }
    int childSize = 0;
    in >> childSize;

    for(int i = 0; i < childSize; ++i)
    {
        if (in.status() == QDataStream::ReadCorruptData) {
            return kBadParameterError;
        }
        RowNode* child = new RowNode();
        child->setParent(row);
        row->addChild(child);
        readFromQDataStream(in,child);
    }
    if (in.status() == QDataStream::ReadCorruptData)
        return kBadParameterError;
    return kNoErr;
}

/*
void
RowNode::writeToJSON(std::ostream& out)
{
    writeToJSON(out,this);
}

void
RowNode::writeToJSON(std::ostream& out,RowNode* row)
{
    out << "[";
    for(int i = 0; i < row->getRowSize(); ++i)
    {
        QVariant data = row->getDataAt(i);

        if(data == QVariant::Int)
            out << data.toInt();
        else if(data == QVariant::Double)
            out << data.toDouble();
        else if(data == QVariant::Bool)
            out << (data.toBool() ? "true" : "false");
        else
            out << data.toString().toStdString();
        out << (row->getChildListSize() ? ",":"");
    }

    for(int i = 0; i < row->getChildListSize(); ++i)
        writeToJSON(out,row->getChild(i));

    out << "]" << std::endl;
}

void
RowNode::readFromJSON(std::istream &in)
{
    clear();
    readFromJSON(in,this);
}

void
RowNode::readFromJSON(std::istream& in,RowNode* row)
{
    char val;
    in >> val; /// read "["

    while((char)in.peek() != ']')
    {
        QString data = "";
        while((char)in.peek() != ',')
        {
            in >> val;
            data += val;
        }
        row->addData(data.trimmed());
        if((char)in.peek() == '[')
            readFromJSON(in,row);
    }

    in >> val; /// read "]"
}
*/

/*Err RowNode::writeToCSV(const QString & filename) {
    Err e = kNoErr;
    QByteArray jsonContent = rn::serialize(this);
    QFile f( filename );
    bool sucess = f.open( QIODevice::WriteOnly );
    if (sucess == false) {
        e = kFileOpenError; eee;
    }
    f.write(jsonContent);
    f.close();
error:
    return e;
}*/

static void
findMatchingContentRecursive(RowNode * node, const QList<colIdx> & idxList, const VariantItems & valVector,
bool recurse, QList<RowNode*> & list)
{
    if (node == NULL)
        return;

    int i, size = node->getChildListSize();
    for (i = 0; i < size; i++) {
        RowNode * child = node->getChild(i);
        bool match = true;
        for (int j = 0; j < idxList.size(); j++) {
            const QVariant & inputVal = child->getDataAt(idxList[j]);
            const QVariant & matchVal = valVector[j];
            if (inputVal != matchVal) { //? QVariant comparison good enough? Need to convert first?
                match = false;
            }
        }
        if (match)
            list.push_back(child);
        if (recurse)
            findMatchingContentRecursive(child, idxList, valVector, recurse, list);
    }
}

Err
RowNode::getRowNodeListMatchingContent(const RowNode & content, bool recursive, QList<RowNode*> & outList) {
    Err e = kNoErr;
    outList.clear();
    if (content.getChildListSize() != 1) {
        cerr << "Incorrect structure. Did not expect children size other than one:" << content.getChildListSize() << endl;
        e = kBadParameterError; ree;
    }
    //prepare content
    QList<colIdx> idxList;
    QStringList headerList = content.getRootDataAsStringList();
    VariantItems & valVector = content.getChild(0)->getRowData();
    for (int i = 0; i < headerList.size(); i++) {
        colIdx idx = this->getDataIndexOf(headerList[i]);
        if (idx < 0) {
            cerr << "Did not find header name of:" << headerList[i].toStdString() << endl;
            e = kBadParameterError; ree;
        }
        idxList.push_back(idx);
    }

    //get the matches
    findMatchingContentRecursive(this, idxList, valVector, recursive, outList);

//error:
    return e;
}

/// Xml read and write functionality for RowNode

#ifdef XMLTESTCODE
void
RowNode::writeToXml(QDomDocument* doc, QDomElement* root)
{
    writeToXml(doc,root,this);
}

void
RowNode::writeToXml(QDomDocument* doc, QDomElement* root,RowNode* row)
{
    QDomElement node = doc->createElement("node");
    root->appendChild(node);

    node.setAttribute("size", row->getRowSize());
    for(int i = 0; i < row->getRowSize(); ++i)
    {
        QDomElement data = doc->createElement("data");
        node.appendChild(data);

        QString name = row->getRoot()->getDataAt(i).toString();
        QString value = row->getDataAt(i).toString();
        data.setAttribute("name", name);
        data.setAttribute("value", value);
    }

    for(int i = 0; i < row->getChildListSize(); ++i)
        writeToXml(doc, &node,row->getChild(i));
}

void
RowNode::readFromXml(QDomDocument* doc, QDomElement* root)
{
    clear();
    readFromXml(doc, root,this);
}

void
RowNode::readFromXml(QDomDocument*, QDomElement*,RowNode*)
{
//    int rowSize = 0;
//    QVariant data;

//    in >> rowSize;
//    for(int i = 0; i < rowSize; ++i)
//    {
//        in >> data;
//        row->addData(data);
//    }

//    int childSize = 0;
//    in >> childSize;
//    for(int i = 0; i < childSize; ++i)
//    {
//        RowNode* child = new RowNode();
//        child->setParent(row);
//        row->addChild(child);
//        readFromQDataStream(in,child);
//    }
}
#endif //XMLTESTCODE

pmi::Err RowNode::clearAndCopyHeaderAndRowList(const QList<RowNode*> & rows) {
    Err e = kNoErr;
    clear();
    if (rows.size() <= 0)
        return kNoErr;
    //Check to make sure the rows are from the same parent
    const RowNode * root = rows[0]->getRoot();
    for (int i = 1; i < rows.size(); i++) {
        if (root != rows[i]->getRoot()) {
            e = kBadParameterError; eee;
        }
    }

    //Copy the root header
    m_rowData = root->getRowData();

    //For each row, deep copy
    for (int i = 0; i < rows.size(); i++) {
        RowNode * newrow = new RowNode;
        newrow->deepCopy(rows[i]);
        addChild(newrow);
    }

error:
    return e;
}
static RecursionMessage
add_one_data(RowNode *node, void * data, std::vector<RowNode*> * /*list*/)
{
    QVariant * val = (QVariant*) data;
    node->addValue(*val);
    return RecursionContinue;
}

static RecursionMessage
prepend_one_data(RowNode *node, void * data, std::vector<RowNode*> * /*list*/)
{
    QVariant * val = (QVariant*) data;
    node->getRowData().prepend(*val);
    return RecursionContinue;
}

void RowNode::addDataToRootAndResizeChildren(const QString & colName, const QVariant & defaultVal, bool avoidDuplicate) {
    if (avoidDuplicate && this->rootHasData(colName)) {
        debugCoreMini() << "Root already contains " << colName << ". Duplicate not added.";
        return;
    }
    if (colName.isEmpty()) {
        warningCoreMini() << "Column name is empty.";
        return;
    }
    this->getRoot()->addValue(colName);
    AddChildrenRecursive(this->getRoot(), NULL, add_one_data, (void*)&defaultVal);
}

void RowNode::prependDataToRootAndResizeChildren(const QString & colName, const QVariant & defaultVal, bool avoidDuplicate) {
    if (avoidDuplicate && this->rootHasData(colName)) {
        debugCoreMini() << "root already contains " << colName << ". Duplicate not added.";
        return;
    }
    this->getRoot()->getRowData().prepend(colName);
    AddChildrenRecursive(this, NULL, prepend_one_data, (void*) &defaultVal);
}

void RowNode::applyFunction(addnode f, void * data, std::vector<RowNode*> * list, ApplyFunction type)
{
    RecursionMessage recurse = RecursionStopAfterSiblings;
    bool isroot = isRoot();
    if ( (type == ApplyFunction_ThisNode_UnlessRoot && !isroot) ||
         (type == ApplyFunction_ThisNode_IncludeRoot) )
    {
        recurse = f(this, data, list);
        if (recurse == RecursionStopImmedidately) {
            return;
        }
    }
    recurse = pmi::AddChildrenRecursive(this, list, f, data);
}

static RecursionMessage remove_column(RowNode *node, void *data, std::vector<RowNode *> * /*list*/)
{
    ptrdiff_t index = reinterpret_cast<ptrdiff_t>(data);
    if (index >= 0 && index < node->getRowData().size()) {
        node->getRowData().removeAt(index);
    } else {
        debugCoreMini() << "Could not remove column index=" << index;
    }
    return RecursionContinue;
}

bool RowNode::removeColumn(const QString &colName)
{
    ptrdiff_t index = getDataIndexOf(colName);
    if (index < 0) {
        debugCoreMini() << "colName not found to remove:" << colName;
        return false;
    }
    applyFunction(remove_column, reinterpret_cast<void*>(index), NULL,
                  ApplyFunction_ThisNode_IncludeRoot);
    return true;
}

struct ValueIntOfInterest {
    int colIndex;
    int value;
    ValueIntOfInterest() {
        value = -1;
        colIndex = -1;
    }
};

static RecursionMessage
addnode_if_same_value_int(RowNode *node, void * data, std::vector<RowNode*> * list)
{
    ValueIntOfInterest *valptr = (ValueIntOfInterest*)(data);
    int id_node = toInt(node->getDataAt(valptr->colIndex));
    if (valptr->value == id_node) {
        if (list) list->push_back(node);
        //return RecursionStopImmedidately;
    }
    return RecursionContinue;
}

//TODO: handle this with generic values (e.g. QVariant).  This requires proper comparision between qvariants
//Maybe need code such as: value.type() == QVariant::String
void RowNode::collectRowNodeListWith(const QString & colName, int value, std::vector<RowNode*> & list)
{
    list.clear();
    ValueIntOfInterest data;
    data.value = value;
    data.colIndex = getRoot()->getDataIndexOf(colName);
    applyFunction(addnode_if_same_value_int, &data, &list, ApplyFunction_ThisNode_UnlessRoot);
}

pepId
getPIDFromVar(const QVariant & val)
{
    pepId pid = -1;
    pid = toInt(val,-1);
    return pid;
}

pepId
getPID(const RowNode * node)
{
    pepId pid = -1;
    if (node == NULL)
        return pid;

    bool ok = false;
    QVariant val = node->getDataAt(kPeptidesId, &ok);
    if (ok) {
        pid = getPIDFromVar(val);
    }
    return pid;
}

namespace pmi {
//TODO: stop calling this directly, but use RowNode::applyFunction
RecursionMessage
AddChildrenRecursive(const RowNode * node, std::vector<RowNode*> * list, addnode f, void * data)
{
    RecursionMessage recurse = RecursionStopAfterSiblings;
    if (node == NULL)
        return recurse;

    int i, size = node->getChildListSize();
    for (i = 0; i < size; i++) {
        RowNode * child = node->getChild(i);
        if (f) {
            recurse = f(child, data, list);
            if (recurse == RecursionContinue) {
                recurse = AddChildrenRecursive(child, list, f, data);
            }
        } else {
            if (list) list->push_back(child);
            recurse = AddChildrenRecursive(child, list, f, data);
        }
        if (recurse == RecursionStopImmedidately)
            return recurse;
    }
    return recurse;
}

//Get leaf RowNodes; We do not want group row nodes, which are duplicated information.
static RecursionMessage
add_nongroup_nodes(RowNode *node, void * /*data*/, std::vector<RowNode*> * list)
{
    if (node->getChildListSize() <= 0) {
        if (list) list->push_back(node);
    }
    return RecursionContinue;
}

static RecursionMessage
add_all_nodes(RowNode *node, void * /*data*/, std::vector<RowNode*> * list)
{
    if (list) list->push_back(node);
    return RecursionContinue;
}


//The content can either be a single row or a row group; The row group contains redundant information and we do not need it.
//Collect only the RowNode * of interest.
void getLeafRowNodeList(const RowNode & content, QList<RowNode*> & rowList, RowNode::ApplyFunction type) {
    rowList.clear();
    std::vector<RowNode*> list;
    AddChildrenRecursive(&content, &list, add_nongroup_nodes);

    bool isroot = content.isRoot();
    if ( (type == RowNode::ApplyFunction_ThisNode_UnlessRoot && !isroot) ||
         (type == RowNode::ApplyFunction_ThisNode_IncludeRoot) )
    {
        rowList.push_back((RowNode*)&content);
    }

    for (unsigned int i = 0; i < list.size(); i++) {
        rowList.push_back(list[i]);
    }
}

void getAllRowNodeList(const RowNode & input, QList<RowNode*> & rowList) {
    rowList.clear();
    std::vector<RowNode*> list;
    AddChildrenRecursive(&input, &list, add_all_nodes);
    for (unsigned int i = 0; i < list.size(); i++) {
        rowList.push_back(list[i]);
    }
}

void getLeafRowNodeListWithColumn(const RowNode & content, QList<RowNode*> & rowList, QString colName, QVariant value) {
    rowList.clear();
    colIdx idx = content.getRootDataIndexOf(colName);
    if (idx < 0) {
        return;
    }

    QString valueStr = value.toString();

    std::vector<RowNode*> list;
    AddChildrenRecursive(&content, &list, add_nongroup_nodes);
    for (unsigned int i = 0; i < list.size(); i++) {
        //Note: using string to be safe. Should use proper casting instead.
        if (list[i]->getDataAt(idx).toString() == valueStr) {
            rowList.push_back(list[i]);
        }
    }
}

RowNodeWriteParameter::RowNodeWriteParameter(QTextStream & s, bool doNumbering,
                      const qmap_strstr * rootNameFriendly,
                      const RowNode * rownode,
                      const qmap_str_qmap_strstr * colValuesFriendly,
                      QSqlQuery * q) :
    m_stream(s),
    m_numbering(doNumbering),
    m_rootColumnNameFriendly(rootNameFriendly),
    m_q(q),
    m_writeLocation(CSVStream)
{
    if (colValuesFriendly && rownode) {
        QList<QString> keys = colValuesFriendly->keys();
        foreach(QString key, keys) {
            colIdx idx = rownode->getRootDataIndexOf(key);
            if (idx >= 0) {
                qmap_strstr mapvalues = (*colValuesFriendly)[key];
                qmap_strstr & newmap = m_columnValueFriendly[idx];
                newmap = mapvalues;
            } else {
                debugCoreMini() << "RowNodeWriteParameter: Could not find column " << key;
            }
        }
    }
}

static void
streamRowData(RowNodeWriteParameter * param, const RowNode *node) {
    bool numbering = param->showNumbering();
    QTextStream & stream = param->getStream();
    if (stream.device() == NULL) {
        static int count = 0;
        if (count++ < 5) {
            debugCoreMini() << "stream device null. No writing";
        }
        return;
    }

    if(numbering)
        stream << node->getRowNumberAsString() << ",";

    const VariantItems & rowData = node->getRowData();
    bool isRoot = node->isRoot();
    for(int i = 0; i < rowData.size(); i++){
        QString content = rowData[i].toString().trimmed(); //
        if (isRoot) {
            content = param->columnRootName(content);
        } else {
            content = param->columnValueName(i, content);
        }
        if (i > 0)
            stream << ",";
        if(content.indexOf(",") >=0 || content.indexOf("\n") >= 0)
            content = "\"" +content + "\"";
        stream << content;
    }
    stream << "\n";
}

//modsNameList: Glu->Leu  (E) [-15.9585];  (NGlycan) [1606.5867]
//glycanList: "" or "Hex(1)bla(2)"
static Err
matchGlycanFromModsList(const QStringList & modsNameList, const QStringList & glyList, QStringList & newList)
{
    Err e = kNoErr;
    newList.clear();
    QList<int> mapList;
    int gly_count = 0;
    if (modsNameList.size() < glyList.size()) {
        debugCoreMini() << "More mods than glycan mods. modsNameList:" << modsNameList << " glyList:" << glyList;
        e = kBadParameterError; eee;
    }

    for(int i = 0; i < modsNameList.size(); i++) {
        if (modsNameList[i].contains("gly",Qt::CaseInsensitive)) {
            if (gly_count < glyList.size()) {
                newList << glyList[gly_count++];
            } else {
                debugCoreMini() << "Not enough glycan mods to map. " << "gly_count=" << gly_count << " modsNameList:" << modsNameList << " glyList:" << glyList << " mapList:" << mapList;
                e = kBadParameterError; eee;
            }
        } else {
            newList << "";
        }
    }

error:
    if (e) newList.clear();
    return e;
}

//This will expand Byologic output by modification
static Err
expand_row_for_pivot_table(const RowNode * node, const VariantItems & rowData, QList<VariantItems> & vlist)
{
    Err e = kNoErr;
    QStringList rootNames = node->getRootDataAsStringList();
    QMap<QString,QStringList> col_modsList;
    QMap<QString,int> col_indexList;
    int mods = 0;

    vlist.clear();

    QStringList mustMatchList;
    //These are potential items to split on.  Some are required and some are not.
    mustMatchList << kModificationsNameList
                  << kModificationsIdList
                  << kModificationsProteinPositionList
                  << kModificationsPositionList
                  << kModificationsSummaryList
                  << kModificationsResidueList
                  ;

    foreach(QString key, mustMatchList) {
        col_modsList.insert(key, QStringList());
    }
    col_modsList.insert(kGlycanList, QStringList());

    if (rootNames.size() != rowData.size()) {
        e = kBadParameterError; eee;
    }

    //Get split data
    for (int i = 0; i < rootNames.size(); i++) {
        if (col_modsList.contains(rootNames[i])) {
            bool split_by_semicolon = rootNames[i] == kModificationsNameList;
            QString s = rowData[i].toString().trimmed();
            QStringList list;
            //E.g. "Gly->Asp(G) [58.0055]; Oxidation(M,W) [15.9949]"
            //split by ';' first, and then try ','
            //Note: Have to be careful when no token exists: "Oxidation(M,W) [15.9949]";
            //      don't want to split by "," in this case.
            if (s.contains(";") || split_by_semicolon) {
                list = s.split(";", QString::SkipEmptyParts);
            } else {
                list = s.split(",", QString::SkipEmptyParts);
            }
            col_modsList[rootNames[i]] = list;
            col_indexList[rootNames[i]] = i;
        }
    }

    mods = col_modsList[kModificationsNameList].size();
    if (mods > 0) {
        //Check to make sure all values are valid
        foreach(QString key, mustMatchList) {
            if (col_modsList[key].size() != mods) {
                debugCoreMini() << "Row content does not match expected mods = " << mods << "on key = " << key;
                debugCoreMini() << "splitValues[key]:" << col_modsList[key];
                debugCoreMini() << "rootNames:" << rootNames;
                //debugCoreMini() << "rowData:" << rowData;
                e = kBadParameterError; eee;
            }
        }

        //Glycan needs special attention
        if (col_modsList[kGlycanList].size() > 0) {
            QStringList glyList = col_modsList[kGlycanList];
            QStringList modsNameList = col_modsList[kModificationsNameList];
            QStringList newList;
            e = matchGlycanFromModsList(modsNameList,glyList, newList); eee;
            col_modsList[kGlycanList] = newList;
        }

        for (int i = 0; i < mods; i++) {
            vlist.push_back(rowData);
            VariantItems & row = vlist.back();
            foreach(QString key, col_modsList.keys()) {
                int index = col_indexList[key];
                row[index] = col_modsList[key].value(i).trimmed(); //note: kGlycanList can be out of bounds.
            }
        }
    }

error:
    //If no modifications, no need to split
    if (vlist.size() == 0) {
        vlist.push_back(rowData);
    }
    return e;
}


static Err
insertRowDataToDatabase(RowNodeWriteParameter * param, const RowNode *node) {
    Err e = kNoErr;
    if (param->writeLocation() == RowNodeWriteParameter::CSVStream) {
        static int count = 0;
        if (count++ < 5) {
            debugCoreMini() << "stream device null. No writing";
        }
        return kBadParameterError;
    }
    QSqlQuery & q = *param->query();

    QList< VariantItems > vlist;
    const VariantItems & rowData = node->getRowData();
    if (q.record().count() > rowData.size()) {
        debugCoreMini() << "q.record().count() > rowData.size()" << q.record().count() << "," << rowData.size();
        return kBadParameterError;
    }

    if (param->writeLocation() == RowNodeWriteParameter::DatabaseInsertForPivotInput) {
        expand_row_for_pivot_table(node, rowData, vlist);
    } else {
        vlist.push_back(rowData);
    }

    foreach(const VariantItems & rowData, vlist) {
        for(int i = 0; i < rowData.size(); i++){
            bool ok = false;
            const QString s = rowData[i].toString().trimmed();
            double fval = 0;
            int ival = 0;
            bool might_be_double = s.contains(".");
            if (might_be_double) {
                fval = rowData[i].toDouble(&ok);
            } else {
                ival = rowData[i].toInt(&ok);
            }
            if (ok) {
                if (might_be_double) {
                    q.bindValue(i,fval);
                } else {
                    q.bindValue(i,ival);
                }
            } else {
                q.bindValue(i,s);
            }
        }
        e = QEXEC_NOARG(q); eee;
    }

error:
    return e;
}


static RecursionMessage
write_direct_children(RowNode *node, void * p, std::vector<RowNode*> * ) {
    RowNodeWriteParameter* param = static_cast<RowNodeWriteParameter*> (p);
    if (param->writeLocation() == RowNodeWriteParameter::CSVStream) {
        streamRowData(param,node);
    } else {
        insertRowDataToDatabase(param,node);
    }
    return RecursionStopAfterSiblings;
}


static RecursionMessage
write_leaf_nodes(RowNode *node, void * p, std::vector<RowNode*> * ) {
    RowNodeWriteParameter* param = static_cast<RowNodeWriteParameter*> (p);
    if( node->getChildListSize() > 0 )
        return RecursionContinue;

    if (param->writeLocation() == RowNodeWriteParameter::CSVStream) {
        streamRowData(param,node);
    } else {
        insertRowDataToDatabase(param,node);
    }
    return RecursionContinue;
}

static RecursionMessage
write_nodes(RowNode *node, void * p, std::vector<RowNode*> * ) {
    RowNodeWriteParameter* param = static_cast<RowNodeWriteParameter*> (p);
    if (param->writeLocation() == RowNodeWriteParameter::CSVStream) {
        streamRowData(param,node);
    } else {
        insertRowDataToDatabase(param,node);
    }
    return RecursionContinue;
}

//The content can either be a single row or a row group; The row group contains redundant information and we do not need it.
//Collect only the RowNode * of interest.
//TODO: rename to write_to_csv_or_sqlite
QString writeToFile(QString outFilename, const RowNode & content, RowNodeLevel group, bool numbering,
                const RowNode * columnInfo) {
    Err e = kNoErr;
    qmap_strstr * colNameMap=NULL;
    qmap_str_qmap_strstr * colValueMap = NULL;

    QString errMsg;
    QTextStream stream;
    int opened_db = 0;
    if (!outFilename.endsWith(".sqlite", Qt::CaseInsensitive)) { //e.g. .csv, .seg
        QFile qfile(outFilename);
        if ( !qfile.open(QIODevice::WriteOnly | QIODevice::Text) ) {
            errMsg = QString("Cannot write to file:\n%1").arg(outFilename);
            e = kFileOpenError; eee;
        }
        stream.setDevice(&qfile);
        writeToCSV( stream, content, group, numbering, colNameMap, colValueMap, NULL, columnInfo);
        qfile.close();
    } else {
        QSqlDatabase db = QSqlDatabase::addDatabase(kQSQLITE, "write_rownode");
        opened_db = 1;
        db.setDatabaseName(outFilename);
        if (!db.open()) {
            errMsg = QString("Cannot write to file:\n%1").arg(outFilename);
            e = kFileOpenError; eee;
        }
        writeToCSV( stream, content, group, numbering, colNameMap, colValueMap, &db, columnInfo);
        if (db.open()) {
            db.close();
        }
    }
error:
    if (opened_db) QSqlDatabase::removeDatabase("write_rownode");
    return errMsg;
}

static Err
writeColumnDefinitions(QSqlDatabase & db, const RowNode * columnInfo)
{
    Err e = kNoErr;
    const QMap<QString,ColumnFriendlyName::FriendlyNameStruct> & coldef =
            ColumnFriendlyName::getColumnFriendlyNamesNoConstruct();

    QStringList keys = coldef.keys();
    QMap<QString, const RowNode *> col_rownode_map;
    if (columnInfo) {
        for (int i = 0; i < columnInfo->getChildListSize(); i++) {
            const RowNode * node = columnInfo->getChild(i);
            col_rownode_map[node->getDataAt(kColumnName).toString()] = node;
        }
    }
    //debugCoreMini() << "col_rownode_map=" << col_rownode_map;

    QSqlQuery q = makeQuery(db, true);
    e = QEXEC_CMD(q, "DROP TABLE IF EXISTS ColumnInfo"); eee;
    e = QEXEC_CMD(q, "CREATE TABLE IF NOT EXISTS ColumnInfo(Id INTEGER PRIMARY KEY \
                  , InternalName TEXT \
                  , DisplayName TEXT \
                  , Tooltip TEXT \
                  , DataType TEXT \
                  , SigFig INT \
                  , Visible INT \
                  , Width INT \
                  )"); eee;
    e = QPREPARE(q,
        "INSERT INTO ColumnInfo(InternalName, DisplayName, Tooltip, DataType, SigFig, Visible, Width)\
         VALUES (?,?,?,?,?,?,?)"); eee;

    foreach(QString key, keys) {
        const ColumnFriendlyName::FriendlyNameStruct & obj = coldef[key];
        q.bindValue(0, key);
        q.bindValue(1, obj.m_name);
        q.bindValue(2, obj.m_detail);
        q.bindValue(3, obj.typeToString());
        q.bindValue(4, obj.m_sigfig);

        const RowNode * node = NULL;
        if (col_rownode_map.contains(key)) {
            node = col_rownode_map.value(key);
        }
        if (node) {
            q.bindValue(5, node->getDataAt(kColumnVisible));
            q.bindValue(6, node->getDataAt(kColumnWidth));
        } else {
            q.bindValue(5, QVariant());
            q.bindValue(6, QVariant());
        }

        e = QEXEC_NOARG(q); eee;
    }

error:
    return e;
}

void writeToCSV(QTextStream & stream, const RowNode & content, RowNodeLevel group, bool numbering,
                qmap_strstr * colNameMap, qmap_str_qmap_strstr * colValueMap, QSqlDatabase * db,
                const RowNode * columnInfo) {
    Err e = kNoErr;
    std::vector<RowNode*> * listptr = NULL;
    QSqlQuery q;
    if (db) {
        q = makeQuery(*db, true);
    }
    RowNodeWriteParameter param(stream, numbering,colNameMap, &content, colValueMap, &q);
    QString cmd_create;
    QString cmd_insert;
    if (db) {
        QStringList collist = content.getRootDataAsStringList();
        collist = makeSqliteColumnNameReady(collist);
        QStringList qlist = makeDuplicateStringsToList("?", collist.size());
        cmd_create = QString("CREATE TABLE IF NOT EXISTS %2(Id INTEGER PRIMARY KEY, %1)").arg(collist.join(","));
        cmd_insert = QString("INSERT INTO %3(%1) VALUES(%2)").arg(collist.join(",")).arg(qlist.join(","));
        QString cmd_drop = QString("DROP TABLE IF EXISTS %1");

        e = writeColumnDefinitions(*db,columnInfo); eee;
        e = QEXEC_CMD(q, cmd_drop.arg("RowNodeUser")); eee;
        e = QEXEC_CMD(q, cmd_create.arg("RowNodeUser")); eee;
        e = QEXEC_CMD(q, cmd_drop.arg("RowNodePivotInput")); eee;
        e = QEXEC_CMD(q, cmd_create.arg("RowNodePivotInput")); eee;
    } else {
        param.setWriteLocation(RowNodeWriteParameter::CSVStream);
        /*Note : The AddChildrenRecursive function won't consider the root, need to handle root separately*/
        write_nodes(const_cast<RowNode*>(&content), &param, listptr);
    }
    if (db) {
        transaction_begin(*db);
        e = QPREPARE(q, cmd_insert.arg("RowNodeUser")); eee;
        param.setWriteLocation(RowNodeWriteParameter::DatabaseInsertForUser);
    }

    switch( group){
    case AllDescendantNodes:
         AddChildrenRecursive (&content, listptr, write_nodes, &param);
        break;
    case DirectChildNodes:
        //write_direct_children(const_cast<RowNode*>(& content), csvfile);
        AddChildrenRecursive (&content, listptr, write_direct_children, &param);
        break;
    case LeafNodes:
         AddChildrenRecursive (&content, listptr, write_leaf_nodes, &param);
        break;
    }

    if (db) {
        e = QPREPARE(q, cmd_insert.arg("RowNodePivotInput")); eee;
        param.setWriteLocation(RowNodeWriteParameter::DatabaseInsertForPivotInput);
        AddChildrenRecursive (&content, listptr, write_nodes, &param);
    }

error:
    if (db) {
        transaction_end(*db);
    }
    if (e) {
        warningCoreMini() << "Error writing";
    }
}

static VariantItems toVarList(const csv_row & inlist) {
    VariantItems outlist;
    for (unsigned int i = 0; i < inlist.size(); i++) {
        outlist.push_back(inlist[i].c_str());
    }
    return outlist;
}

bool readFromCSV( QString csvFileName, RowNode* rootNode) {
    const char field_terminator = ',';
    const char line_terminator  = '\n';
    const char enclosure_char   = '"';
    int row_count = 0;

    csv_parser file_parser;
    if( !QFile::exists(csvFileName))
        return false;
    file_parser.init( csvFileName.toLatin1().data() );

    file_parser.set_enclosed_char(enclosure_char, ENCLOSURE_OPTIONAL);
    file_parser.set_field_term_char(field_terminator);
    file_parser.set_line_term_char(line_terminator);

    csv_row row = file_parser.get_row();
    VariantItems fields = toVarList(row);


    rootNode->clear();
    rootNode->addData(fields);

    while(file_parser.has_more_rows()) {
        row_count++;

        csv_row row = file_parser.get_row();
        VariantItems fields = toVarList(row);

        RowNode* node =new RowNode();
        node->addData(fields);
        rootNode->addChild(node);
    }
    return true;

    /*
    foreach(QVariant v, rootNode->getRowData()){
        debugCoreMini() << v.toString();
    }

    foreach(RowNode* rowNode, rootNode->getChildPointerListReference()){
        foreach(QVariant v, rowNode->getRowData()){
            debugCoreMini() << v.toString();
        }
    }
    return true;
    */
}

void copyDataToChildren(RowNode * parent, const QList<colIdx> & columnIdxList) {
    for (int i = 0; i < parent->getChildListSize(); i++) {
        RowNode * child = parent->getChild(i);
        foreach(colIdx idx, columnIdxList) {
            child->getDataAt(idx) = parent->getDataAt(idx);
        }
    }
}

void copyDataToChildren(RowNode * parent, const QStringList & columnNames) {
    QList<colIdx> colIdxList;
    foreach(QString colName, columnNames) {
        colIdx idx = parent->getRootDataIndexOf(colName);
        if (idx <= 0) {
            debugCoreMini() << "warning, column name not found: " << colName;
        } else {
            colIdxList << idx;
        }
    }
    copyDataToChildren(parent, colIdxList);
}

} //pmi

class RowNodeDeepCopyTest {
public:
    RowNodeDeepCopyTest() {
        RowNode dest;
        RowNode src;
        dest.addData(VariantItems() << "c" << "d" << "a" << "b" );
        src.addData(VariantItems() << "c" << "f" << "d" );

        RowNode * child = src.addNewChildMatchRootDataSize();
        child->getDataAt("c") = "has c";
        child->getDataAt("f") = "has f";
        child->getDataAt("d") = "has d";

        dest.deepCopy(&src, RowNode::DeepCopyPreserveDest_slow);
        std::cout << "RowNode::DeepCopyMatchPreserveDest_slow" << endl;
        dest.print(std::cout);

        dest.deepCopy(&src, RowNode::DeepCopyPreserveDestAppendSource_slow);
        std::cout << "RowNode::DeepCopyMatchPreserveDestAppendSource_slow" << endl;
        dest.print(std::cout);
    }
};

//static RowNodeDeepCopyTest test;


