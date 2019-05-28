/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Yong Joo Kil ykil@proteinmetrics.com
 * Author: Jaroslaw Staniek jstaniek@milosolutions.com
 */

#include "LogStreamRedirector_p.h"
#include "PmiLoggerInstance_p.h"

#include <cstdio>

_PMI_BEGIN

const int STREAM_BUFFER_SIZE = 4096;
// ---

LogStreamRedirector::LogStreamRedirector(LoggerInstance *log, std::ostream *os, QtMsgType type)
    : m_log(log)
    , m_stream(os)
    , m_streamBuffer(os->rdbuf())
    , m_type(type)
{
    m_buffer.reserve(STREAM_BUFFER_SIZE);
    m_stream->rdbuf(this);
}

LogStreamRedirector::~LogStreamRedirector()
{
    m_stream->rdbuf(m_streamBuffer);
}

std::streamsize LogStreamRedirector::xsputn(const char_type *s, std::streamsize n)
{
    m_buffer.append(s, n);
    return n;
}

LogStreamRedirector::int_type LogStreamRedirector::overflow(int_type c)
{
    if (c == traits_type::eof()) {
        return c;
    }
    if (c == '\n') {
        m_log->emitMessage(m_type, nullptr, QString::fromUtf8(m_buffer));
        m_buffer.clear();
    } else {
        m_buffer.append(static_cast<char>(c));
    }
    return c;
}

_PMI_END
