/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Lukas Tvrdy ltvrdy@milosolutions.com
 */

#include "InsilicoPeptidesCsvFormatter.h"
#include "NonUniformFeature.h"
#include "NonUniformFeatureFindingSession.h"
#include "NonUniformTileDevice.h"
#include "NonUniformTileRange.h"

#include <PmiQtCommonConstants.h>

#include <PmiQtStablesConstants.h>

_PMI_BEGIN

const QString kIntensity = QStringLiteral("Intensity");

// sync this with InsilicoPeptidesCsvFormatter::EntryColumns
const QStringList ENTRY_HEADER = QStringList{ kSequence,
                                              kUnchargedMass,
                                              kModificationsPositionList,
                                              kModificationsNameList,
                                              kStartTime,
                                              kEndTime,
                                              kChargeList,
                                              kComment,
                                              kGlycanList,
                                              kApexTime,
                                              kIntensity
};

class Q_DECL_HIDDEN InsilicoPeptidesCsvFormatter::Private
{
public:
    Private(NonUniformFeatureFindingSession *ses)
        : session(ses)
    {
        // init output channel: QStringList for now
        outputHillItem = ENTRY_HEADER;
        clearItem();
    }

    NonUniformFeatureFindingSession *session = nullptr;
    QStringList outputHillItem;
    int rowCounter = 0;

public:
    void clearItem()
    {
        for (int i = 0; i < outputHillItem.size(); ++i) {
            outputHillItem[i] = QString();
        }
    }
};

InsilicoPeptidesCsvFormatter::InsilicoPeptidesCsvFormatter(NonUniformFeatureFindingSession *session,
                                                           QObject *parent /*= nullptr*/)
    : QObject(parent)
    , d(new Private(session))
{
}

InsilicoPeptidesCsvFormatter::~InsilicoPeptidesCsvFormatter()
{
}

void InsilicoPeptidesCsvFormatter::acceptFeature(const NonUniformFeature &feature)
{
    const QChar chargeSeparator(',');

    QStringList row = d->outputHillItem;

    row[EntryColumns::Sequence] = QString("UNKNOWN_%1").arg(d->rowCounter);
    row[EntryColumns::UnchargedMass] = QString::number(feature.unchargedMass());

    const QVector<NonUniformHillCluster> clusters = feature.chargeClusters();
    row[EntryColumns::ChargeList] = feature.chargeList();

    const ScanIndexNumberConverter &converter = d->session->converter();
    NonUniformTileRange range = d->session->device()->range();

    ScanIndexInterval minMaxScanIndex = feature.minMaxScanIndex();
    double timeStart = converter.scanIndexToScanTime(minMaxScanIndex.start());
    double timeEnd
        = converter.scanIndexToScanTime(std::min(minMaxScanIndex.end(), range.scanIndexMax()));

    row[EntryColumns::StartTime] = QString::number(timeStart);
    row[EntryColumns::EndTime] = QString::number(timeEnd);
    row[EntryColumns::ApexTime] = QString::number(feature.apexTime());
    row[EntryColumns::Intensity] = QString::number(feature.maximumIntensity());

    emit rowFormatted(row);
    d->rowCounter++;
}

QStringList InsilicoPeptidesCsvFormatter::header()
{
    return ENTRY_HEADER;
}

_PMI_END
