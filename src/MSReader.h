/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef __MS_READER_H__
#define __MS_READER_H__

#include <memory>
#include <sqlite3.h>

#include <QSharedPointer>
#include <QSqlDatabase>

#include <CentroidOptions.h>
#include <GridUniform.h>

#include "MSDataNonUniformAdapter.h"
#include "MSReaderInterface.h"
#include "MSReaderOptions.h"
#include "MzCalibration.h"
#include "ProgressBarInterface.h"

class MSReaderBrukerTest;

_PMI_BEGIN

class CacheFileManager;
class CacheFileManagerInterface;
class MS1PrefixSum;
#ifdef PMI_MS_ENABLE_CENTROID_CACHE
class MSCentroidCacheManager;
#endif

PMI_COMMON_MS_EXPORT Err bugPatchSchema_CompressionInfo(QSqlDatabase &db);

/*!
 * \brief The MSReader class handles all known MS file types.
 * Note: MSReader inherits from MSReaderBase, but this is not required.
 *       But inheriting helps enforce all required functions needed via compiler.
 */
class PMI_COMMON_MS_EXPORT MSReader : public QObject, public MSReaderInterface, public MSReaderOptions
{
    Q_OBJECT

#ifdef PMI_QT_COMMON_BUILD_TESTING
    friend class NonUniformTileBuilderTest;
    friend class MSDataNonUniformAdapterTest;
#endif

public:
    enum XICMode { XICModeVendor, XICModeTiledCache, XICModeManual };

public:
    /*!
     * \brief returns singleton. This is important as we use MSReader instance to manage calibration
     * info. \return
     */
    static MSReader *Instance();
    static void releaseInstance();

    /*!
     * \brief reallocateInstance is intended to free space. This patch was needed because
     *  MS vendor API and/or MSReadBase::m_scanInfoList can consume a lot of memory.
     *
     * \param preserveCalibration was introduced to fix a bug where resetting this also reset the
     * calibration during project creation.
     *
     * \return handle to the reallocated instance.
     */
    static MSReader *reallocateInstance(bool preserveCalibration = true);

    static Err constructCacheFiles(const QStringList &files, int threadCount = 0);

public:
    ~MSReader();

    /*!
     * \brief This will return the name of the currently open one.  Otherwise, it will return
     * unknown. \return
     */
    MSReaderBase::MSReaderClassType classTypeName() const;

    /*!
     * \brief Opens the MS file from filepath provided in @param filename
     *
     * \note: This function should be called in the main thread, because some vendors (like Thermo)
     *        are known to fail to open MS file from different (worker) threads, \see
     *        MSReaderTest::testOpenFromOtherThread
     *
     * \return kNoErr on success; otherwise specific error
     */
    Err openFile(const QString &filename,
        msreader::MSConvertOption convert_options = msreader::ConvertWithCentroidButNoPeaksBlobs,
                 QSharedPointer<ProgressBarInterface> progress = NoProgress) override;
    Err closeFile() override;

    bool canOpen(const QString &filename) const override;

     /*!
     * \brief getLockmassScans returns all scans with lock mass in them.
     * In this case, the MSReader will then assume that MS1 are the ones with the scans.
     *
     * \return
     */
    Err getLockmassScans(QList<msreader::ScanInfoWrapper> *lockmassList) const override;

     /*!
     * \brief getScanInfoListAtLevel meant to be used for open reader.
     *
     * \param level
     * \param lockmassList
     * \return
     */
    Err getScanInfoListAtLevel(int level,
                               QList<msreader::ScanInfoWrapper> *lockmassList) const override;
    Err getBasePeak(point2dList *points) const override;
    Err getTICData(point2dList *points) const override;
    Err getScanData(long scanNumber, point2dList *points, bool do_centroiding = false,
                    msreader::PointListAsByteArrays *pointListAsByteArrays = nullptr) override;
    Err getXICData(const msreader::XICWindow &win, point2dList *points,
                   int msLevel = 1) const override;
    Err getScanInfo(long scanNumber, msreader::ScanInfo *obj) const override;
    Err getScanPrecursorInfo(long scanNumber, msreader::PrecursorInfo *pinfo) const override;
    Err getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const override;

