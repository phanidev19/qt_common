/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "MultiselectComboBox.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QItemDelegate>
#include <QMouseEvent>
#include <QStylePainter>

MultiselectComboBoxDelegate::MultiselectComboBoxDelegate(QObject *parent)
    : QItemDelegate(parent)
{
    ;
}

MultiselectComboBoxDelegate::~MultiselectComboBoxDelegate()
{
}

bool MultiselectComboBoxDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                              const QStyleOptionViewItem &option,
                                              const QModelIndex &index)
{
    Q_ASSERT(event);
    Q_ASSERT(model);

    // make sure that the item is checkable
    Qt::ItemFlags flags = model->flags(index);
    if (!(flags & Qt::ItemIsUserCheckable) || !(option.state & QStyle::State_Enabled)
        || !(flags & Qt::ItemIsEnabled))
        return false;

    // make sure that we have a check state
    QVariant value = index.data(Qt::CheckStateRole);
    if (!value.isValid())
        return false;

    if (event->type() == QEvent::MouseButtonPress) {
        Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
        return model->setData(index, ((state == Qt::Checked) ? Qt::Unchecked : Qt::Checked),
                              Qt::CheckStateRole);
    }

    return QItemDelegate::editorEvent(event, model, option, index);
}

MultiselectComboBox::MultiselectComboBox(QWidget *parent)
    : QComboBox(parent)
{
    setItemDelegate(new MultiselectComboBoxDelegate(this));

    MultiselectComboBoxEventFilter *eventFiter = new MultiselectComboBoxEventFilter(this);

    // Enable editing on items view
    view()->setEditTriggers(QAbstractItemView::CurrentChanged);
}

MultiselectComboBox::~MultiselectComboBox()
{
}

void MultiselectComboBox::paintEvent(QPaintEvent *)
{
    QStylePainter painter(this);
    painter.setPen(palette().color(QPalette::Text));

    // draw the combobox frame, focusrect and selected etc.
    QStyleOptionComboBox opt;
    initStyleOption(&opt);

    const QString displayText = rootModelIndex().data(Qt::DisplayRole).toString();

    // if no display text been set , use "..." as default
    if (displayText.isNull()) {
        opt.currentText = "...";
    } else {
        opt.currentText = displayText;
    }
    painter.drawComplexControl(QStyle::CC_ComboBox, opt);

    // draw the icon and text
    painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
}

MultiselectComboBoxEventFilter::MultiselectComboBoxEventFilter(MultiselectComboBox *comboBox)
    : QObject(comboBox)
    , m_comboBox(comboBox)
{
    m_comboBox->view()->viewport()->installEventFilter(this);
}

MultiselectComboBoxEventFilter::~MultiselectComboBoxEventFilter()
{
}

bool MultiselectComboBoxEventFilter::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QAbstractItemView *view = m_comboBox->view();

        QMouseEvent *m = static_cast<QMouseEvent *>(event);
        if (m_comboBox->isVisible() && view->rect().contains(m->pos())
            && view->currentIndex().isValid() && (view->currentIndex().flags() & Qt::ItemIsEnabled)
            && (view->currentIndex().flags() & Qt::ItemIsSelectable)) {
            return true;
        }
    }
    return false;
}
