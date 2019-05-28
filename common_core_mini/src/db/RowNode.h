/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */


#ifndef ROW_NODE_H
#define ROW_NODE_H

#include <QList>
#include <QVariant>
#include <QModelIndex>
#include <QDataStream>
#include <QFile>
#include <QTextStream>
#include <QMap>
#include <QStringList>
#include <QRectF>

#include <iostream>

#include "common.h"
#include <pmi_common_core_mini_debug.h>
#include <pmi_common_core_mini_export.h>

class QDomElement;
class QDomDocument;
class QSqlDatabase;
class QSqlQuery;

class RowNode;

class PMI_COMMON_CORE_MINI_EXPORT VariantItems : protected QVector<QVariant> {
public:

    typedef QVector<QVariant>::const_iterator const_iterator;

    VariantItems() {}
    explicit VariantItems(std::initializer_list<QVariant> args) : QVector<QVariant>(args) { }
    VariantItems &operator<< (const QVariant &t) {
        invalidateHash();
        append(t); return *this;
    }

    QVector<QString> toVectorString() const {
        QVector<QString> list;
        list.reserve(QVector<QVariant>::size());
        for (int i = 0; i < size(); i++) {
            list.push_back(QVector<QVariant>::at(i).toString());
        }
        return list;
    }

    void clear() {
        m_colNameHash = QHash<QString,int>();
        QVector<QVariant>::clear();
    }

    void resize(int size) {
        QVector<QVariant>::resize(size);
    }

    const QVariant & at(int i) {
        return QVector<QVariant>::at(i);
    }

    const QVariant & at(int i) const{
        return QVector<QVariant>::at(i);
    }

    int size() const {
        return QVector<QVariant>::size();
    }

    int length() const {
        return QVector<QVariant>::length();
    }

    int count() const {
         return QVector<QVariant>::count();
    }

    bool isEmpty() const { return QVector<QVariant>::isEmpty(); }

    bool contains(const QString &t) const {
        if (m_useHash) {
            if (!isHashValid()) {
                //Hack the compiler with const issue; any better solution to do lazy update of hash?
                //Maybe we can consider up-front computation of hash after each update (e.g. update hash after push_back call)
                VariantItems * ths = (VariantItems*)this;
                ths->updateHash();
            }
            return m_colNameHash.contains(t);
        }
        debugCoreMini() << "VariantItems::contains should not be called unless m_useHash is explicitly disabled.";
        return QVector<QVariant>::contains(t);
    }

    int indexOf(const QString &t, int from = 0) const {
        if (m_useHash) {
            if (!isHashValid()) {
                //Hack the compiler with const issue; any better solution to do lazy update of hash?
                //Maybe we can consider up-front computation of hash after each update (e.g. update hash after push_back call)
                VariantItems * ths = (VariantItems*)this;
                ths->updateHash();
            }
            if (m_colNameHash.contains(t)) {
                return m_colNameHash[t];
            }
            return -1;
        }
        debugCoreMini() << "VariantItems::indexOf should not be called unless m_useHash is explicitly disabled.";
        return QVector<QVariant>::indexOf(t, from);
    }

    QVariant& operator [](int i) {
        //This reference operator[] can potentially modify the content, which would make the hash invalid.
        //So, we always assume using this reference operator[] invalidates the hash and must be re-computed.
        //Note: Another, maybe better approach is to not use these operators and replace with get/set functions.
        invalidateHash();
        return QVector<QVariant>::operator [](i);
    }

    const QVariant& operator [](int i) const {
       return QVector<QVariant>::operator [](i);
    }

    void push_back(const QVariant &t) {
        QVector<QVariant>::push_back(t);
        invalidateHash();  //don't recompute until required (at indexOf call).
    }

    void prepend(const QVariant &t) {
        QVector<QVariant>::prepend(t);
        invalidateHash();  //don't recompute until required (at indexOf call).
    }

    const_iterator begin() const {
        return QVector<QVariant>::constBegin();
    }

    const_iterator end() const {
        return QVector<QVariant>::constEnd();
    }

    void reserve(int size) {
        QVector<QVariant>::reserve(size);
    }