    /*!
     * \brief getFragmentType provides MS/MS (or scan level 2 or higher) information
     * \param scanNumber
     * \param scanLevel is 2 or higher. For scanLevel=1 it returns kNoErr and puts empty string to fragType 
     * \param fragType output
     * \return
     */
    Err getFragmentType(long scanNumber, long scanLevel, QString *fragType) const override;

    /*!
     * \brief This function will eventually generalize getTIC and getUV.
     * For now, this replace getUVData() but not getTICData()
     *
     * \param chroList
     * \return Err
     */
    Err getChromatograms(QList<msreader::ChromatogramInfo> *chroList,
                         const QString &channelInternalName = QString()) override;

    using MSReaderInterface::getBestScanNumber;
    Err getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const override;

#ifdef PMI_QT_COMMON_BUILD_TESTING
    // used in MSDataNonUniformAdapterTest.cpp for dumping XIC information
    Err getBestScanNumberFromScanTime(int msLevel, double scanTime, long *scanNumber) const;
#endif

    QString getFilename() const;

    Err computeLockMass(int filesId, const MzCalibrationOptions &option);

    Err saveCalibrationToDatabase(QSqlDatabase &db) const;

    Err saveCalibrationToDatabase(sqlite3 *db) const;

    Err loadCalibrationAndCentroidOptionsFromDatabase(const QString &filename);

    void clearCalibrationCentroidOptions();

    /*!
     * \brief This will do hard close on all existing file connections that's being managed.
     * \return Err
     */
    void closeAllFileConnections();

    Err getTimeDomain(double *startTime, double *endTime) const;

    void setCentroidOptions(const CentroidOptions &centroidOptions);

    bool isOpen() const;

    Err getScanDataMS1Sum(double startTime, double endTime, GridUniform *outGrid,
                          QSharedPointer<ProgressBarInterface> progress);
    Err createPrefixSumCache(QSharedPointer<ProgressBarInterface> progress = NoProgress);

    Err getDomainInterval_sampleContent(int level, double *startMz, double *endMz,
                                        int max_number_of_scans_to_check = 30) const;

    Err _getScanDataMS1SumManualScanNumber(int msLevel, long scanStart, long scanEnd,
                                           GridUniform::ArithmeticType type, GridUniform *gridOut);
    Err _getScanDataMS1SumManualScanTime(int msLevel, double startTime, double endTime,
                                         GridUniform::ArithmeticType type, GridUniform *gridOut);

    Err _getScanDataMS1SumPrefix(int msLevel, double startTime, double endTime,
                                 GridUniform *outGrid, QSharedPointer<ProgressBarInterface> progress);

    void setUseScanInfoCache(bool useCache);

    int bestMSLevelOne() const;

    MSReaderBase::FileInfoList filesInfoList() const;

    //!\brief Setup method for XIC extraction
    // @param mode
    //      XICModeVendor (default) If vendor's XIC is not implemented, it fall backs to manual
    //      method XICModeTiledCache - NonUniform cache is created and used XICModeManual - XIC
    //      extracted manually from centroided scans
    void setXICDataMode(XICMode mode);

    Err setScanInfoCache(long scanNumber, const msreader::ScanInfo &obj);

    int maxNumberOfPrefixSumScansToCache() const;

    void setMaxNumberOfPrefixSumScansToCache(int size);

    CacheFileManagerInterface *cacheFileManager();
    const CacheFileManagerInterface *cacheFileManager() const;

    //! \brief aligns times for given XIC windows to times of real scan numbers
    Err alignTimesInWindow(const msreader::XICWindow &win, const MSDataNonUniformAdapter *adapter,
                           int msLevel, msreader::XICWindow *transformed) const;

