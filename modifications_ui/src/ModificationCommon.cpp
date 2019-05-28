/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "ModificationCommon.h"

#include <QComboBox>
#include <QGridLayout>

struct GridItemPosition {
    int row = 0;
    int col = 0;
    int rowSpan = 1;
    int colSpan = 1;
};

void insertRow(QGridLayout *layout, int rowIndex)
{
    Q_ASSERT(layout);

    if (rowIndex < 0 || rowIndex > layout->rowCount()) {
        return;
    }
    QVector<QPair<QLayoutItem *, GridItemPosition>> items;
    while (layout->count() > 0) {
        GridItemPosition pos;
        layout->getItemPosition(0, &pos.row, &pos.col, &pos.rowSpan, &pos.colSpan);
        items.push_back(qMakePair(layout->takeAt(0), pos));
    }

    for (auto item : items) {
        if (item.second.row + item.second.rowSpan - 1 < rowIndex) {
            layout->addItem(item.first, item.second.row, item.second.col, item.second.rowSpan,
                            item.second.colSpan);
            continue;
        }
        if (item.second.row >= rowIndex) {
            layout->addItem(item.first, item.second.row + 1, item.second.col, item.second.rowSpan,
                            item.second.colSpan);
            continue;
        }
        layout->addItem(item.first, item.second.row, item.second.col, item.second.rowSpan + 1,
                        item.second.colSpan);
    }
}

void removeRow(QGridLayout *layout, int rowIndex)
{
    Q_ASSERT(layout);
    if (rowIndex < 0 || rowIndex >= layout->rowCount()) {
        return;
    }
    QVector<QPair<QLayoutItem *, GridItemPosition>> items;
    items.reserve(layout->count());
    while (layout->count() > 0) {
        GridItemPosition pos;
        layout->getItemPosition(0, &pos.row, &pos.col, &pos.rowSpan, &pos.colSpan);
        items.push_back(qMakePair(layout->takeAt(0), pos));
    }

    for (auto item : items) {
        if (item.second.row + item.second.rowSpan - 1 < rowIndex) {
            layout->addItem(item.first, item.second.row, item.second.col, item.second.rowSpan,
                            item.second.colSpan);
            continue;
        }
        if (item.second.row > rowIndex) {
            layout->addItem(item.first, item.second.row - 1, item.second.col, item.second.rowSpan,
                            item.second.colSpan);
            continue;
        }
        if (item.second.row == rowIndex && item.second.rowSpan == 1) {
            continue;
        }
        layout->addItem(item.first, item.second.row, item.second.col, item.second.rowSpan - 1,
                        item.second.colSpan);
    }
}

bool setCurrentComboBoxIndexByRootIndex(QComboBox *comboBox, Qt::ItemDataRole role)
{
    QAbstractItemModel *model = comboBox->model();
    const QModelIndex rootIndex = comboBox->rootModelIndex();
    const QVariant rootData = model->data(rootIndex, role);
    const int rowCount = model->rowCount(rootIndex);
    for (int i = 0; i < rowCount; ++i) {
        const QModelIndex itemIndex = model->index(i, 0, rootIndex);
        const QVariant itemData = model->data(itemIndex, role);
        if (!itemData.isNull() && itemData.type() == rootData.type() && itemData == rootData) {
            comboBox->setCurrentIndex(i);
            return true;
        }
    }
    return false;
}