    void removeAt(int i) {
        QVector<QVariant>::removeAt(i);
        invalidateHash();  //don't recompute until required (at indexOf call).
    }

    void setUseHash(bool useHash) {
        m_useHash = useHash;
        if (m_useHash) {
            if(!isHashValid()) {
                updateHash();
            }
        } else {
            m_colNameHash.clear();
        }
    }

private:

    void invalidateHash() {
        //invalidate hash by clearing it; if this is too expensive, we will create a bool flag.
        m_colNameHash.clear();
    }

    void updateHash() {
        if(m_useHash) {
            m_colNameHash.clear();
            for (int i = 0; i < size(); i++) {
                m_colNameHash[at(i).toString()] = i;
            }
        }
    }

    bool isHashValid() const {
        //For now, do a simple check.  Later, we should check the content, but that can be expensive.
        if (m_colNameHash.size() != size())
            return false;

        return true;
    }

private:
    QHash<QString, int> m_colNameHash;
    bool m_useHash = true;  //enable by default.
};

/*!
 * @brief For the given row, this specifies the ith data entry (i.e. column index).
 * Trying to use type consistently, but no guarantees yet.
 */
typedef int colIdx;

class RowNode;

namespace pmi {
//Helper functions

enum RecursionMessage {
    RecursionContinue = 0,
    RecursionStopImmedidately,
    RecursionStopAfterSiblings
};

/*!
 * @brief call back for AddChildrenRecursive.
 * @return true if the function should recurse into the @node.
 */
typedef RecursionMessage (*addnode)(RowNode *node, void *data, std::vector<RowNode *> *list);

/*!
 * @brief This will add all children of xnode. Note this will not add the input xnode into the list.
 *
 * @param list  list of a children
 * @param node  input node. E.g. the root node.
 * @param f if not nullptr, this function will handle the adding to list
 * @param data f's data, if any
 * @return bool if false, the recursion stops; @f can determine when to stop recursion.
 */
PMI_COMMON_CORE_MINI_EXPORT RecursionMessage AddChildrenRecursive(const RowNode * xnode, std::vector<RowNode*> * list,
                                                            addnode f = nullptr, void * data = nullptr);

} //pmi

class PMI_COMMON_CORE_MINI_EXPORT RowNode {

protected:
    VariantItems m_rowData;
    RowNode * m_parentPtr;
    QList<RowNode*> m_childPtrList;

public:

    void setUseHash(bool val) {
        Q_UNUSED(val);
        /*
         * We should not need to set this and unset this anymore after proper VariantItems class.
        RowNode * root = getRoot();
        root->m_rowData.setUseHash(val);
        */
    }

    colIdx getColumnIndexFromName(const QString & colName, bool showWarning = true) const;
    QString getNameFromColumnIndex(colIdx index) const;


    enum DeepCopyType {
         DeepCopyMatchSource = 0
        ,DeepCopyPreserveDest_slow  //this only preserves root names and not the content, which will be invalid values (because dest is cleared)
        ,DeepCopyPreserveDestAppendSource_slow  //this will contain names from both source and dest, but the dest content will be invalid (because dest is cleared)
    };

    RowNode() {
        m_parentPtr = NULL;
    }

    /*!
     * @brief Copy constructor does deep copy.
     */
    RowNode(const RowNode & obj);

    /*!
     * @brief Assignment operator does deep copy.
     */
    RowNode& operator= (const RowNode &cSource);


    /*!
     * \brief Note that if this RowNode is inserted into a different RowNode (via addChild()), the second xparentPtr is not necceary to enter as this function will change it appropriately.
     * \param xRowData content for this rownode
     * \param xparentPtr pass the parent pointer
     */
    explicit RowNode(const VariantItems & xRowData, RowNode * xparentPtr = NULL) {
        m_parentPtr = NULL;
        setup(xRowData, xparentPtr);
    }

    virtual ~RowNode() {
        clear();
    }

    /*!
     * \brief This is used to clear the list of RowNode pointers managed by TreeTable.  This
     * function needs to be called anywhere the RowNode tree is modified with new or deletion of pointers.
     * NOTE: In the future, we should update the hash instead of completely clearing it out.
     */
    virtual void clearRowNodePointerHash() {
    }

