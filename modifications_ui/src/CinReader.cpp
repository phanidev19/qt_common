/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "CinReader.h"

#include <fcntl.h>
#include <io.h>
#include <iostream>

CinReader::CinReader(QByteArray *buffer, QObject *parent)
    : QThread(parent)
{
    _setmode(_fileno(stdin), _O_BINARY);
    m_worker.reset(new CinReaderWorker(buffer));
    m_worker->moveToThread(this);
}

CinReader::~CinReader()
{
}

void CinReader::run()
{
    m_worker->startTimer(250);
    exec();
    m_worker.reset();
}

CinReaderWorker::CinReaderWorker(QByteArray *buffer)
    : m_buffer(buffer)
{
}

CinReaderWorker::~CinReaderWorker()
{
}

void CinReaderWorker::timerEvent(QTimerEvent *event)
{
    while (!std::cin.eof()) {
        char byte;
        std::cin.get(byte);
        m_buffer->append(byte);
    }
}
