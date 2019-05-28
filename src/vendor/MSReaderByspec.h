#ifndef _MSREADERBYSPEC_H_
#define _MSREADERBYSPEC_H_

#include <QString>
#include <QMap>
#include <QList>
#include <QStringList>
#include <QSqlDatabase>
#include <QSharedPointer>
#include "MSReaderBase.h"
#include "CentroidOptions.h"
#include "CompressionInfoHolder.h"

_PMI_BEGIN

class CacheFileManagerInterface;
class ProgressBarInterface;

_MSREADER_BEGIN

/*!
 * \brief The MSReaderByspec class will read .byspec2 format. Note, the .byspec (without '2' suffix)
 * is a really old version of this format, which is not supported anymore.  This will also internally
 * convert supported MS data into .byspec2 format and then read the content through the generated .byspec2 file.
 *
 * Note: this used to be called MSReaderPWIZ because it was originally designed to use pwiz_/msconvert-pmi.exe to generate the .byspec2 file.
 * However, it's since been generalized to use multiple CLI to create .byspec2 files.
 */
class PMI_COMMON_MS_EXPORT MSReaderByspec : public MSReaderBase
{
public:
    explicit MSReaderByspec(const CacheFileManagerInterface &cacheFileManager);
    ~MSReaderByspec();

    virtual MSReaderClassType classTypeName() const override;
    bool canOpen(const QString &filename) const override;

    /*!
     * \brief openFile
     * \param filename
     * \param convert_options for doing minimal conversion with only chromatogram
     * \return
     */
    Err openFile(const QString &filename,
                 MSConvertOption convert_options = ConvertWithCentroidButNoPeaksBlobs,
                 QSharedPointer<ProgressBarInterface> progress = NoProgress) override;
    Err closeFile() override;

    //    virtual Err getFileInfo(QMap<QString,QString> & infoItems) const;

    Err getBasePeak(point2dList *points) const override;
    Err getTICData(point2dList *points) const override;
    /// Note: should be const, but the updating the byspec prevents us from doing that.
    Err getScanData(long scanNumber, point2dList *points, bool do_centroiding = false,
                    PointListAsByteArrays *pointListAsByteArrays = NULL) override;
    Err getXICData(const XICWindow &win, point2dList *points, int ms_level = 1) const override;
    Err getScanInfo(long scanNumber, ScanInfo *obj) const override;
    Err getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const override;

    Err getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const override;
    Err getFragmentType(long scanNumber, long scanLevel, QString *fragType) const override;

    Err cacheScanNumbers(const QList<int> &scanNumberList, QSharedPointer<ProgressBarInterface> progress);

    /*!
     * \brief This function will eventually generalize getTIC and getUV.  For now, this replace
     * getUVData() but not getTICData() \param chroList \return Err
     */
    Err getChromatograms(QList<ChromatogramInfo> *chroList,
                         const QString &channelInternalName = QString()) override;

    QSqlDatabase *getDatabasePointer();

    using MSReaderBase::getBestScanNumber;
    Err getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const override;

    /*!
     * \brief This opens the file through database connection.  This is meant to be a one-off
     * function. Hopefully, no one else requires this interface. Do not use this unless you
     * understand exact implementation and assumptions. \param byspecFile \return
     */
    Err indirectOpen_becareful(const QSqlDatabase &db);

    void setCentroidOptionsByspec(const CentroidOptions &centOpt)
    {
        m_centroidOptionInByspec = centOpt;
    }

    Err getLockmassScans(QList<msreader::ScanInfoWrapper> *list) const override;
    MSReaderBase::FileInfoList filesInfoList() const override;

private:
    void clear();
    Err _postOpenMetaPopulate();

    Err _pruneTICChromatogram(point2dList &points) const;
    Err constructCentroidedMS1Cache();
    Err getXICFromCentroidedCache(const msreader::XICWindow &win, point2dList &points) const;
    Err constructTICByDatabaseQuery(point2dList &inpoints) const;
    Err constructTICByQueryingCentroidedMS1(point2dList &inpoints) const;
    Err constructTICBySummingCentroidedMS1(point2dList &inpoints) const;
    /*!
     * \brief extractScanNumbersToPoints
     * \param scanNumberList must be a unique list and ordered; no duplicates
     * \param pointsList corresponding points of the scans
     * \return
     */
    Err extractScanNumbersToByspec(const QList<int> &uniqueAndOrderedScanNumberList,
                                   QList<point2dList> &pointsList, QSharedPointer<ProgressBarInterface> progress);
    Err extractScanNumberAndMergeIntoByspec(qlonglong scanNumber, QSharedPointer<ProgressBarInterface> progress);
    Err patchPeaksBlob(long scanNumber, QSharedPointer<ProgressBarInterface> progress);
    Err makeUniqueAndNotCachedScanNumbers(const QList<int> &scanNumberList,
                                          QList<int> &outScanNumberList);
    Err fetchCompressionInfo();

private:
    const CacheFileManagerInterface *const m_cacheFileManager;
    QSqlDatabase *m_byspecDB;
    QStringList m_tableNames;
    bool m_containsSpectraMobilityValue; //TODO: use PrivateData and make this table schema caching cleaner; possibly as a general utility class
    QHash<QString, QStringList> m_tableColumnNames;
    QString m_databaseConnectionName;
    bool m_containsCompression = false;
    bool m_containsCentroidedCompression = false;
    bool m_containsNonEmptyPeaksMS1CentroidedTable = false;
    CompressionInfoHolder m_compressionInfoHolder;
    /// If true, it means we are opening file by database connection.
    bool m_openIndirectWithDatabase = false;
    // centroiding with smoothing options
    CentroidOptions m_centroidOptionInByspec;
};

PMI_COMMON_MS_EXPORT Err makeByspec(QString inputMSFileName, QString byspecProxyFilename,
                                    MSConvertOption convert_options,
                                    const CentroidOptions &centroidOption,
                                    const IonMobilityOptions &ionMobilityOptions,
                                    QSharedPointer<ProgressBarInterface> progress);

Err computeAndApplyLockMassCalibration(QString byspecFilename,
                                       const CacheFileManagerInterface &cacheFileManager,
                                       QString lockmassListWithAliasName, QString ppmTolerance);

_MSREADER_END
_PMI_END

#endif // _MSREADERBYSPEC_H_
