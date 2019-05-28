/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#ifndef MSREADERTABLEVIEW_H
#define MSREADERTABLEVIEW_H

#include <QTableView>

class MSReaderTableView : public QTableView
{
public:
    explicit MSReaderTableView(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    Q_DISABLE_COPY(MSReaderTableView)
};

#endif // MSREADERTABLEVIEW_H
