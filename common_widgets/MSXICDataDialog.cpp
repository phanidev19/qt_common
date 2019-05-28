/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#include "MSXICDataDialog.h"
#include "MSReader.h"
#include <qwt_plot_curve.h>
#include <qwt_plot_zoomer.h>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegExpValidator>

namespace
{
const int LEFT_OFFSET = 10;
const int TOP_OFFSET = 10;
const int GROUPBOX_WIDTH = 400;
const int GROUPBOX_HEIGHT = 200;

const int DIALOG_MINIMUM_WIDTH = 600;
const int DIALOG_MINIMUM_HEIGHT = 400;
}

using namespace pmi;

MSXICDataDialog::MSXICDataDialog(QWidget *parent)
    : QDialog(parent)
    , m_plot(new QwtPlot(this))
    , m_curve(new QwtPlotCurve("Time/Intensity"))
{
    m_inputParameterGroupBox = new QGroupBox(tr("Input data"), this);
    m_inputParameterGroupBox->setGeometry(LEFT_OFFSET, TOP_OFFSET, GROUPBOX_WIDTH, GROUPBOX_HEIGHT);

    QGridLayout *gridLayout = new QGridLayout(this);
    gridLayout->setSpacing(1);
    m_inputParameterGroupBox->setLayout(gridLayout);

    m_mzStart = createInputSet(gridLayout, tr("mz Start:"), 0);
    m_mzEnd = createInputSet(gridLayout, tr("mz End:"), 1);
    m_timeStart = createInputSet(gridLayout, tr("Start time:"), 2);
    m_timeEnd = createInputSet(gridLayout, tr("End time:"), 3);

    m_goButton = new QPushButton(tr("Go"), m_inputParameterGroupBox);
    gridLayout->addWidget(m_goButton, 4, 0);

    connect(m_goButton, &QPushButton::clicked, this, &MSXICDataDialog::handleGoButtonClicked);

    m_plot->setAxisTitle(QwtPlot::Axis::yLeft, tr("Intensity"));
    m_plot->setAxisTitle(QwtPlot::Axis::xBottom, tr("Time"));
    m_plot->setAxisAutoScale(QwtPlot::Axis::yLeft);
    m_plot->setAxisAutoScale(QwtPlot::Axis::xBottom);

    m_zoomer = new QwtPlotZoomer(m_plot->canvas());
    m_zoomer->setRubberBandPen(QColor(Qt::black));
    m_zoomer->setTrackerPen(QColor(Qt::black));
    m_zoomer->setMousePattern(QwtEventPattern::MouseSelect2, Qt::RightButton, Qt::ControlModifier);
    m_zoomer->setMousePattern(QwtEventPattern::MouseSelect3, Qt::RightButton);

    m_curve->attach(m_plot);

    setMinimumWidth(DIALOG_MINIMUM_WIDTH);
    setMinimumHeight(DIALOG_MINIMUM_HEIGHT);
}

MSXICDataDialog::~MSXICDataDialog()
{
    m_curve->detach();
    delete m_curve;
}

void MSXICDataDialog::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    m_plot->setGeometry(LEFT_OFFSET, m_inputParameterGroupBox->geometry().bottom() + TOP_OFFSET,
                        this->width() - (LEFT_OFFSET * 2),
                        this->height() - m_inputParameterGroupBox->height() - (TOP_OFFSET * 3));
}

QLineEdit *MSXICDataDialog::createInputSet(QGridLayout *gridLayout, QString title, int row)
{
    QLabel *label = new QLabel(title, m_inputParameterGroupBox);

    QRegExp regulerExpression("(|\\.|[0-9]){30}");
    QLineEdit *lineEdit = new QLineEdit(m_inputParameterGroupBox);
    lineEdit->setValidator(new QRegExpValidator(regulerExpression, this));

    gridLayout->addWidget(label, row, 0);
    gridLayout->addWidget(lineEdit, row, 1);

    return lineEdit;
}

void MSXICDataDialog::handleGoButtonClicked()
{
    QString mzStartText(m_mzStart->text().trimmed());
    QString mzEndText(m_mzEnd->text().trimmed());
    QString startTimeText(m_timeStart->text().trimmed());
    QString endTimeText(m_timeEnd->text().trimmed());

    if (mzStartText.isEmpty() || mzEndText.isEmpty() || startTimeText.isEmpty()
        || endTimeText.isEmpty()) {
        QMessageBox::critical(this, tr("XIC Data"), tr("Either of the input data is empty"));
        return;
    }

    double mzStart = mzStartText.toDouble();
    double mzEnd = mzEndText.toDouble();
    double startTime = startTimeText.toDouble();
    double endTime = endTimeText.toDouble();

    msreader::XICWindow window(mzStart, mzEnd, startTime, endTime);

    point2dList points;
    Err e = MSReader::Instance()->getXICData(window, &points, 1);

    if (e != kNoErr) {
        QMessageBox::critical(this, tr("XIC Data"),
                              tr("Error reading XIC Data, Error code %1").arg(e));
        return;
    }

    if (points.empty()) {
        QMessageBox::information(this, tr("XIC Data"),
                                 tr("No data is available for selected input range"));
        return;
    }

    const QVector<QPointF> vector = QVector<QPointF>::fromStdVector(points);
    m_curve->setSamples(vector);

    // This lines are used to restore zoom effect
    m_plot->setAxisAutoScale(QwtPlot::Axis::yLeft);
    m_plot->setAxisAutoScale(QwtPlot::Axis::xBottom);
    m_plot->replot();

    // To stay in sync with plot data and zoom level, has set zoom base
    m_zoomer->setZoomBase();
}
