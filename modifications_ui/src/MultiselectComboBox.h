/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef MULTISELECTCOMBOBOX_H
#define MULTISELECTCOMBOBOX_H

#include <QComboBox>
#include <QItemDelegate>

class MultiselectComboBoxDelegate : public QItemDelegate
{
    Q_OBJECT

public:
    MultiselectComboBoxDelegate(QObject *parent = nullptr);
    virtual ~MultiselectComboBoxDelegate();

protected:
    virtual bool editorEvent(QEvent *event, QAbstractItemModel *model,
                             const QStyleOptionViewItem &option, const QModelIndex &index) override;
};

class MultiselectComboBox : public QComboBox
{
    Q_OBJECT

public:
    MultiselectComboBox(QWidget *parent = nullptr);
    virtual ~MultiselectComboBox();

protected:
    virtual void paintEvent(QPaintEvent *event) override;
};

class MultiselectComboBoxEventFilter : public QObject
{
    Q_OBJECT

public:
    MultiselectComboBoxEventFilter(MultiselectComboBox *comboBox = nullptr);
    virtual ~MultiselectComboBoxEventFilter();

protected:
    virtual bool eventFilter(QObject *o, QEvent *e) override;

private:
    MultiselectComboBox *m_comboBox = nullptr;
};

#endif // MULTISELECTCOMBOBOX_H
