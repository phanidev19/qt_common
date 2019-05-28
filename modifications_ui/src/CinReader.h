/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef CINREADER_H
#define CINREADER_H

#include "pmi_modifications_ui_export.h"

#include <QScopedPointer>
#include <QThread>

class CinReaderWorker : public QObject
{
    Q_OBJECT
public:
    CinReaderWorker(QByteArray *buffer);
    ~CinReaderWorker();

protected:
    virtual void timerEvent(QTimerEvent *event) override;

private:
    QByteArray *m_buffer;
};

/*!
 * \brief CinReader is a non-blocking reader from stdin. It reads stdin in another thread
 */
class PMI_MODIFICATIONS_UI_EXPORT CinReader : public QThread
{
    Q_OBJECT
public:
    CinReader(QByteArray *buffer, QObject *parent = nullptr);
    ~CinReader();

protected:
    void run();

    QScopedPointer<CinReaderWorker> m_worker;
};

#endif // CINREADER_H
