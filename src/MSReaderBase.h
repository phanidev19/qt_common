/*
 * Copyright (C) 2015-2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef MSREADERBASE_H
#define MSREADERBASE_H

#include "CentroidOptions.h"
#include "MSReaderInterface.h"
#include "MSReaderOptions.h"
#include "MSReaderTypes.h"
#include "pmi_common_ms_export.h"

#include <math_utils.h>
#include <math_utils_chemistry.h>

#include <common_errors.h>
#include <common_math_types.h>

#include <QDataStream>
#include <QDebug>
#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>

_PMI_BEGIN

class ProgressBarInterface;
class MzCalibration;
struct MzCalibration_Options;

// TODO: remove logic from data structures

#define BYOMAP_DEFAULT_CONVERT_OPTION ConvertFull

class PMI_COMMON_MS_EXPORT MSReaderBase : public MSReaderInterface, public MSReaderOptions
{
public:
    enum MSReaderClassType {
        MSReaderClassTypeUnknown = 0,
        MSReaderClassTypeThermo,
        MSReaderClassTypeBruker,
        MSReaderClassTypeWaters,
        MSReaderClassTypeABSCIEX,
        MSReaderClassTypeAgilent,
        MSReaderClassTypeByspec,
        MSReaderClassTypeTesting,
        MSReaderClassTypeBrukerTims,
        MSReaderClassTypeShimadzu,
        MSReaderClassTypeSimulated
    };


    /// Finding parent scan requires adding a small episilon (forget exactly when and why this was
    /// introduced)
    static double scantimeFindParentFudge;

    class ScanInfoList
    {
    public:
        typedef QMap<long, msreader::ScanInfo>::iterator iter;
        typedef QMap<long, msreader::ScanInfo>::const_iterator const_iter;

        typedef QMap<double, long> ScanTimeScanNumberMap;

        Err updateCacheFully(const MSReaderBase *ms) const;

        /*!
         * \brief get scan time for the given scan number
         * \param scanNumber
         * \return
         */
        /*
        Err getScanTimeFromScanNumber(long scan, double * scanTime) const;
        */

        /*!
         * \brief finds the closest scan number for the given time.
         * Note: The common use case it to access an existing scan time.
         * However, due to potential numerical issues, we will check neighboring scan time
         * to make sure to return the closest scan time.
         *
         * \param scanNumber
         * \return Err
         */
        Err getBestScanNumberFromScanTime(const MSReaderBase *ms, int scanLevel, double scanTime,
                                          long *scanNumber) const;

        void clear();

        /*!
         * \brief check if m_scanNumberToInfo is fully populated
         * \return
         */
        bool isFullyCached() const;

        int scanInfoCacheCount(int *totalScansExpected) const;

        Err getAllScansInfoAtLevel(const MSReaderBase *ms, int level,
                                   QList<msreader::ScanInfoWrapper> *list) const;

        /*!
         * \brief getScanInfoCache
         * \param scanNumber
         * \param snfo
         * \return true if cache found
         */
        bool scanInfoCache(long scanNumber, msreader::ScanInfo *snfo) const;

        void setScanInfoCache(long scanNumber, const msreader::ScanInfo &obj);

        Err setScanInfoCache(const QMap<long, msreader::ScanInfo> &scanInfoCacheFull);

        /*!
         * \brief numberOfScansAtLevel returns number of scans given that scan.
         * Zero is returns if no scans for given level.
         *
         * \param level
         * \return
         */
        int numberOfScansAtLevel(const MSReaderBase *ms, int level) const;

    private:
        Err updateFully(const MSReaderBase *ms);

        int m_totalNumberOfScansExpected = -1;
        QMap<long, msreader::ScanInfo> m_scanNumberToInfo;
        QMap<int, ScanTimeScanNumberMap> m_scanTimeToScanNumber;
    };

    typedef QVector<QPair<QString, QString>> FileInfoList;

public:
    virtual ~MSReaderBase();

    virtual MSReaderClassType classTypeName() const = 0;

    virtual Err getBestScanNumberFromScanTime(int msLevel, double scanTime, long *scanNumber) const;

    virtual QString getFilename() const;
    void setFilename(const QString &filename);

    /*!
     * \brief getLockmassScans returns all scans with lock mass in them.
     * This base returns not implemented 'error'.
     * In this case, the MSReader will then assume that MS1 are the ones with the scans.
     *
     * \return
     */
    Err getLockmassScans(QList<msreader::ScanInfoWrapper> *lockmassList) const override;

    /*!
     * \brief getScanInfoListAtLevel meant to be used for open reader.
     * Base class should not be directly called.
     *
     * \param level
     * \param lockmassList
     * \return
     */
    Err getScanInfoListAtLevel(int level,
                               QList<msreader::ScanInfoWrapper> *lockmassList) const override;

    Err cacheScanNumbers(const QList<int> & scanNumberList, QSharedPointer<ProgressBarInterface> progress);

    bool scanInfoCache(long scanNumber, msreader::ScanInfo *obj) const;

    void setScanInfoCache(long scanNumber, const msreader::ScanInfo &obj) const;

    int scanInfoCacheCount(int *totalScansExpected) const;

    void clearScanInfoList();

    /*!
     * \brief setScanInfoCache
     * \param scanInfoCacheFull -- this is expected to be completely full. No error check done yet.
     */
    Err setScanInfoCache(const QMap<long, msreader::ScanInfo> &scanInfoCacheFull);

    int numberOfScansAtLevel(int level) const;

    /*!
     * \brief provide meta information to place into the FilesInfo table
     * \return
     */
    virtual FileInfoList filesInfoList() const;

    static CentroidOptions::CentroidMethod getPreferedCentroidSource(const QString &filename);
    static CentroidOptions::CentroidMethod
    getCentroidMethod_Vendor_Or_Custom(const CentroidOptions &options, const QString &filename);

protected:
    virtual void clear();

private:
    QString m_filename;

    /// cache of scan number and time
    mutable ScanInfoList m_scanInfoList;
};

_PMI_END

#endif // MSREADERBASE_H
