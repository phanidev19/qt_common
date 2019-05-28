/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NONUNIFORM_HILL_CLUSTER_FINDER_H
#define NONUNIFORM_HILL_CLUSTER_FINDER_H

#include "pmi_common_ms_export.h"

#include <pmi_core_defs.h>

#include <QObject>
#include <QScopedPointer>

#include "MSReaderTypes.h"

#include "NonUniformTilePoint.h"
#include "NonUniformTileStore.h"
#include "ProgressBarInterface.h"

_PMI_BEGIN

class AbstractChargeDeterminator;
class AbstractMonoisotopeDeterminator;
class NonUniformFeature;
class NonUniformFeatureFindingSession;
class MzScanIndexRect;
class NonUniformHillCluster;
class ScanIndexNumberConverter;


/*!
* \brief Finds clusters of hills in centroided data stored in NonUniform tiles
*/
class PMI_COMMON_MS_EXPORT NonUniformHillClusterFinder : public QObject {
    Q_OBJECT
public:    
    explicit NonUniformHillClusterFinder(NonUniformFeatureFindingSession *session, QObject *parent = nullptr);
    ~NonUniformHillClusterFinder();

    //! \brief Allows to stop feature finding after percentage limit of points are processed
    //! \a percLimit is expected in range 0.01 - 1.0
    void setPercentLimit(double percLimit);

    static const double INVALID_INTENSITY;

    //! \brief Allows to stop feature finding when max intensity of unprocessed peaks is less than
    //! specified minimum. 
    //!
    //! Set \a maxIntensity to @see NonUniformHillClusterFinder::INVALID_INTENSITY
    //! to disable this stop condition. It's off by default
    //! 
    //! Pragmatic value is 5e4 or 1e5, found by testing with MAM data set
    void setIntensityThreshold(double minIntensity);

    void setChargeDeterminator(AbstractChargeDeterminator *chargeDeterminator);

    void setMonoisotopeDeterminator(AbstractMonoisotopeDeterminator *monoisotopeDeterminator);

    void run(QSharedPointer<ProgressBarInterface> progress = NoProgress);

    //! \brief Stops the feature finding executed by call to NonUniformHillClusterFinder::run()
    void stop();

signals:
    //! \brief provides output continuosly when run() is executed
    //! enum HillClusterEntryColumns provides index value into item for respective values
    // e.g. to get intensity you call item.at(HillClusterEntryColumns::INTENSITY)
    void hillItemDetails(const QVector<NonUniformTilePoint> &hillPoints, int featureId);

    //! \brief provides output continuosly when run() is executed
    void featureFound(const NonUniformFeature& feature);

private:
    void markHills(const QVector<NonUniformTilePoint> &hillPoints, int featureId);

    void publishHillCluster(const NonUniformHillCluster &hillCluster);

    point2dList extractCrossSection(double mz, int scanIndex);

private:
    Q_DISABLE_COPY(NonUniformHillClusterFinder)
    class Private;
    const QScopedPointer<Private> d;
};

_PMI_END

#endif // NONUNIFORM_HILL_CLUSTER_FINDER_H
