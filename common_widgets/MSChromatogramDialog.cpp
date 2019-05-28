/*
 *  Copyright (C) 2018 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is
 * strictly prohibited. Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#include "MSChromatogramDialog.h"
#include "MSReader.h"

namespace
{
const int LEFT_OFFSET = 10;
const int TOP_OFFSET = 10;
const int RIGHT_OFFSET = 10;
const int BOTTOM_OFFSET = 10;

const int DIALOG_MINIMUM_WIDTH = 600;
const int DIALOG_MINIMUM_HEIGHT = 400;
} // namespace

MSChromatogramDialog::MSChromatogramDialog(QWidget *parent)
    : QDialog(parent)
{
    m_chromatogramTextEdit = new QTextEdit(this);
    m_chromatogramTextEdit->setReadOnly(true);

    setMinimumWidth(DIALOG_MINIMUM_WIDTH);
    setMinimumHeight(DIALOG_MINIMUM_HEIGHT);

    displayChromatogramData();
}

void MSChromatogramDialog::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    m_chromatogramTextEdit->setGeometry(LEFT_OFFSET, TOP_OFFSET,
                                        this->width() - LEFT_OFFSET - RIGHT_OFFSET,
                                        this->height() - TOP_OFFSET - BOTTOM_OFFSET);
}

void MSChromatogramDialog::displayChromatogramData()
{
    QList<pmi::msreader::ChromatogramInfo> chroInfoList;
    const QString channelInternalName;
    pmi::MSReader::Instance()->getChromatograms(&chroInfoList, channelInternalName);

    QString data;

    for (pmi::msreader::ChromatogramInfo info : chroInfoList) {
        QString line("%1\t%2\t%3\n");
        data.append(line.arg(QString::number(info.points.size()))
                        .arg(info.channelName)
                        .arg(info.internalChannelName));
    }

    m_chromatogramTextEdit->setText(data);
}