    /*!
     * \brief getChildPointerListReference get member variable holder child pointers; only use this if you know what you are doing.
     * NOTE: Avoid using this as this makes it difficult to manage the protected member variable.  Yong will likely remove this function
     * with a more protected version.
     * NOTE: E.g. of potential problem:  the caller deletes / adds the list.  This would make the TreeTable::m_rownode_hash invalid, causing problems in the UI level.
     * \return reference to the child list
     */
    QList<RowNode*> & getChildPointerListReference() {
        return m_childPtrList;
    }

    const QList<RowNode*> & getChildPointerListReference() const {
        return m_childPtrList;
    }

    virtual void clear() {
        clearChildren();
        m_rowData.clear();
        m_parentPtr = NULL;
        //called by clearChilren; _clearRowNodePointerHashRoot();
    }

    virtual void clearChildren() {
        for (int i = 0; i < m_childPtrList.size(); i++) {
            delete m_childPtrList[i];
        }
        m_childPtrList.clear();
        _clearRowNodePointerHashRoot();
    }

    void setup(const VariantItems & xRowData, RowNode * xparentPtr) {
        clear();
        m_rowData = xRowData;
        m_parentPtr = xparentPtr;
    }

    void setParent(RowNode * parent) {
        m_parentPtr = parent;
        _clearRowNodePointerHashRoot();
    }

    void addData(const VariantItems & dataList) {
        foreach(QVariant v, dataList){
            addValue(v);
        }
    }

    /*!
     * \brief This adds QStringList individually.  This function is useful
     *  as without this, addValue(const QVariant&) would be called instead,
     *  which would add QStringList as a QVariant
     * \param dataList
     * \param trim
     */
    void addData(const QStringList & dataList, bool trim = true) {
        foreach(const QString & v, dataList){
            if (trim) {
                addValue(v.trimmed());
            } else {
                addValue(v);
            }
        }
    }

    /*!
     * \brief addValue adds any QVariant as a single item.  Note that
     *  this may cause problems if adding a list that's considered a QVariant,
     * such as QMap, QHash, etc...
     * \param data
     */
    void addValue(const QVariant & data) {
        m_rowData.push_back(data);
    }


    /*!
     * \brief This will add a new data (e.g. column name) to the root data.  It will also extend all children data by one.
     * However, if the given name already exists, it will not be added.
     * \param rootColName new data to add
     * \param avoidDuplicate if false, will add the given @rootColName even if that value already exists.
     */
    void addDataToRootAndResizeChildren(const QString & rootColName, const QVariant & defaultVal, bool avoidDuplicate = true);

    void prependDataToRootAndResizeChildren(const QString & colName, const QVariant & defaultVal, bool avoidDuplicate = true);

    int getRowSize() const {
        return m_rowData.size();
    }

    /*!
     * \brief Returns \c true if node has at least one child.
     */
    bool isGroupNode() const {
        return m_childPtrList.size() > 0;
    }

    QVariant & getDataAt(colIdx index) {
        static QVariant obj;
        if (index < 0 || index >= m_rowData.size()) {
            std::cerr << "warning index out of bounds = " << index << std::endl;
            obj = QVariant();
            return obj;
        }
        return m_rowData[index];
    }

    const QVariant & getDataAt(colIdx index) const {
        static QVariant obj;
        if (index < 0 || index >= m_rowData.size()) {
            std::cerr << "warning index out of bounds = " << index << std::endl;
            obj = QVariant();
            return obj;
        }
        return m_rowData[index];
    }

    const VariantItems & getRowData() const {
        return m_rowData;
    }

    //TODO: remove this in future.  Use setRowData instead.
    VariantItems & getRowData() {
        return m_rowData;
    }

    //Note: use this instead of VariantItems & getRowData()
    void setRowData(const VariantItems & data) {
        m_rowData = data;
    }

    QList<QString> getRowDataToStringList() const {
        QList<QString> list;
        foreach(const QVariant &obj, m_rowData)
            list.push_back(obj.toString());

        return list;
    }

    QList<QString> getRootDataAsStringList() const {
        return getRoot()->getRowDataToStringList();
    }

    //reanme to childSize()
    int getChildListSize() const {
        return m_childPtrList.size();
    }

