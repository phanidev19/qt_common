/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#include "MSReaderWidget.h"
#include "CsvWriter.h"
#include "MSReaderTableModel.h"
#include "MSReaderTableView.h"
#include "MSXICDataDialog.h"
#include "MSChromatogramDialog.h"
#include "qwt_plot.h"
#include "qwt_plot_curve.h"
#include "qwt_plot_zoomer.h"

#include <QFileDialog>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QTableView>

namespace
{
const int LEFT_MARGIN = 10;
const int TOP_MARGIN = 10;
const int BOTTOM_MARGIN = 10;
const int RIGHT_MARGIN = 10;
}

using namespace pmi;

MSReaderWidget::MSReaderWidget(QWidget *parent)
    : QWidget(parent)
    , m_tableView(new MSReaderTableView(this))
    , m_plot(new QwtPlot(this))
    , m_popUpMenu(new QMenu(this))
    , m_filePath()
    , m_overlayIndex(-1)
    , m_displayCentroidData(false)
{
    m_colorList << Qt::black << Qt::red << Qt::green << Qt::blue << Qt::cyan << Qt::magenta
                << Qt::yellow;

    m_tableModel = new MSReaderTableModel(this);
    m_tableView->setModel(m_tableModel);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_tableView, &QTableView::customContextMenuRequested, this,
            &MSReaderWidget::handleCustomMenuOnTableView);
    connect(m_tableView, &QTableView::doubleClicked, this,
            &MSReaderWidget::handleTableviewDoubleClicked);

    m_plot->setAxisTitle(QwtPlot::Axis::yLeft, tr("Intensity"));
    m_plot->setAxisTitle(QwtPlot::Axis::xBottom, tr("m/z"));
    m_plot->setAxisAutoScale(QwtPlot::Axis::yLeft);
    m_plot->setAxisAutoScale(QwtPlot::Axis::xBottom);

    m_popUpMenu->addAction(tr("Export primary data row to CSV"), this,
                           SLOT(handleExportPrimaryDataToCSV()));
    m_popUpMenu->addAction(tr("Export table to CSV"), this, SLOT(handleExportTableToCSV()));
    m_popUpMenu->addAction(tr("Overlay"), this, SLOT(handlePlotOverlay()));
    m_secondaryExportAction = new QAction(tr("Export secondary data row to CSV"), m_popUpMenu);
    connect(m_secondaryExportAction, &QAction::triggered, this,
            &MSReaderWidget::handleExportSecondaryDataToCSV);

    m_zoomer = new QwtPlotZoomer(m_plot->canvas());
    m_zoomer->setRubberBandPen(QColor(Qt::black));
    m_zoomer->setTrackerPen(QColor(Qt::black));
    m_zoomer->setMousePattern(QwtEventPattern::MouseSelect2, Qt::RightButton, Qt::ControlModifier);
    m_zoomer->setMousePattern(QwtEventPattern::MouseSelect3, Qt::RightButton);
}

MSReaderWidget::~MSReaderWidget()
{
    clearCurves();
}

Err MSReaderWidget::openFile(const QString &filePath)
{
    closeFile();    

    MSReader *reader = MSReader::Instance();
    Err e = reader->openFile(filePath); ree;

    long totalScan;
    long startScan;
    long endScan;

    e = reader->getNumberOfSpectra(&totalScan, &startScan, &endScan); ree;

    m_msInfoList.clear();
    for (int index = startScan; index <= endScan; ++index) {
        MSInfo msInfo;
        e = reader->getScanInfo(index, &msInfo.scanInfo); ree;

        if (msInfo.scanInfo.scanLevel > 1) {
            msreader::PrecursorInfo precursorInfo;
            /// For some vendor data format this gives error which is not sufficient to
            /// return from this point, so didn't return on any such failures
            e = reader->getScanPrecursorInfo(index, &msInfo.precursorInfo);

            if (e != kNoErr) {
                qWarning() << QString::fromStdString(convertErrToString(e));
            }

            reader->getFragmentType(index, msInfo.scanInfo.scanLevel, &msInfo.fragmentationType);
        }

        QPair<int, MSInfo> pair;
        pair.first = index;
        pair.second = msInfo;
        m_msInfoList.push_back(pair);
    }

    m_tableModel->setMSInfoList(m_msInfoList);
    m_filePath = filePath;
    return kNoErr;
}

