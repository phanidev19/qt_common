/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#include "CacheFileCreatorThread.h"
#include "pmi_common_ms_debug.h"

#include <QProcess>

#include <algorithm>
#include <iterator>

_PMI_BEGIN

bool CacheFileCreatorThread::isFail() const
{
    return m_isFail;
}

void CacheFileCreatorThread::addFile(const QString &file)
{
    m_files.push_back(file);
}

void CacheFileCreatorThread::addFiles(const QStringList &files)
{
    std::copy(std::cbegin(files), std::cend(files), std::back_inserter(m_files));
}

void CacheFileCreatorThread::run()
{
    for (const auto &file : m_files) {
        const int result = QProcess::execute("PMi-CacheFileCreator.exe", QStringList() << file);
        bool isFail = false;

        switch (result) {
        case 0: // success
            break;
        default: // error
            isFail = true;
            break;
        }

        if (isFail) {
            warningMs() << "Error during processing file:" << file;
        }

        m_isFail = isFail;
    }
}

_PMI_END
