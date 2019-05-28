/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include <MSReader.h>
#include <MSReaderShimadzu.h>

#include <QApplication>
#include <QFileInfo>
#include <QScopedPointer>

using namespace pmi;
using namespace pmi::msreader;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        qWarning() << "Usage: ShimadzuGetNumberOfSpectraCompareTest <datafile>";
    }

    const QString filename = QString(argv[1]).trimmed();
    const QFileInfo fileInfo(filename);
    if (!fileInfo.exists()) {
        qWarning() << "Datafile does not exists.";
    }

    QScopedPointer<MSReaderShimadzu> shimadzuReader(new MSReaderShimadzu());

    if (kNoErr != shimadzuReader->openFile(filename)) {
        qWarning() << "Datafile is broken.";
    }


    auto calculator = [](MSReaderShimadzu* shimadzuReader) {
        long totalNumber = -1;
        long startScan = -1;
        long endScan = -1;
        shimadzuReader->getNumberOfSpectra(&totalNumber, &startScan, &endScan);
        qWarning() << totalNumber << " (" << startScan << " to " << endScan << ")";
    };

    calculator(shimadzuReader.data());

    MSReaderShimadzu::DebugOptions options;
    options.getNumberOfSpectraMaxScan = 0;
    shimadzuReader->setDebugOptions(options);
    calculator(shimadzuReader.data());

    options.getNumberOfSpectraMaxScan = 1000000;
    shimadzuReader->setDebugOptions(options);
    calculator(shimadzuReader.data());

}