Err MSReaderWidget::closeFile()
{
    //openCurrentFile();
    clearCurves();

    m_filePath.clear();
    m_plot->replot();

    m_msInfoList.clear();
    m_tableView->reset();
    m_tableModel->setMSInfoList(m_msInfoList);

    return MSReader::Instance()->closeFile();
}

QString MSReaderWidget::filePath() const
{
    return m_filePath;
}

void MSReaderWidget::setCentroidState(bool state)
{
    if (m_displayCentroidData != state) {
        m_displayCentroidData = state;

        // This is just to refresh the data for current selected row
        handleTableviewDoubleClicked(m_tableView->currentIndex());
    }
}

bool MSReaderWidget::centroidState() const
{
    return m_displayCentroidData;
}

bool MSReaderWidget::isEmpty() const
{
    return m_filePath.isEmpty();
}

void MSReaderWidget::renderXICData()
{
    MSXICDataDialog dialog(this);
    dialog.exec();
}

void MSReaderWidget::getChromatogram()
{
    MSChromatogramDialog dialog(this);
    dialog.exec();
}

void MSReaderWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    const int height = this->height() / 2;
    m_plot->setGeometry(LEFT_MARGIN, TOP_MARGIN, this->width() - LEFT_MARGIN - RIGHT_MARGIN,
                        height - TOP_MARGIN - BOTTOM_MARGIN);
    m_tableView->setGeometry(LEFT_MARGIN, height, this->width() - LEFT_MARGIN - RIGHT_MARGIN,
                             height - BOTTOM_MARGIN);
}

void MSReaderWidget::renderPlotData(const point2dList &points, QwtPlotCurve *curve,
                                    bool centroid, QColor color)
{
    QwtPointSeriesData *data = new QwtPointSeriesData();
    const QVector<QPointF> vector = QVector<QPointF>::fromStdVector(points);
    data->setSamples(vector);

    QwtPlotCurve::CurveStyle style = QwtPlotCurve::Lines;
    if (centroid) {
        style = QwtPlotCurve::Sticks;
    }

    curve->setPen(color);
    curve->setStyle(style);
    curve->setData(data);

    // This lines are used to restore zoom effect
    m_plot->setAxisAutoScale(QwtPlot::Axis::yLeft);
    m_plot->setAxisAutoScale(QwtPlot::Axis::xBottom);
    m_plot->replot();

    // To stay in sync with plot data and zoom level, has set zoom base
    m_zoomer->setZoomBase();
}

void MSReaderWidget::handleCustomMenuOnTableView(const QPoint &point)
{
    m_overlayIndex = m_tableView->indexAt(point).row();

    if (m_displayCentroidData
        && msreader::PeakPickingCentroid != m_msInfoList[m_overlayIndex].second.scanInfo.peakMode) {
        m_popUpMenu->addAction(m_secondaryExportAction);
    } else {
        m_popUpMenu->removeAction(m_secondaryExportAction);
    }

    m_popUpMenu->popup(m_tableView->mapToGlobal(point));
}

void MSReaderWidget::exportData(bool isPrimary)
{
    if (openCurrentFile() != kNoErr) {
        return;
    }

    const QString filePath(QFileDialog::getSaveFileName(
        this, tr("Save File..."), QDir::currentPath(), tr("CSV Files (*.csv)")));

    if (!filePath.isEmpty()) {
        point2dList points;

        bool centroid = true;
        if (isPrimary) {
            centroid = false;
        }

        Err e = MSReader::Instance()->getScanData(m_msInfoList[m_overlayIndex].first, &points,
                                                  centroid);

        if (e == kNoErr) {
            e = PlotBase(points).saveDataToFile(filePath);
        }

        if (kNoErr == e) {
            QMessageBox::information(this, tr("Export CSV"), tr("CSV exported successfull"));
        } else {
            QString errorString(QString(tr("Error occured, Error code: %1")).arg(e));
            QMessageBox::critical(this, tr("Export CSV"), errorString);
        }
    }
}

void MSReaderWidget::handleExportPrimaryDataToCSV()
{
    exportData(true);
}

void MSReaderWidget::handleExportSecondaryDataToCSV()
{
    exportData(false);
}

Err MSReaderWidget::openCurrentFile()
{
    Err e = MSReader::Instance()->openFile(m_filePath);

    if (e != kNoErr) {
        qWarning() << "Error opening file" << e;
    }
    return e;
}

