/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef MSREADER_TEST_UTILS_H
#define MSREADER_TEST_UTILS_H

#include <MSReaderBase.h>

#include <pmi_core_defs.h>

#include <QBuffer>
#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QIODevice>

_PMI_BEGIN

using namespace msreader;

namespace MSReaderTestUtils {

    static QVector<XICWindow> xicFromFile(const QString &filePath) {
        QVector<XICWindow> result;

        QFile file(filePath);
        file.open(QIODevice::ReadOnly);

        QByteArray data = file.readAll();
        QBuffer out(&data);
        out.open(QIODevice::ReadOnly);

        QDataStream ds(&out);

        int i = 0;
        while (!ds.atEnd()){
            XICWindow window;
            ds >> window;
            i++;
            //qDebug() << i << window.mz_start << window.mz_end << window.time_start << window.time_end;
            result.push_back(window);
        }

        return result;
    }

};

_PMI_END

#endif // MSREADER_TEST_UTILS_H