    void addChild(RowNode * child, bool toBack = true) {
        child->setParent(this);
        if (toBack)
            m_childPtrList.push_back(child);
        else
            m_childPtrList.push_front(child);
        _clearRowNodePointerHashRoot();
    }

    /*!
     * \brief addNewChildMatchHeaderSize add a new child and its data has the same size as the header.
     * \return the new added child RowNode * ptr
     */
    RowNode * addNewChildMatchRootDataSize() {
        RowNode * child = new RowNode;
        child->setParent(this);
        int len = getRoot()->getRowData().size();
        VariantItems data;
        data.resize(len);
        child->addData(data);
        m_childPtrList.push_back(child);
        _clearRowNodePointerHashRoot();
        return child;
    }

    /*!
     * \brief removes the child from the internal list, but does not delete
     * \param child to be removed, but not deleted
     * \return the child again
     */
    static RowNode* removeRowNode(RowNode* child)
    {
        RowNode* parent = child->parent();

        //is root, nothing to remove child from..
        if(!parent)
            return child;

        for(QList<RowNode*>::iterator itr = parent->m_childPtrList.begin();
            itr != parent->m_childPtrList.end();
            ++itr)
        {
            if(*itr == child)
            {
                itr = parent->m_childPtrList.erase(itr); //note: erase(itr) does not call destructor
                parent->_clearRowNodePointerHashRoot();
                break;
            }
        }
        return child;
    }

    void deleteChild(int index)
    {
        if (index >= 0 && index < m_childPtrList.size())
            deleteRowNode(m_childPtrList[index]);

        //Note: use this, if this calls ~RowNode()
        //m_childPtrList.removeAt(index);
    }

    static void deleteRowNode(RowNode* child)
    {
        if(child)
        {
            //_clearRowNodePointerHashRoot() not needed as removeRowNode() calls it.
            removeRowNode(child);
            child->setParent(NULL);
            child->clear();
            delete child;
        }
    }

    //rename to child(), others as well
    RowNode* getChild(int index) const {
        if (index < 0 || index >= m_childPtrList.size())
            return NULL;
        return m_childPtrList[index];
    }

    RowNode* getChildFirst() const {
        return m_childPtrList.isEmpty() ? nullptr : m_childPtrList.first();
    }

    RowNode* getChildLast() const {
        if (m_childPtrList.size() <= 0)
            return NULL;
        return m_childPtrList.last();
    }

    RowNode* parent() const {
        return m_parentPtr;
    }

    int getChildIndex(RowNode *child) const {
        return m_childPtrList.indexOf(child);
    }

    /*!
     * @brief Positional index of the node among siblings
     *
     * @return the position index of the RowNode in the parent's list of children, -1 if node does
     * not have parent
     */
    int indexAsChildFromParent() const
    {
        if (m_parentPtr) {
            // Error: cannot convert argument 1 from 'const RowNode *const ' to 'RowNode *const &'
            // * m_childPtrList contains non-const RowNode <RowNode *> 
            // * "this" is const pointer to const RowNode (const RowNode *const) 
            // indexOf(const T &t,...) expects RowNode *const &
            // so we need to cast away one const:
            return m_parentPtr->m_childPtrList.indexOf(const_cast<RowNode *const>(this));
        }
        return -1;
    }

    //rename this to rootDataContains
    bool rootHasData(const QString & columnName) const {
        const RowNode * root = this->getRoot();
        if (root) {
            return root->m_rowData.contains(columnName);
        }
        return false;
    }

    static QList<int> rowPath(const RowNode* node)
    {
        QList<int> path;

        while (node) {
            RowNode* parent = node->parent();
            if (!parent) {
                break;
            }
            for (int i = 0; i < parent->getChildListSize(); ++i) {
                if (parent->getChild(i) == node) {
                    path.push_front(i);
                    break;
                }
            }
            node = parent;
        }

        return path;
    }

    /*!
     * \brief get parent until the given level
     * \param level level 0 means root, 1 means child from root, etc...
     * \return parent until the given level
     */
    RowNode * parentUntilLevel(int level) {
        QList<int> path = rowPath(this);
        RowNode * parent_node = this;
        for (int i = path.size(); i > level; i--) {
            if (parent_node == NULL) {
                std::cerr << "Warning, already at root. Can't get parent of NULL" << std::endl;
                break;
            }
            parent_node = parent_node->parent();
        }
        return parent_node;
    }