void MSReaderWidget::clearCurves()
{
    for (QwtPlotCurve *curve : m_curveList) {
        curve->detach();
        delete curve;
    }

    for (QwtPlotCurve *curve : m_centroidCurveList) {
        curve->detach();
        delete curve;
    }

    m_centroidCurveList.clear();
    m_curveList.clear();
}

void MSReaderWidget::handleExportTableToCSV()
{
    const QString filePath(QFileDialog::getSaveFileName(
        this, tr("Save File..."), QDir::currentPath(), tr("CSV Files (*.csv)")));

    if (!filePath.isEmpty()) {
        CsvWriter writer(filePath);

        if (!writer.open()) {
            QString errorString(QString("Error opening file: %1").arg(filePath));
            QMessageBox::critical(this, tr("Export CSV"), errorString);
            return;
        }

        QStringList stringList;
        for (int index = 0; index < m_tableModel->columnCount(QModelIndex()); ++index) {
            stringList.append(
                m_tableModel->headerData(index, Qt::Horizontal, Qt::DisplayRole).toString());
        }

        writer.writeRow(stringList);

        for (int index = 0; index < m_msInfoList.size(); ++index) {
            QStringList stringList;

            msreader::ScanInfo info(m_msInfoList[index].second.scanInfo);
            stringList.append(QString::number(m_msInfoList[index].first));
            stringList.append(QString::number(info.retTimeMinutes));
            stringList.append(QString::number(info.scanLevel));
            stringList.append(info.nativeId);
            stringList.append(QString::number(info.peakMode));
            stringList.append(QString::number(info.scanMethod));

            if (info.scanLevel > 1) {
                msreader::PrecursorInfo precursorInfo(m_msInfoList[index].second.precursorInfo);
                stringList.append(QString::number(precursorInfo.dMonoIsoMass));
                stringList.append(QString::number(precursorInfo.lowerWindowOffset));
                stringList.append(QString::number(precursorInfo.upperWindowOffset));
                stringList.append(QString::number(precursorInfo.nChargeState));
                stringList.append(precursorInfo.nativeId);
            } else {
                stringList.append("");
                stringList.append("");
                stringList.append("");
                stringList.append("");
                stringList.append("");
            }
            stringList.append(m_msInfoList[index].second.fragmentationType);
            writer.writeRow(stringList);
        }
        QMessageBox::information(this, "Export CSV", "File created successfully");
    }
}

void MSReaderWidget::handlePlotOverlay()
{
    if (m_overlayIndex == -1) {
        return;
    }

    retrieveMSDataPointsAndRenderPlots(m_overlayIndex);
}

void MSReaderWidget::retrieveMSDataPointsAndRenderPlots(int index)
{
    if (openCurrentFile() != kNoErr) {
        return;
    }

    bool centroid = false;
    if (msreader::PeakPickingCentroid == m_msInfoList[index].second.scanInfo.peakMode) {
        centroid = true;
    }

    int colorIndex = m_curveList.size() % m_colorList.size();

    // Primary data will be retrieved over here
    pmi::point2dList massDataPoints;
    Err e = MSReader::Instance()->getScanData(m_msInfoList[index].first, &massDataPoints,
                                              centroid);

    if (kNoErr == e) {
        QwtPlotCurve* curve = new QwtPlotCurve("Mass/Intensity");
        curve->attach(m_plot);
        m_curveList.append(curve);

        renderPlotData(massDataPoints, curve, centroid, m_colorList[colorIndex]);
    }

    // Secondary data will be retrieved over here
    if (m_displayCentroidData && !centroid) {
        Err e = MSReader::Instance()->getScanData(m_msInfoList[index].first,
                                                  &massDataPoints, true);

        if (kNoErr == e && !massDataPoints.empty()) {
            QwtPlotCurve* centroidCurve = new QwtPlotCurve("Mass/Intensity (Centroid)");
            centroidCurve->attach(m_plot);
            m_centroidCurveList.append(centroidCurve);

            renderPlotData(massDataPoints, centroidCurve, true, m_colorList[colorIndex]);
        }
    }
}

void MSReaderWidget::handleTableviewDoubleClicked(const QModelIndex &modelIndex)
{
    clearCurves();
    const int index = modelIndex.row();

    if (index == -1) {
        return;
    }

    retrieveMSDataPointsAndRenderPlots(index);
}
