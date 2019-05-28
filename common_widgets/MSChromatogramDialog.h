/*
 *  Copyright (C) 2018 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#ifndef MSCHROMATOGRAMDIALOG_H
#define MSCHROMATOGRAMDIALOG_H

#include <QDialog>
#include <QTextEdit>

class MSChromatogramDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MSChromatogramDialog(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event);

private:
    void displayChromatogramData();
    QTextEdit *m_chromatogramTextEdit;
};

#endif // MSCHROMATOGRAMDIALOG_H
