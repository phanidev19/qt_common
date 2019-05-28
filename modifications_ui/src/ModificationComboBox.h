/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef MODIFICATIONCOMBOBOX_H
#define MODIFICATIONCOMBOBOX_H

#include <QComboBox>
#include <QItemDelegate>

class ModificationComboBoxDelegate : public QItemDelegate
{
    Q_OBJECT

public:
    ModificationComboBoxDelegate(QObject *parent = nullptr);
    virtual ~ModificationComboBoxDelegate();

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option,
                       const QModelIndex &index) const override;
};

class ModificationComboBox : public QComboBox
{
    Q_OBJECT

public:
    ModificationComboBox(QWidget *parent = nullptr);
    virtual ~ModificationComboBox();

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

private:
    static int s_modificationsSizeHintHeightCache;
    static int s_modificationsSizeHintWidthCache;
};

#endif // MODIFICATIONCOMBOBOX_H
