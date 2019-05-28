/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NONUNIFORM_FEATURE_FINDING_SESSION_H
#define NONUNIFORM_FEATURE_FINDING_SESSION_H

#include "ScanIndexNumberConverter.h"
#include "pmi_common_ms_export.h"

// common_stable
#include <pmi_core_defs.h>
#include <common_math_types.h>

// common_tiles
#include <NonUniformTileManager.h>
#include <NonUniformTileStore.h>

#include <QScopedPointer>

class QRect;

_PMI_BEGIN

class MSDataNonUniformAdapter;
class MzScanIndexRect;
struct NonUniformTilePoint;
class NonUniformTileDevice;

/*
 * \brief Facade holding together objects that feature finder typically needs, represents the state
 * of the feature finding
 *
 * The state of feature finder involves the state of selection (which points are already processed),
 * state of index (which centroided point is the next maximum intensity) and others like hill index
 */
class PMI_COMMON_MS_EXPORT NonUniformFeatureFindingSession
{

public:
    explicit NonUniformFeatureFindingSession(MSDataNonUniformAdapter *doc);
    ~NonUniformFeatureFindingSession();

    //! \brief Begins painting session by initializing needed structures and returns true if
    //! successful; otherwise returns false.
    bool begin();

    //! \brief Ends the session by releasing the resources allocated. You don't normally need to
    //! call this since it is called by the destructor.
    void end();

    //! \brief Defines the area that feature finder will use to find features in. Note: it's aligned
    //! to tiles for now so if the area in \a searchArea is within tile bounds, bigger area will be used in the end
    //! aligned to MzScanIndexRect occupied by tiles
    void setSearchArea(const MzScanIndexRect &searchArea);
    MzScanIndexRect searchArea() const;

    void setSearchArea(const QRect &tileRect);

    QRect searchAreaTile() const;

    //! \brief Provides current maximum ion in the tiles, takes selection into account
    //! \see selectionTileManager() and updateIndexForTiles()
    void maxIntensity(point2d *maxMzIntensity, NonUniformTilePoint *point);

    //!\brief Provides manager, that holds tiles that contain hill indexes.
    //!
    //! \return pointer, which can be null, check if available before using it
    NonUniformTileHillIndexManager *hillIndexManager() const;

    //!\brief tiles that contain bit flag if tile point is processed, pointer always available
    NonUniformTileSelectionManager *selectionTileManager() const;

    //!\brief tiles holding ms1 content, pointer always available
    NonUniformTileManager *tileManager() const;

    //!\brief device holding ms1 content
    NonUniformTileDevice *device() const;

    MSDataNonUniformAdapter *document() const;

    //!\brief For debugging purposes, provides stats about points already processed 
    void searchAreaSelectionStats(int *selected, int *deselected);

    //!\brief When provided, additional information about hills is stored: hill id for every point
    //! Useful for debugging&verifying of hill building algorithm
    void setHillIndexStore(NonUniformTileHillIndexStore *hillIndexStore);

    const ScanIndexNumberConverter &converter() const;

    //! \brief number of points that will be processed from the document provided 
    int totalPointCount() const;

    // \brief Update the index which tracks maximum in tiles. Provide
    // \a tilePositions with list of tiles which were processed and their selection has been changed
    // externally (e.g. modifying selection of points in tiles through selectionTileManager()
    void updateIndexForTiles(const QList<QPoint> &tilePositions);

private:
    bool initializeSelection();
    bool initializeHillIndex();
    bool initializeIndex();

private:
    Q_DISABLE_COPY(NonUniformFeatureFindingSession)
    class Private;
    const QScopedPointer<Private> d;
};

_PMI_END

#endif // NONUNIFORM_FEATURE_FINDING_SESSION_H