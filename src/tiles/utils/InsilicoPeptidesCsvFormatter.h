/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef INSILICO_PEPTIDES_CSV_FORMATTER_H
#define INSILICO_PEPTIDES_CSV_FORMATTER_H

#include "pmi_common_ms_export.h"

#include <pmi_core_defs.h>

#include <QObject>
#include <QStringList>

_PMI_BEGIN

class NonUniformFeature;
class NonUniformFeatureFindingSession;

/*!
 * \brief Provides formatting of the feature from feature finder to CSV format usedby Insilico
 * peptides CSV used in Byologic for importing/exporting insilico peptides
 */
class PMI_COMMON_MS_EXPORT InsilicoPeptidesCsvFormatter : public QObject
{
    Q_OBJECT
public:
    // sync with ENTRY_HEADER in cpp
    enum EntryColumns {
        Sequence = 0,
        UnchargedMass,
        ModificationsPositionList,
        ModificationsNameList,
        StartTime,
        EndTime,
        ChargeList,
        Comment,
        GlycanList,
        ApexTime, 
        Intensity,
        ColumnCount
    };

    explicit InsilicoPeptidesCsvFormatter(NonUniformFeatureFindingSession *session,
                                 QObject *parent = nullptr);
    ~InsilicoPeptidesCsvFormatter();

    static QStringList header();

    void acceptFeature(const NonUniformFeature &feature);

signals:
    void rowFormatted(const QStringList &csvRow);

private:
    Q_DISABLE_COPY(InsilicoPeptidesCsvFormatter)
    class Private;
    const QScopedPointer<Private> d;
};

_PMI_END

#endif // INSILICO_PEPTIDES_CSV_FORMATTER_H