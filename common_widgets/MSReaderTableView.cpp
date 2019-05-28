/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#include "MSReaderTableView.h"

#include <QMouseEvent>

MSReaderTableView::MSReaderTableView(QWidget *parent)
    : QTableView(parent)
{
}

void MSReaderTableView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        return;
    }
    QTableView::mousePressEvent(event);
}
