/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#ifndef MSREADERWIDGET_H
#define MSREADERWIDGET_H

#include "MSReader.h"
#include "pmi_common_widgets_export.h"
#include "qwt_plot.h"
#include <QItemSelection>
#include <QPair>
#include <QWidget>

struct MSInfo
{
    pmi::msreader::ScanInfo scanInfo;
    pmi::msreader::PrecursorInfo precursorInfo;
    QString fragmentationType;
};

typedef QList<QPair<int, MSInfo>> MSInfoList;

class QwtPlot;
class QwtPlotCurve;
class QwtPlotZoomer;
class QTableView;
class QMenu;
class MSReaderTableModel;
class MSReaderTableView;

namespace pmi
{
enum Err;
}
class PMI_COMMON_WIDGETS_EXPORT MSReaderWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MSReaderWidget(QWidget *parent = nullptr);
    ~MSReaderWidget();

    pmi::Err openFile(const QString &filePath);
    pmi::Err closeFile();
    QString filePath() const;
    void setCentroidState(bool state);
    bool centroidState() const;
    bool isEmpty() const;
    void renderXICData();
    void getChromatogram();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void handleCustomMenuOnTableView(const QPoint &point);
    void handleExportPrimaryDataToCSV();
    void handleExportSecondaryDataToCSV();
    void handleExportTableToCSV();
    void handlePlotOverlay();
    void handleTableviewDoubleClicked(const QModelIndex &modelIndex);

private:
    void renderPlotData(const pmi::point2dList &points, QwtPlotCurve *curve, bool centroid, QColor color = Qt::black);
    void exportData(bool isPrimary);
    void retrieveMSDataPointsAndRenderPlots(int index);
    pmi::Err openCurrentFile();
    void clearCurves();

    MSReaderTableModel *m_tableModel;
    MSReaderTableView *m_tableView;
    QwtPlot *m_plot;
    QwtPlotZoomer *m_zoomer;
    QMenu *m_popUpMenu;
    QAction *m_secondaryExportAction;
    QString m_filePath;
    pmi::point2dList m_currentMassDataPoints;
    pmi::point2dList m_currentCentroidedMassDataPoints;
    MSInfoList m_msInfoList;
    QList<QwtPlotCurve*> m_curveList;
    QList<QwtPlotCurve*> m_centroidCurveList;
    QList<Qt::GlobalColor> m_colorList;
    int m_overlayIndex;
    bool m_displayCentroidData;
};

#endif // MSREADERWIDGET_H