    //! Get the top most row path
    int topRowPath() {
        QList<int> list = rowPath(this);
        if (list.size() <= 0)
            return -1;
        return list[0];
    }

    static QModelIndex indexFromRow(const QAbstractItemModel* model, const QModelIndex &rootIndex, colIdx column, const RowNode* row)
    {
        QList<int> rowList = rowPath(row);

        QModelIndex parent = rootIndex;
        for(int i = 0; i < rowList.size(); ++i)
            parent = model->index(rowList[i], column, parent);

        return parent;
    }

    static QModelIndex indexFromRow(const QAbstractItemModel* model, const QModelIndex &rootIndex, colIdx column, const QList<int>& rowList)
    {
        QModelIndex parent = rootIndex;
        for(int i = 0; i < rowList.size(); ++i)
            parent = model->index(rowList[i], column, parent);

        return parent;
    }

    void print(const QString & filename) const;

    void print(std::ostream & os, int w=0, const QString & seperator=QString("|"), bool showDash=true) const ;

    /*!
     * @brief Searches for the given val for the given index at current node, and also searches through all children, recursively.
     *
     * @param index column index
     * @param val search value
     * @return bool if the value exists
     */
    RowNode * hasSearchRecursive(colIdx index, const QVariant & val) ;

    /*!
     * @brief Searches for the given val and val2 for the given indices at current node, and also searches through all children, recursively.
     *
     * @param index
     * @param val
     * @param index2
     * @param val2
     * @return RowNode *
     */
    RowNode * hasSearchRecursive2(colIdx index, const QVariant & val, colIdx index2, const QVariant & val2) ;

    QString getRowNumberAsString() const;

    /*!
     * @brief Find the root from this node.
     * @param rowLevel tells the level of this RowNode. E.g. root rownode will have level be 0, a child of root will be 1.
     *
     * @return RowNode *  the root node (That is, the node with parentPtr == NULL).
     */
    const RowNode * getRoot(int * rowLevel = NULL) const
    {
        const RowNode * root = this;
        if (rowLevel)
            *rowLevel = 0;

        while (root->m_parentPtr != NULL) {
            root = root->m_parentPtr;
            if (rowLevel)
                *rowLevel += 1;
        }

        return root;
    }

    //Note: getRoot() always returns a valid pointer
    RowNode * getRoot(int * rowLevel = NULL)
    {
        RowNode * root = this;
        if (rowLevel)
            *rowLevel = 0;

        while (root->m_parentPtr != NULL) {
            root = root->m_parentPtr;
            if (rowLevel)
                *rowLevel += 1;
        }

        return root;
    }

    //! get tree level; how far am I from the root? Root is zero.
    int getTreeLevel() const {
        int rowLevel = 0;
        getRoot(&rowLevel);
        return rowLevel;
    }

    /*!
     * \brief isRoot checks to see if this is the root node
     * \return true if root
     */
    bool isRoot() const
    {
        const RowNode * root = getRoot();
        return root == this;
    }


    /*!
     * @brief Finds the value in the rowData. E.g. called on the root level
     * !! NOTE: You probably want to use getRootDataIndexOf(), which returns the column names.
     * TODO: make this private
     *
     * @param value or name of rowData entry; fieldName, e.g. kPeptidesId
     * @return int returns colIdx or -1 if not found
     */
    colIdx getDataIndexOf(const QString & value, bool showWarning = true) const {
        colIdx cindex = getColumnIndexFromName(value, showWarning);
        if (showWarning) {
            if (cindex < 0) {
                std::cout << "getDataIndexOf() returns cindex = " << cindex << std::endl;
                std::cout << "input value = " << value.toStdString() << std::endl;
            }
            if (!this->isRoot()) {
                std::cout << "warning, getDataIndexOf(string) is meant to be called by the root node. This is not a root node" << std::endl;
            }
        }
        return cindex;
    }

