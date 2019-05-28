/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "ModificationComboBox.h"

#include "ModificationInfoModel.h"

#include <QAbstractItemView>
#include <QItemDelegate>

static const int MODIFICATION_COMBOBOX_MIN_SYMBOLS = 40;

int ModificationComboBox::s_modificationsSizeHintHeightCache = -1;
int ModificationComboBox::s_modificationsSizeHintWidthCache = -1;

ModificationComboBoxDelegate::ModificationComboBoxDelegate(QObject *parent)
    : QItemDelegate(parent)
{
}

ModificationComboBoxDelegate::~ModificationComboBoxDelegate()
{
}

void ModificationComboBoxDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                         const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    if (index.data(Qt::UserRole).toInt() == -1) {
        opt.font.setBold(true);
    } else {
        opt.rect.adjust(opt.fontMetrics.averageCharWidth(), 0, 0, 0);
    }
    QItemDelegate::paint(painter, opt, index);
}

ModificationComboBox::ModificationComboBox(QWidget *parent)
    : QComboBox(parent)
{
    setItemDelegate(new ModificationComboBoxDelegate(this));
    setMinimumWidth(fontMetrics().averageCharWidth() * MODIFICATION_COMBOBOX_MIN_SYMBOLS);
    setSizeAdjustPolicy(AdjustToMinimumContentsLengthWithIcon);
}

ModificationComboBox::~ModificationComboBox()
{
}

QSize ModificationComboBox::sizeHint() const
{
    const ModificationInfoModel *modModel = qobject_cast<const ModificationInfoModel *>(model());
    if (modModel == nullptr) {
        return QComboBox::sizeHint();
    }
    // optimization. Full calculation of combobox size takes about 3 seconds
    if (s_modificationsSizeHintWidthCache < 0) {
        const QModelIndex parentIndex = modModel->modificationParentIndex();
        const int rows = modModel->rowCount(parentIndex);
        QString longestString;
        for (int i = 0; i < rows; ++i) {
            const QString currentString
                = modModel->data(modModel->index(i, 0, parentIndex), Qt::DisplayRole).toString();
            if (longestString.length() < currentString.length()) {
                longestString = currentString;
            }
        }
        longestString += 5; // length of "(De) "
        s_modificationsSizeHintWidthCache
            = static_cast<int>(fontMetrics().averageCharWidth() * longestString.length());
    }
    return QSize(s_modificationsSizeHintWidthCache, s_modificationsSizeHintHeightCache);
}

QSize ModificationComboBox::minimumSizeHint() const
{
    return QComboBox::minimumSizeHint();
}
