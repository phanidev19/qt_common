/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#ifndef MSXICDATADIALOG_H
#define MSXICDATADIALOG_H

#include "MSReaderWidget.h"
#include <QDialog>

class QGroupBox;
class QLineEdit;
class QGridLayout;

namespace pmi
{

class PMI_COMMON_WIDGETS_EXPORT MSXICDataDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MSXICDataDialog(QWidget *parent = nullptr);
    ~MSXICDataDialog();

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
private slots:
    void handleGoButtonClicked();

private:
    QLineEdit *createInputSet(QGridLayout *gridLayout, QString title, int row);

    QwtPlot *m_plot;
    QLineEdit *m_mzStart;
    QLineEdit *m_mzEnd;
    QLineEdit *m_timeStart;
    QLineEdit *m_timeEnd;
    QGroupBox *m_inputParameterGroupBox;
    QPushButton *m_goButton;
    QwtPlotCurve *m_curve;
    QwtPlotZoomer *m_zoomer;
};
}
#endif // MSXICDATADIALOG_H