    /*!
     * \brief This will call MSReaderByspec class instead of using other vendor API. The reason for
     * doing this is to allow MSReaderBypsec to convert vendor files into .byspec2 file and use that
     * instead of reading through vendor API. This is done primarly for speed (but traded with space).
     */
    void clearVendorsRoutedThroughByspec();
    void setVendorsRoutedThroughByspec(const QSet<MSReaderBase::MSReaderClassType> &routedVendors);
    QSet<MSReaderBase::MSReaderClassType> vendorsRoutedThroughByspec() const;

    // it is a temporary solution until byspec3 is implemented
    // see https://proteinmetrics.atlassian.net/browse/LT-4173?focusedCommentId=83352&page=com.atlassian.jira.plugin.system.issuetabpanels:comment-tabpanel#comment-83352
    bool isBrukerTims() const;

private:
    MSReader();

    Err _getXICManual(const msreader::XICWindow &win, point2dList *points, int msLevel) const;
    Err _getBasePeakManual(point2dList *points) const;
    Err _getTICManual(point2dList *points) const;
    Err _getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const;

    Err _loadCentroidOptionsFromDatabase(const QString &filename);
    Err _makeReader(const QString &fileName);

    /*!
     * \brief Because the byspec can be opened in different modes (chronos only, full), we will
     * close them right after using.  Otherwise, the opening gets complicated.
     */
    void _closeAllByspecConnections();

    void _handlePassingCentroidToMSReaderByspec();

    Err getXICDataCached(const msreader::XICWindow &win, point2dList *points, int ms_level);

    /*!
     * \brief helper function to dump content of the xicData to csv files if the actual and expected
     * differs
     */
    void dumpXicData(const QString &msFilePath, const point2dList &actual,
                     const point2dList &expected) const;

    QSharedPointer<MSReaderBase> makeReader(MSReaderBase::MSReaderClassType reader) const;

    Err getPrefixSumCache(std::shared_ptr<MS1PrefixSum> *ms1PrefixSumPtr,
                          QSharedPointer<ProgressBarInterface> progress = NoProgress);

private:
    static MSReader *s_instance;

private:
    const QScopedPointer<CacheFileManager> m_cacheFileManager;

    /*!
     * \brief This contains un-opened list of possible vendors.  This will be used to figure out
     * what instance we should create to read.
     */
    QList<QSharedPointer<MSReaderBase>> m_vendorList;

    /*!
     * \brief This is the handle to currently opened file after calling openFile
     */
    QSharedPointer<MSReaderBase> m_openReader;

    /*!
     * \brief This keeps a list of opened connections.  We do not want to close them right away as
     * it takes some time to re-open.
     * \remark
     * We also do not want to keep them open for a very long time
     * because the user may want to delete/move them.  Or if our application crashes, will the file
     * get locked?  A middle ground is to close any existing connections when the user closes
     * document. For now, we'll close all connections when a document gets closed.  Later, we may
     * want to selectively close them for the given document.
     */
    QList<QSharedPointer<MSReaderBase>> m_openedReaderList;

    MzCalibrationManager m_caliManager;

    CentroidOptions m_centroidOption; // centroiding with smoothing options

    QMap<QString, std::shared_ptr<MS1PrefixSum>> m_fileName_ms1PrefixSumPtr;
    QMap<QString, QSharedPointer<MSDataNonUniformAdapter>> m_fileName_nonUniformTiles;

#ifdef PMI_MS_ENABLE_CENTROID_CACHE
    MSCentroidCacheManager *m_centroidCacheManager;
#endif

    /// Temporary solution to disable ScanInfo caching for Byomap/Intact MS1 extraction.
    bool m_useScanInfoCache = true;

    /// Count of number of open ms reader with cached scan info. This will be trigged during
    /// closeFile().
    int m_scanInfoListCacheCountMax = 1;

    XICMode m_xicMode;

    /// Used for MS1PrefixSum classs
    int m_maxNumberOfPrefixSumScansToCache = 100;

    /// Read these vendors with MsReaderByspec instead of their native API
    QSet<MSReaderBase::MSReaderClassType> m_vendorsRoutedThroughByspec;
};

_PMI_END

#endif
