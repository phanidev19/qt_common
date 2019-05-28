#ifndef __MZ_CALIBRATION_H__
#define __MZ_CALIBRATION_H__

#include "PlotBase.h"
#include "config-pmi_qt_common.h"
#include "pmi_common_ms_export.h"
#ifdef PMI_MS_MZCALIBRATION_USE_MSREADER_TO_READ
#include "MSReaderBase.h"
#endif

#include <MzCalibrationOptions.h>

#include <QSqlDatabase>

struct sqlite3;

_PMI_BEGIN

class CacheFileManagerInterface;
class MSReader;

class PMI_COMMON_MS_EXPORT MzCalibration final
{
public:
    MzCalibration();
    explicit MzCalibration(const CacheFileManagerInterface &cacheFileManager);

    void clear() {
        m_filesId = -1;
        m_lockmass_single_point = -1;
    }

    double calibrate(double mz, double time) const;

    void calibrate(double time, PlotBase & plot) const;
    void calibrate(double time, point2dList & plist) const;

    Err readFromDB(QSqlDatabase &db, int filesId);
    Err readFromDB(QString filename, int filesId);
    Err writeToDB(QSqlDatabase &db) const;
    Err writeToDB(sqlite3 *db) const;
    Err writeToDB(QString filename) const;

    Err computeCoefficientsFromByspec(QString filename);
    Err computeCoefficientsFromByspecDB(QSqlDatabase &db);
#ifdef PMI_MS_MZCALIBRATION_USE_MSREADER_TO_READ
    Err computeCoefficientsFromScanList(MSReader * ms, const QList<msreader::ScanInfoWrapper> & list);
#endif

    void clearCoefficients();

    void setup(const MzCalibrationOptions & opt) {
        m_options = opt;
    }

    void setupPreviewCoeffs(double m00, double m11, double m22);

    void setFilesId_callAfterComputing(int filesId) {
        m_filesId = filesId;
    }

    inline const MzCalibrationOptions& options() const { return m_options; }

    inline const QList<pmi::PlotBase>& coefficientPlots() const { return m_coeffList; }

private:
    void _extract_first_lockmass();
    Err _decimateCoefficientPlotSize(int expected_reduced_number_of_points);
    void _setupDBQueries(QString *insertCmd, QString *createTableCmd, QString *deleteFilesCmd) const;
    Err _findCalibrationScansFilter(QSqlDatabase &db, QString * outFilterString) const;

private:
    const CacheFileManagerInterface *m_cacheFileManager;
    int m_filesId;
    QList<pmi::PlotBase> m_coeffList;
    MzCalibrationOptions m_options;
    double m_lockmass_single_point;  /// used when lock_mass_use_ppm_for_one_point is true
};

//! @todo Private functions? Move to MzCalibration_p.h?
Err findWatersLockMassByContent(QString filename, int * function_i);

Err read_lock_mass_from_byspec(QString specname, MzCalibration & precursor_calibration, MzCalibration & fragment_calibration);

/*!
* @brief This will 'harden' all observed peaks, as if instrument took care of the lock mass calibration.
*/
Err applyCoefficientsToByspec(const QString &filename,
                              const CacheFileManagerInterface &cacheFileManager, int filesId);

/*!
 * \brief The MzCalibrationManager class is to maintain a list of files and its associated calibration information.
 *
 * Ideally, we would hold this information as part of the MS files.  But since we do not control vendor files, we must
 * then associate it else where.  We have chosen to place this within the document.  This has a small side benefit that we can
 * re-run calibration algorithm differently per document.
 *
 * The implementation is made such that document creation first creates the calibration and then saves it to the document.  The
 * MSReader (now a singleton) keeps an in-memory version that it uses during project creation and while the document is open.
 * This means MSReader needs to get updated during document open and close (Byosphere will also need to do this during tab toggle).
 */
class PMI_COMMON_MS_EXPORT MzCalibrationManager final
{
public:
    explicit MzCalibrationManager(const CacheFileManagerInterface &cacheFileManager);

    Err loadFromDatabase(QSqlDatabase & db);

    Err loadFromDatabase(const QString & filename);

    Err saveToDatabase(QSqlDatabase & db) const;

    Err saveToDatabase(sqlite3 * db) const;

    typedef QHash<QString, MzCalibration>::const_iterator const_iter;

    const_iter get(QString filename, bool * found) const;

    void add(const QString & _filename, const MzCalibration & obj);

    void clear() {
        m_fileToCalibration.clear();
    }

    QHash<QString, MzCalibration> fileToCalibration() const {
        return m_fileToCalibration;
    }

private:
    const CacheFileManagerInterface *const m_cacheFileManager;
    QHash<QString, MzCalibration> m_fileToCalibration;
};

_PMI_END


#endif
