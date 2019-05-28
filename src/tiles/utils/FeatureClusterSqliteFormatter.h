/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef FEATURE_CLUSTER_SQLITE_FORMATTER_H
#define FEATURE_CLUSTER_SQLITE_FORMATTER_H

#include "pmi_common_ms_export.h"

#include <common_errors.h>
#include <pmi_core_defs.h>

#include <QObject>
#include <QScopedPointer>

class QSqlDatabase;

_PMI_BEGIN

class NonUniformFeature;
class NonUniformFeatureFindingSession;

class PMI_COMMON_MS_EXPORT FeatureClusterSqliteFormatter : public QObject
{
    Q_OBJECT
public:
    explicit FeatureClusterSqliteFormatter(QSqlDatabase *db,
                                           NonUniformFeatureFindingSession *session,
                                           QObject *parent = nullptr);

    ~FeatureClusterSqliteFormatter();

    // initializes the schemas that formatter uses
    Err init();

    Err writeSamples(const QStringList &sampleNames) const;

    void setCurrentSample(const QString &sampleName);

    void acceptFeature(const NonUniformFeature &feature);

    // be sure to select sample first
    Err saveFeature() const;

    Err flush();

signals:
    // TODO: find better name and unify with Csv serializers
    void rowFormatted();

private:
    Err begin();
    Err end();

private:
    Q_DISABLE_COPY(FeatureClusterSqliteFormatter)
    class Private;
    const QScopedPointer<Private> d;
};

_PMI_END

#endif // FEATURE_CLUSTER_SQLITE_FORMATTER_H