    /*!
     * @brief Finds the value in the rowData FROM THE root node.
     *
     * @param fieldName, e.g. kPeptidesId
     * @return int returns index or -1 if not found
     *
     */
    colIdx getRootDataIndexOf(const QString & fieldName) const {
        return getColumnIndexFromName(fieldName);
    }

    const QVariant & getRootDataAt(const colIdx idx) {
        const RowNode * root = getRoot();
        return root->getDataAt(idx);
    }

    /*!
     * @brief This will destroy all parent/child relationships.
     * Warning: Only use this if you know how to free their memory.
     * @brief call_level should be zero by the caller; just don't pass in any argument
     */
    void delinkChildrenRecursive(int call_level = 0);


    /*!
     * @brief Convience function.  Instead of using getDataAt(), this will find the proper index first.
     * This searches the rowData of the root RowNode for the fieldName (and normally children with the data entries).
     * Only use this function is the RowNode is filled with proper field names.
     *
     * @param fieldName  e.g. kPeptidesId
     * @param ok if an address of the bool is provide, it will fill it as either true or false; true of QVariant & is found.
     * @return QVariant &  returns the object.  If not found, it will return a dummy object;
     *
     */
    QVariant & getDataAt(const QString & fieldName, bool *ok = NULL, bool ignoreRootWarning = false);

    /*!
     * @brief Convience function.  Instead of using getDataAt(), this will find the proper index first.
     * This searches the rowData of the root RowNode for the fieldName (and normally children with the data entries).
     * Only use this function is the RowNode is filled with proper field names.
     *
     * @param fieldName  e.g. kPeptidesId
     * @param ok if an address of the bool is provide, it will fill it as either true or false; true of QVariant & is found.
     * @return QVariant &  returns the object.  If not found, it will return a dummy object;
     *
     */
    const QVariant & getDataAt(const QString & fieldName, bool *ok = NULL, bool ignoreRootWarning = false) const;

    /*!
     * @brief the data from one RowNode to another
     * @param src The source from which to copy
     */
    void deepCopy(const RowNode* src, DeepCopyType type = DeepCopyMatchSource);

    /*!
     * \brief copies the src row node and also copies the header information at the root node. Read more...
     *        Note that the word content is used to describe the children of RowNode with root with column headers. Maybe table or tree is better term.
     *
     *  Keep in mind that the returned row will be the root node (the header) and the child will be the content of the @src.  E.g.
     *  The @src with value of (1,"PEPTIDE",32.0) with root/parent value of ("Flag","Peptide","Score"), and headersOfInterest is ("Peptide","Score").
     *  Then the new row will have the following structure:
     *  this.m_rowData = ("Peptide","Score") and first child's m_rowData = ("PEPTIDE",32.0).
     *  If the @src is root, then only the root header gets copied.
     *
     *  Warning: only tested/works if this the root node. If this is a child node, it will not work properly.
     *
     * \param src the row to copy (e.g. (1,PEPTIDE,32.0) )
     * \param headersOfInterest the headers to copy. E.g. kPeptide, kScore
     * \param recursive if the current row has children, this will copy them if true.
     * \return error flag
     */
    pmi::Err copyContentWithHeaderClearFirst(const RowNode* src, const QStringList & headersOfInterest, bool recursive);

    /*!
     * @brief Recursively sets a column data except the root node, which would contain the header data.
     *        Note that the word content is used to describe the children of RowNode with root with column headers.
     * @param header the column to change
     * @param value the value to assign
     * @param recursive to change @dest children(s) recurisvely
     * @return error flag
     */
    pmi::Err setContentValuesExceptHeader(const QString & header, const QVariant & value, bool recursive);

    /*!
     * \brief setContentValuesExceptHeader Recursively sets a column data except the root node (which would contain the header data).
     * \param headerList column(s) to change
     * \param valueList value(s) to assign
     * \param recursive to change @dest children(s) recurisvely
     * \return error flag
     */
    pmi::Err setContentValuesExceptHeader(const QStringList & headerList, const VariantItems & valueList, bool recursive);

