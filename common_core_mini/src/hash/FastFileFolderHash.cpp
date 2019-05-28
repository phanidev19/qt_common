/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#include "FastFileFolderHash.h"
#include "HashUtils.h"
#include "pmi_common_core_mini_debug.h"

#include <QDirIterator>
#include <QFileInfo>

_PMI_BEGIN

FastFileFolderHash::FastFileFolderHash(QCryptographicHash::Algorithm algorithm, int sampleCount,
                                       qint64 sampleSize, int minSizeForFastHashMultiplier,
                                       int maxFileCountInFolderToUse)
    : m_hash(algorithm)
    , m_sampleCount(sampleCount)
    , m_sampleSize(sampleSize)
    , m_minSizeForFastHashMultiplier(minSizeForFastHashMultiplier)
    , m_maxFileCountInFolderToUse(maxFileCountInFolderToUse)
{
    debugCoreMini() << "FastFileFolderHash.algorithm:" << hashAlgorithmName(algorithm);
    debugCoreMini() << "FastFileFolderHash.sampleCount:" << m_sampleCount;
    debugCoreMini() << "FastFileFolderHash.sampleSize:" << m_sampleSize;
    debugCoreMini() << "FastFileFolderHash.minSizeForFastHashMultiplier:"
                    << m_minSizeForFastHashMultiplier;
    debugCoreMini() << "FastFileFolderHash.maxFileCountInFolderToUse:"
                    << m_maxFileCountInFolderToUse;
}

FastFileFolderHash::~FastFileFolderHash()
{
}

void FastFileFolderHash::setExtensionPartsToSkip(const QStringList &extensionPartsToSkip)
{
    m_extensionPartsToSkip = extensionPartsToSkip;
}

Err FastFileFolderHash::addPath(const QString &path)
{
    Err e = kNoErr;

    const QFileInfo fileInfo(path);

    if (fileInfo.isFile()) {
        e = addFile(path); ree;

        return e;
    }

    if (fileInfo.isDir()) {
        e = addDir(path); ree;

        return e;
    }

    rrr(kFileOpenError);
}

Err FastFileFolderHash::addFile(const QString &path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly)) {
        rrr(kFileOpenError);
    }

    if (file.size() < m_sampleSize * m_minSizeForFastHashMultiplier * m_sampleCount) {
        if (m_hash.addData(&file)) {
            return kNoErr;
        }

        rrr(kFileOpenError);
    }

    m_hash.addData(QByteArray::number(file.size()));

    const qint64 posStep = file.size() / m_sampleCount;
    const qint64 endPos = posStep * m_sampleCount;

    for (qint64 pos = 0; pos < endPos; pos += posStep) {
        if (!file.seek(pos)) {
            rrr(kFileOpenError);
        }

        const QByteArray sample = file.read(m_sampleSize);

        if (sample.isEmpty()) {
            rrr(kFileOpenError);
        }

        m_hash.addData(sample);
    }

    return kNoErr;
}

namespace
{

struct Entry {
    QString path;
    qint64 size;
};
}

static Err fileEntries(const QString &path, const QStringList &extensionPartsToSkip,
                       QList<Entry> *entries)
{
    entries->clear();

    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        const QString path = it.next();
        const QString suffix = QFileInfo(path).suffix();

        if (!extensionPartsToSkip.empty()
            && std::find_if(std::cbegin(extensionPartsToSkip), std::cend(extensionPartsToSkip),
                            [&](const QString &extensionPart) {
                                return suffix.contains(extensionPart, Qt::CaseInsensitive);
                            })
                != std::cend(extensionPartsToSkip)) {
            continue;
        }

        QFile file(path);

        if (!file.open(QIODevice::ReadOnly)) {
            rrr(kFileOpenError);
        }

        entries->push_back({ path, file.size() });
    }

    return kNoErr;
}

static qint64 totalSize(const QList<Entry> &entries)
{
    return std::accumulate(std::cbegin(entries), std::cend(entries), static_cast<qint64>(0),
                           [](qint64 total, const Entry &entry) { return total + entry.size; });
}

Err FastFileFolderHash::addDir(const QString &path)
{
    Err e = kNoErr;

    QList<Entry> entries;

    e = fileEntries(path, m_extensionPartsToSkip, &entries); ree;

    m_hash.addData(QByteArray::number(totalSize(entries)));

    const auto begin = std::begin(entries);
    const auto end
        = std::next(std::begin(entries), std::min(entries.size(), m_maxFileCountInFolderToUse));

    std::partial_sort(begin, end, std::end(entries), [](const Entry &entry1, const Entry &entry2) {
        return entry1.size > entry2.size || entry1.size == entry2.size && entry1.path < entry2.path;
    });

    for (auto it = begin; it != end; ++it) {
        e = addFile(it->path); ree;
    }

    return e;
}

void FastFileFolderHash::reset()
{
    m_hash.reset();
}

QByteArray FastFileFolderHash::result() const
{
    return m_hash.result();
}

Err FastFileFolderHash::hash(const QString &path, QCryptographicHash::Algorithm algorithm,
                             int sampleCount, qint64 sampleSize, int minSizeForFastHashMultiplier,
                             int maxFileCountInFolderToUse, QByteArray *result)
{
    Err e = kNoErr;

    FastFileFolderHash hash(algorithm, sampleCount, sampleSize, minSizeForFastHashMultiplier,
                            maxFileCountInFolderToUse);

    e = hash.addPath(path); ree;
    *result = hash.result();

    return e;
}

_PMI_END
