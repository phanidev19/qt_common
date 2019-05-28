/*
* Copyright (C) 2017-2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author  : Sam Bogar (sbogar@proteinmetrics.com)
*/
#ifndef COMPARISONWRITER_H
#define COMPARISONWRITER_H

#include "CsvWriter.h"
#include "MSReaderBase.h"
#include "ComparisonStructs.h"

class ComparisonWriter
{
public:
    ComparisonWriter() {}
    ~ComparisonWriter() {}

    void writeSummaryInfoToFile(const QSet<ComparisonStructs::MetaInfoForScanInBothFiles> &scansPresentInBothFiles,
        const ComparisonStructs::UserInput &userInput, const ComparisonStructs::DiffInfo &diffInfo,
        const QString fileName);
    void writeNativeIDSetToFile(const QString & firstFileName, const QString & secondFileName,
        const QString & outputFileName,
        const QSet<ComparisonStructs::MetaInfoForScanInBothFiles> &source);
    void writeScanSetToFile(const QString &fileName,
        const QSet<ComparisonStructs::MetaInfoForScanInOneFile> &source);
private:
    void putMetaInfoIntoQStringList(
        const ComparisonStructs::MetaInfoForScanInOneFile &MetaInfoForScanInOneFile,
        QStringList *list);
    void putMetaInfoDiffIntoQStringList(const ComparisonStructs::MetaInfoForScanInBothFiles &scan,
        QStringList *list);
};

#endif //COMPARISONWRITER_H