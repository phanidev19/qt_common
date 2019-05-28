/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#ifndef CACHE_FILE_CREATOR_THREAD_H
#define CACHE_FILE_CREATOR_THREAD_H

#include <common_errors.h>
#include <pmi_core_defs.h>

#include <QThread>

_PMI_BEGIN

class CacheFileCreatorThread : public QThread
{
public:
    bool isFail() const;

    void addFile(const QString &file);
    void addFiles(const QStringList &files);

protected:
    virtual void run() override;

private:
    QStringList m_files;

    bool m_isFail = false;
};

typedef QSharedPointer<CacheFileCreatorThread> CacheFileCreatorThreadPtr;

_PMI_END

#endif // CACHE_FILE_CREATOR_THREAD_H