    /*!
     * \brief getRowNodeListMatchingContent will find all matching RowNode * base on @content
     * \param content is expected to the content structure (root with header and one child) with the contraint.
     * \param outList collected output.
     * \return error if content structure is invalid or if the input content's columns do not exists
     */
    pmi::Err getRowNodeListMatchingContent(const RowNode & content, bool recursive, QList<RowNode*> & outList);

    /*!
     * \brief clearAndCopyHeaderAndRowList Clears and copies the given rows recursively; root header also copied.
     * \param rows expects the input list containing the same header root
     * \return
     */
    pmi::Err clearAndCopyHeaderAndRowList(const QList<RowNode*> & rows);

    /*!
     * Read to QDataStream object
     */
    pmi::Err readFromQDataStream(QDataStream& in);
    /*!
     * Write RowNode to QDataStream object
     */
    void writeToQDataStream(QDataStream& out) const;
    /*!
     * Read RowNode to Xml object
     */
    void readFromXml(QDomDocument* doc, QDomElement* root);
    /*!
     * Write RowNode to Xml object
     */
    void writeToXml(QDomDocument* doc, QDomElement* root);

    void _print_data(std::ostream & os, int w=0, const QString & seperator=QString("|"), bool showDash=true) const;

    /*!
     * \brief We want to call the virtual function on the root, which might be TreeTable class, while all its children are
     * plain RowNode class.  This needs to be called anything the RowNode pointer is added/deleted.
     */
    void _clearRowNodePointerHashRoot() {
        RowNode * root = getRoot();
        root->clearRowNodePointerHash();
    }

    enum ApplyFunction {
        ApplyFunction_ThisNode_UnlessRoot,
        ApplyFunction_ThisNode_IncludeRoot,
        ApplyFunction_SkipThis
    };

    void applyFunction(pmi::addnode f, void * data = NULL, std::vector<RowNode*> * list= NULL, ApplyFunction type=ApplyFunction_ThisNode_UnlessRoot);

    bool removeColumn(const QString & colName);

    void collectRowNodeListWith(const QString & colName, int value, std::vector<RowNode*> & list);

private:

    void _print_this_with_root(std::ostream & os, const QString & seperator, bool showDash) const;

    /*!
     * Write to QDataStream
     */
    void writeToQDataStream(QDataStream& out, const RowNode* row) const;
    /*!
     * Read to QDataStream
     */
    pmi::Err readFromQDataStream(QDataStream& in,RowNode* row);
    /*!
     * Write to Xml
     */
    void writeToXml(QDomDocument* doc, QDomElement* root,RowNode* row);
    /*!
     * Read to Xml
     */
    void readFromXml(QDomDocument* doc, QDomElement* root,RowNode* row);
};

/*!
 * @brief Convience function.  Instead of using getDataAt(), this will find the proper index first.
 * This searches the rowData of the root RowNode for the fieldName (and normally children with the data entries).
 * Only use this function is the RowNode is filled with proper field names.
 *
 * @param fieldName name to search for.
 * @param ok will assign to true if proper value is found; false otherwise.
 * @return QVariant returns copy of the value, if not found, an empty QVariant() returned.
 */
PMI_COMMON_CORE_MINI_EXPORT QVariant getDataWithFieldName(const RowNode * ths, const QString & fieldName, bool *ok = NULL) ;

PMI_COMMON_CORE_MINI_EXPORT pmi::pepId getPID(const RowNode * node);
PMI_COMMON_CORE_MINI_EXPORT pmi::pepId getPIDFromVar(const QVariant & val);

namespace pmi {

/*!
 * Feel free to edit.
 * \brief The RowNodeUpdate struct is used to handle refreshing of tree table views. Each of these elements
 * define a {row+column} edit.
 * Note: For a delete event, the row may not exists.  In this case, the whole tree view
 * would be refreshed.
 * Note: Currently, we use row's root to decide if the whole tree view should be refreshed.  Later, we can make this
 * refresh more specific to the row&column locations.
 */
struct PMI_COMMON_CORE_MINI_EXPORT RowNodeUpdate
{
    RowNode* row;
    QString columnName;
    RowNodeUpdate() {
        row = NULL;
    }

