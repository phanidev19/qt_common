/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Yong Joo Kil ykil@proteinmetrics.com
 * Author: Jaroslaw Staniek jstaniek@milosolutions.com
 */

#ifndef PMILOGSTREAMREDIRECTOR_H
#define PMILOGSTREAMREDIRECTOR_H

#include <pmi_core_defs.h>

#include <QtDebug>

#include <fstream>

_PMI_BEGIN

class LoggerInstance;

//! Stream redirector from cout/cerr used by Loggger
class LogStreamRedirector : public std::streambuf
{
public:
    LogStreamRedirector(LoggerInstance *log, std::ostream *os, QtMsgType type);

    ~LogStreamRedirector();

private:
    std::streamsize xsputn(const char_type *s, std::streamsize n) override;

    int_type overflow(int_type c = traits_type::eof()) override;

    LoggerInstance * const m_log;
    std::ostream * const m_stream;
    std::streambuf * const m_streamBuffer;
    const QtMsgType m_type;
    QByteArray m_buffer;
};

_PMI_END

#endif
