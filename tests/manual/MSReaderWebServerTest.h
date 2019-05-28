/*
 * Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef MSREADERWEBSERVERTEST_H
#define MSREADERWEBSERVERTEST_H

#include <QHttpEngine/QHttpHandler>

class QDir;

//! A Web handler that serves .raw files from specified directory
class MSReaderWebHandler : public QHttpHandler
{
    Q_OBJECT
public:
    explicit MSReaderWebHandler(const QString &dir, QObject *parent = nullptr);
    ~MSReaderWebHandler();

protected:
    /** @brief Reimplementation of QHttpHandler::process()
     * Process the request either by fulfilling it, send a redirect with QHttpSocket::writeRedirect()
     * or write an error to the socket using QHttpSocket::writeError().
     * Supported paths:
     * - /
     */
    void process(QHttpSocket *socket, const QString &path) Q_DECL_OVERRIDE;

    void processDirectory(QHttpSocket *socket, const QString &relativePath, const QString &absolutePath);
    void processFile(QHttpSocket *socket, const QString &relativePath, const QString &absolutePath);
    void sendContents(QHttpSocket *socket, const QString &title, const QString &contents);

private:
    class Private;
    Q_DISABLE_COPY(MSReaderWebHandler)
    Private* const d;
};

#endif