    RowNodeUpdate(RowNode * r, QString cn) {
        row = r;
        columnName = cn;
    }
};


typedef QMap<QString,QString> qmap_strstr;
typedef QMap<colIdx, qmap_strstr > qmap_idx_qmap_strstr;
typedef QMap<QString, qmap_strstr > qmap_str_qmap_strstr;

class PMI_COMMON_CORE_MINI_EXPORT RowNodeWriteParameter
{
public:
    enum WriteLocation {CSVStream=0,DatabaseInsertForUser,DatabaseInsertForPivotInput};
    RowNodeWriteParameter(QTextStream & s, bool doNumbering,
                          const qmap_strstr * rootNameFriendly,
                          const RowNode * rownode,
                          const qmap_str_qmap_strstr * colValuesFriendly,
                          QSqlQuery * q);

    QString columnRootName(const QString & oldRootName) {
        if (m_rootColumnNameFriendly) {
            if (m_rootColumnNameFriendly->contains(oldRootName)) {
                return m_rootColumnNameFriendly->value(oldRootName);
            }
        }
        return oldRootName;
    }

    QString columnValueName(colIdx idx, const QString & inputValue) {
        if (m_columnValueFriendly.contains(idx)) {
            const qmap_strstr mapper = m_columnValueFriendly[idx];  //NOTE: be careful about using reference
            if (mapper.contains(inputValue)) {
                return mapper.value(inputValue);
            }
        }
        return inputValue;
    }

    QTextStream & getStream() {
        return m_stream;
    }

    bool showNumbering() const {
        return m_numbering;
    }

    QSqlQuery * query() {
        return m_q;
    }

    WriteLocation writeLocation() const {
        return m_writeLocation;
    }
    void setWriteLocation(WriteLocation write_loc) {
        m_writeLocation = write_loc;
    }

private:
    QTextStream & m_stream;
    bool m_numbering;
    const qmap_strstr * m_rootColumnNameFriendly;
    qmap_idx_qmap_strstr m_columnValueFriendly;
    QSqlQuery * m_q;
    WriteLocation m_writeLocation;
};

typedef QList<RowNodeUpdate> RowNodeUpdateList;

enum RowNodeLevel
{
    AllDescendantNodes,
    DirectChildNodes,
    LeafNodes
};

typedef QList<RowNode*> RowNodePtrList;

PMI_COMMON_CORE_MINI_EXPORT void getLeafRowNodeList(const RowNode & content, RowNodePtrList & rowList, RowNode::ApplyFunction type = RowNode::ApplyFunction_SkipThis);
PMI_COMMON_CORE_MINI_EXPORT void getLeafRowNodeListWithColumn(const RowNode & content, QList<RowNode*> & rowList, QString colName, QVariant value);
/*!
 * \brief get all children rownodes except the root
 * \param input rownode
 * \param rowList output list
 */
PMI_COMMON_CORE_MINI_EXPORT void getAllRowNodeList(const RowNode & input, QList<RowNode*> & rowList);

PMI_COMMON_CORE_MINI_EXPORT QString writeToFile(QString csvFileName, const RowNode & content, RowNodeLevel group, bool numbering, const RowNode * columnInfo);
PMI_COMMON_CORE_MINI_EXPORT void writeToCSV(QTextStream & csvStream, const RowNode & content, RowNodeLevel group, bool numbering = true, qmap_strstr * colNameMap=nullptr, qmap_str_qmap_strstr * colValueMap=nullptr, QSqlDatabase * db = nullptr, const RowNode * columnInfo = nullptr);

//Note:
//Our current CSV parser doesn't like empty quotes.
//E.g. KSTSGGTAALGCLVK,1448.76567900323,"","","84.9543","88.9543","3"
//The 3rd and 4th gets parsed  as ", and ", instead of empty string.
PMI_COMMON_CORE_MINI_EXPORT bool readFromCSV(QString csvFileName, RowNode* rootNode);

PMI_COMMON_CORE_MINI_EXPORT void copyDataToChildren(RowNode * parent, const QStringList & columnNames);


inline QStringList toStringList(const VariantItems & vlist) {
    QStringList slist;
    foreach(const QVariant & v, vlist) {
        slist.push_back(v.toString());
    }
    return slist;
}

PMI_COMMON_CORE_MINI_EXPORT extern QVariant g_dummy_variant;

} //pmi

#endif
