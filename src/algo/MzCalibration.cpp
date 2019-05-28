#include <iostream>
#include "sqlite_utils.h"
#include "MzCalibration.h"
#include "QtSqlUtils.h"
#include "pmi_common_ms_debug.h"

#ifdef PMI_MS_MZCALIBRATION_USE_MSREADER_TO_READ
#include "vendor/MSReaderByspec.h"
#include "MSReader.h"
#include "PlotBaseUtils.h"
#endif

#include <CacheFileManager.h>
#include <PmiQtCommonConstants.h>

using namespace std;

_PMI_BEGIN

//////////////////////////////////////////////////////
// Utils
//////////////////////////////////////////////////////
point2dList getNeighboringPointsFromIndex(const PlotBase & inPlot, int index, int width)
{
    point2dList plist;
    if (inPlot.getPointListSize() <= 0)
        return plist;

    int start_index = index - width;
    int end_index = index + width;

    if (start_index < 0) {
        end_index += -start_index;
        start_index = 0;
    }

    //Example
    //[0,1,2,3,4]  len = 5
    //end_index = 5
    //start_index = 1;
    //start_index -= (5-1 - 5) --> -= 1;
    //start_index = 0;

    if (end_index >= inPlot.getPointListSize()) {
        start_index += (inPlot.getPointListSize() - 1 - end_index);
        end_index = inPlot.getPointListSize() - 1;
    }

    if (start_index < 0) start_index = 0;

    for (int i = start_index; i <= end_index; i++) {
        plist.push_back(inPlot.getPoint(i));
    }

    return plist;
}

inline point2d getMedianPoint(point2dList & plist)
{
    point2d p;
    if (plist.size() <= 0)
        return p;

    sort(plist.begin(), plist.end(), point2d_less_y);
    p = plist.at(plist.size() / 2);
    return p;
}

inline point2d getAveragePoint(point2dList & plist)
{
    point2d p(0, 0);
    point2d sum(0, 0);
    if (plist.size() <= 0)
        return p;

    for (int i = 0; i < (int)plist.size(); i++) {
        sum += plist[i];
    }
    p = sum / plist.size();

    return p;
}

Err medianFilter(const PlotBase & inPlot, int width, PlotBase & outPlot)
{
    Err e = kNoErr;

    outPlot.clear();
    for (int i = 0; i < inPlot.getPointListSize(); i++) {
        point2dList plist = getNeighboringPointsFromIndex(inPlot, i, width);
        if (plist.size() <= 0) {
            std::cout << "no neighboring points found. inPlot size = " << inPlot.getPointListSize() << std::endl;
            outPlot.addPoint(inPlot.getPoint(i));
        }
        else {
            point2d medPoint = getMedianPoint(plist);
            medPoint.rx() = inPlot.getPoint(i).x();
            outPlot.addPoint(medPoint);
        }
    }

//error:
    return e;
}

Err computeMovingAverage(const PlotBase & inPlot, int width, PlotBase & outPlot)
{
    Err e = kNoErr;

    outPlot.clear();
    for (int i = 0; i < inPlot.getPointListSize(); i++) {
        point2dList plist = getNeighboringPointsFromIndex(inPlot, i, width);
        if (plist.size() <= 0) {
            std::cout << "no neighboring points found. inPlot size = " << inPlot.getPointListSize() << std::endl;
            outPlot.addPoint(inPlot.getPoint(i));
        }
        else {
            point2d averagePoint = getAveragePoint(plist);
            averagePoint.rx() = inPlot.getPoint(i).x();
            outPlot.addPoint(averagePoint);
        }
    }

    return e;
}

point2d largest(const point2dList & plist) {
    point2d p;
    if (plist.size() <= 0)
        return p;
    p = plist[0];
    for (int i = 1; i < (int)plist.size(); i++) {
        if (p.y() < plist[i].y()) {
            p = plist[i];
        }
    }
    return p;
}

Err printDebugCal_same_length(QString filename, QList<PlotBase> & list, QStringList titles) {
    Err e = kNoErr;
    if (list.size() <= 0)
        return e;
    int psize = list[0].getPointListSize();
    for (int i = 1; i < list.size(); i++) {
        if (psize != list[i].getPointListSize()) {
            e = kBadParameterError; eee;
        }
    }

    {
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly)) {
            //Note: If in C:\Programs Files\... folder, it cannot open write file properly.
            e = kFileOpenError; ree;
        }

        QTextStream out(&file);
        out << titles.join(",") << endl;

        for (int i = 0; i < psize; i++) {
            for (int j = 0; j < list.size(); j++) {
                const point2dList & plist = list[j].getPointList();
                if (j == 0) {
                    out << plist[i].x() << "," << plist[i].y() ;
                }
                else {
                    out << "," << plist[i].y();
                }
            }
            out << endl;
        }
        //error:
        return e;
    }

error:
    return e;
}

Err findWatersLockMassByContent(QString filename, int * function_i)
{
    Err e = kNoErr;
    {
        *function_i = -1;
        QSqlDatabase db = QSqlDatabase::addDatabase(kQSQLITE, "find_waters_lockmass");
        db.setDatabaseName(filename);
        if (!db.open()) {
            warningMs() << "Could not open file:" << filename << endl;
            warningMs() << db.lastError().text();
            e = kBadParameterError; eee;
        }
        {
            debugMs() << "Looking at file=" << filename;
            QString original_path;
            QSqlQuery q = makeQuery(db, true);
            e = QEXEC_CMD(q, "SELECT Filename FROM Files"); eee;
            if (q.next()) {
                original_path = q.value(0).toString();
            }

            for (int i = 1; i < 100; i++) {
                cout << "Looking at function=" << i << endl;
                int total_count = 0;
                int meta_calibrate_count = 0;
                QString cmd = "\
                        SELECT k.PeaksMz, k.PeaksIntensity, s.RetentionTime, s.MetaText, s.FilesId \
                        FROM Peaks k \
                        JOIN Spectra s ON s.PeaksId = k.Id \
                        WHERE [lock_mass_scan_filter] \
                        ORDER BY s.FilesId, s.RetentionTime \
                        ;";
                QString filter = QString("s.NativeId LIKE 'function=%1 %'").arg(i);
                cmd = cmd.replace("[lock_mass_scan_filter]", filter);
                QHash<int, int> mz100_count;
                e = QEXEC_CMD(q, cmd); eee;
                while (q.next()) {
                    PlotBase plot_scan;
                    QString metaText = q.value(3).toString();
                    if (metaText.contains("<Calibration>dynamic</Calibration>")) {
                        meta_calibrate_count++;
                    }
                    e = byteArrayToPlotBase(q.value(0).toByteArray(), q.value(1).toByteArray(), &plot_scan, true); eee;
                    if (plot_scan.getPointListSize() <= 3) //ignore uv data
                        continue;
                    point2d p = plot_scan.getMaxPoint();
                    int mz_100 = p.x() * 10;
                    mz100_count[mz_100] += 1;
                    total_count++;
                }
                if (total_count == 0) {
                    cout << "no content found at function i = " << i << " stopping." << endl;
                    break;
                }
                if (meta_calibrate_count > 0) {
                    cout << "calibrant found in function i = " << i << endl;
                }
                if (total_count < 10) {
                    if (meta_calibrate_count > 0) {
                        qWarning() << original_path << "calibrant metatext found in function i = " << i << " but skipping as total_count=" << total_count;
                    }
                    continue;
                }
                foreach(int mz_key, mz100_count.keys()) {
                    double percent = mz100_count[mz_key] / (total_count + 1e-10);
                    if (percent > 0.6) {
                        QString message = QString("%1 @ function %2 || likely mass lock at m/z=%4 !!! with %3 percent contribution with %5 scans").arg(original_path).arg(i).arg(percent * 100.0).arg(mz_key/10.0).arg(total_count);
                        cout << message.toStdString() << endl;
                        cerr << message.toStdString() << endl;
                        *function_i = i;
                        break;
                    }
                }
                if (meta_calibrate_count > 0) {
                    if (*function_i == i) {
                        cout << "well done, function and meta text both agree, yes calibrant on function " << i << endl;
                    }
                    else {
                        cerr << "!!! meta text and content did not agree about what's calibrant:" << endl;
                        cerr << "meta_calibrate_count=" << meta_calibrate_count << " *function_i,i=" << *function_i << "," << i << endl;
                        cout << "!!! meta text and content did not agree about what's calibrant:" << endl;
                        cout << "meta_calibrate_count=" << meta_calibrate_count << " *function_i,i=" << *function_i << "," << i << endl;
                    }
                }
                else {
                    if (*function_i == i) {
                        cerr << "!!! meta text and content did not agree about what's calibrant:" << endl;
                        cerr << "meta_calibrate_count=" << meta_calibrate_count << " *function_i,i=" << *function_i << "," << i << endl;
                        cout << "!!! meta text and content did not agree about what's calibrant:" << endl;
                        cout << "meta_calibrate_count=" << meta_calibrate_count << " *function_i,i=" << *function_i << "," << i << endl;
                    }
                    else {
                        cout << "well done, function and meta text both agree, no calibrants on function " << i << endl;
                    }
                }
            }
        }
    }

error:
    QSqlDatabase::removeDatabase("find_waters_lockmass");
    return e;
}


//////////////////////////////////////////////////////
// MzCalibration
//////////////////////////////////////////////////////

MzCalibration::MzCalibration()
    : m_cacheFileManager(nullptr)
{
    clear();
}

MzCalibration::MzCalibration(const CacheFileManagerInterface &cacheFileManager)
    : m_cacheFileManager(&cacheFileManager)
{
    clear();
}

double MzCalibration::calibrate(double mz, double time) const
{
    if (m_coeffList.size() <= 0 || mz <= 0)
        return mz;
    static int use_boundary_value = 1;
    static int interpolate = 1;

    //just handle constant for now
    double mz_delta_total = 0;
    double x = 1;
    //Note: Old recal have negation to coefficients already: mz + m00 + m11 * mz + m22 * mz * mz;
    //New recal keeps the coefficients in the same direction as the original delta fit:
    // mz - (a + b * mz + c * mz^2)

    static int count = 0;
    if (m_options.lock_mass_use_ppm_for_one_point && m_coeffList.size() == 1 && m_lockmass_single_point > 0) {
        double coeff0_aka_constant_offset = m_coeffList[0].evaluate(time, interpolate, use_boundary_value);

        double ppmError = coeff0_aka_constant_offset / m_lockmass_single_point; // *1000000;
        mz_delta_total = mz * ppmError; // / 1000000;

        if (count++ == 0) {
            cout << "using ppm calibration with single point with coeff of " << coeff0_aka_constant_offset << " at time " << time << endl;
        }
    }
    else {
        if (count++ == 0) {
            cout << "using constant calibration" << endl;
        }
        for (int i = 0; i < m_coeffList.size(); i++) {
            double delta = m_coeffList[i].evaluate(time, interpolate, use_boundary_value);
            mz_delta_total += delta * x;
            x = x * mz;
        }
    }

    return mz - mz_delta_total;
}

void MzCalibration::calibrate(double time, PlotBase & plot) const
{
    point2dList & plist = plot.getPointList();
    calibrate(time, plist);
}

void MzCalibration::calibrate(double time, point2dList & plist) const
{
    for (int i = 0; i < (int)plist.size(); i++) {
        point2d & p = plist[i];
        p.rx() = calibrate(p.x(), time);
    }
}

Err MzCalibration::readFromDB(QString filename, int filesId)
{
    Err e = kNoErr;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(kQSQLITE, "MzCalibration_readFromDB");
        db.setDatabaseName(filename);
        if (!db.open()) {
            warningMs() << "Could not open file:" << filename << endl;
            warningMs() << db.lastError().text();
            e = kBadParameterError; eee;
        }
        e = readFromDB(db, filesId); eee;
    }

error:
    QSqlDatabase::removeDatabase("MzCalibration_readFromDB");
    return e;
}

Err MzCalibration::writeToDB(QString filename) const
{
    Err e = kNoErr;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(kQSQLITE, "MzCalibration_writeFromDB");
        db.setDatabaseName(filename);
        if (!db.open()) {
            warningMs() << "Could not open file:" << filename << endl;
            warningMs() << db.lastError().text();
            e = kBadParameterError; eee;
        }
        e = writeToDB(db); eee;
    }

error:
    QSqlDatabase::removeDatabase("MzCalibration_writeFromDB");
    return e;
}

Err MzCalibration::readFromDB(QSqlDatabase &db, int _filesId)
{
    Err e = kNoErr;
    QStringList nameList;

    m_options.clear();
    clearCoefficients();

    QSqlQuery q = makeQuery(db, true);
    e = GetSQLiteTableNames(db, nameList); eee;
    if (nameList.contains(kMzCalibration)) {
        e = QEXEC_CMD(q, "SELECT FilesId, LockMassList, PPMTolerance, MovingMedian, MovingAverage, C0X,C0Y,C1X,C1Y,C2X,C2Y FROM MzCalibration"); eee;
        while(q.next()) {
            clearCoefficients();
            m_options.clear();
            int filesId = q.value(0).toInt();
            if (_filesId < 0) {
                warningMs() << "filesId not expected = " << _filesId;
            }
            if (filesId != _filesId)
                continue;

            m_filesId = filesId;
            m_options.lock_mass_list = q.value(1).toString().toStdString();
            m_options.lock_mass_ppm_tolerance = q.value(2).toString().toStdString();
            m_options.lock_mass_moving_median_width = q.value(3).toInt();
            m_options.lock_mass_moving_average_width = q.value(4).toInt();

            QVector<QVariant> ba_cx, ba_cy;
            ba_cx.resize(3);
            ba_cy.resize(3);
            ba_cx[0] = q.value(5).toByteArray();
            ba_cy[0] = q.value(6).toByteArray();

            ba_cx[1] = q.value(7).toByteArray();
            ba_cy[1] = q.value(8).toByteArray();

            ba_cx[2] = q.value(9).toByteArray();
            ba_cy[2] = q.value(10).toByteArray();

            for (int i = 0; i < 3; i++) {
                if (ba_cx[i].isNull() || ba_cy[i].isNull()) {
                    break;
                }
                m_coeffList.push_back(PlotBase());
                e = byteArrayToPlotBase(ba_cx[i].toByteArray(), ba_cy[i].toByteArray(), &m_coeffList.back(), true); eee;
            }
        }
    }

    _extract_first_lockmass();

error:
    return e;
}

void MzCalibration::_extract_first_lockmass()
{
    m_lockmass_single_point = -1;
    QList<double> lock_mass_list = m_options.lockMassList();
    if (lock_mass_list.size() > 0) {
        m_lockmass_single_point = lock_mass_list.front();
    }
}


void MzCalibration::_setupDBQueries(QString *insertCmd, QString *createTableCmd,
                                   QString *deleteFilesCmd) const
{
    QString lock_mass_ppm_tolerance = m_options.lock_mass_ppm_tolerance.c_str();
    bool valid = false;
    lock_mass_ppm_tolerance.toDouble(&valid);
    if (!valid) {
        cout << "lock_mass_ppm_tolerance changed because it was empty " << m_options.lock_mass_ppm_tolerance << endl;
        lock_mass_ppm_tolerance = "100";
    }
    //cout << "lock_mass_ppm_tolerance=" << lock_mass_ppm_tolerance.toStdString() << endl;

    *insertCmd = QString("\
                  INSERT INTO MzCalibration(FilesId,LockMassList,PPMTolerance,MovingMedian,MovingAverage,C0X,C0Y,C1X,C1Y,C2X,C2Y) \
                  VALUES(%1,?,%2,%3,%4,?,?,?,?,?,?) \
                  ;")
                  .arg(m_filesId)
                  .arg(lock_mass_ppm_tolerance)
                  .arg(m_options.lock_mass_moving_median_width)
                  .arg(m_options.lock_mass_moving_average_width);

    *createTableCmd = "CREATE TABLE IF NOT EXISTS MzCalibration \
                        (Id INTEGER PRIMARY KEY, FilesId INT, LockMassList TEXT \
                        ,PPMTolerance TEXT, MovingMedian TEXT, MovingAverage TEXT \
                        ,C0X BLOB, C0Y BLOB \
                        ,C1X BLOB, C1Y BLOB \
                        ,C2X BLOB, C2Y BLOB) \
                        ;";

    *deleteFilesCmd = QString("DELETE FROM MzCalibration WHERE FilesId = %1").arg(m_filesId);
}

Err MzCalibration::writeToDB(QSqlDatabase &db) const
{
    Err e = kNoErr;
    std::vector< std::vector<double> > vec_cx, vec_cy;
    QVector<QByteArray> ba_cx, ba_cy;
    QString insertCmd;
    QString createTableCmd;
    QString deleteFilesCmd;
    _setupDBQueries(&insertCmd, &createTableCmd, &deleteFilesCmd);

    QSqlQuery q = makeQuery(db, true);
    e = QEXEC_CMD(q, createTableCmd); eee;

    if (m_filesId > 0) {
        e = QEXEC_CMD(q, deleteFilesCmd); eee;
    } else {
        debugMs() << "MzCalibration::writeToDB with m_filesId = " << m_filesId;
        e = kBadParameterError; eee;
    }

    e = QPREPARE(q, insertCmd); eee;

    if (m_coeffList.size() > 3) {
        std::cerr << "m_coeffList.size() is larger than three.  Not handling after 3 in list" << endl;
    }

    q.bindValue(0, m_options.lock_mass_list.c_str());
    vec_cx.resize(3); vec_cy.resize(3);
    ba_cx.resize(3); ba_cy.resize(3);
    for (int i = 0; i < 3; i++) {
        if (i < m_coeffList.size()) {
            pointsToByteArray_DoubleDouble_careful(m_coeffList[i].getPointList(), vec_cx[i], vec_cy[i], ba_cx[i], ba_cy[i]);
            q.bindValue(1 + i * 2, ba_cx[i]);
            q.bindValue(1 + i * 2 + 1, ba_cy[i]);
        }
        else {
            q.bindValue(1 + i * 2, QVariant());
            q.bindValue(1 + i * 2 + 1, QVariant());
        }
    }
    e = QEXEC_NOARG(q); eee;

error:
    return e;
}

static Err sqlite3Exec(sqlite3 *db, const QString &sql)
{
    char *errmsg;
    int rc = sqlite3_exec(db, sql.toUtf8().constData(), 0, 0, &errmsg);
    if (rc != SQLITE_OK) {
        warningMs() << "sqlite3_exec(\"" << sql << "\"):" << QString::fromUtf8(errmsg);
        sqlite3_free(errmsg);
        return kSQLiteExecError;
    }
    return kNoErr;
}

Err MzCalibration::writeToDB(sqlite3 * db) const
{
    Err e = kNoErr;
    std::vector< std::vector<double> > vec_cx, vec_cy;
    QVector<QByteArray> ba_cx, ba_cy;
    QString insertCmd;
    QString createTableCmd;
    QString deleteFilesCmd;
    _setupDBQueries(&insertCmd, &createTableCmd, &deleteFilesCmd);

    e = sqlite3Exec(db, createTableCmd); eee;
    if (m_filesId > 0) {
        e = sqlite3Exec(db, deleteFilesCmd); eee;
    } else {
        debugMs() << "MzCalibration::writeToDB with m_filesId = " << m_filesId;
        e = kBadParameterError; eee;
    }
    sqlite3_stmt *statement = NULL;
    e = my_sqlite3_prepare(db, insertCmd, &statement); eee;

    if (m_coeffList.size() > 3) {
        std::cerr << "m_coeffList.size() is larger than three.  Not handling after 3 in list" << endl;
    }

    vec_cx.resize(3); vec_cy.resize(3);
    ba_cx.resize(3); ba_cy.resize(3);
    e = my_sqlite3_bind_text(db, statement, 1, QString::fromStdString(m_options.lock_mass_list)); eee;
    for (int i = 0; i < 3; i++) {
        if (i < m_coeffList.size()) {
            pointsToByteArray_DoubleDouble_careful(m_coeffList[i].getPointList(), vec_cx[i], vec_cy[i], ba_cx[i], ba_cy[i]);
            e = my_sqlite3_bind_blob(db, statement, 2 + i * 2, ba_cx[i], ba_cx[i].size(), MySqliteBindOption_TRANSIENT); eee;
            e = my_sqlite3_bind_blob(db, statement, 3 + i * 2, ba_cy[i], ba_cy[i].size(), MySqliteBindOption_TRANSIENT); eee;
        }
        else {
            e = my_sqlite3_bind_null(db, statement, 2 + i * 2); eee;
            e = my_sqlite3_bind_null(db, statement, 3 + i * 2); eee;
        }
    }
    e = my_sqlite3_step(db, statement, MySqliteStepOption_Reset); eee;

error:
    if (statement) {
        my_sqlite3_finalize(statement, db);
    }
    return e;
}

Err MzCalibration::computeCoefficientsFromByspec(QString filename)
{
    Err e = kNoErr;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(kQSQLITE, "compute_mz_lockmass");
        db.setDatabaseName(filename);
        if (!db.open()) {
            warningMs() << "Could not open file:" << filename << endl;
            warningMs() << db.lastError().text();
            e = kBadParameterError; eee;
        }
        e = computeCoefficientsFromByspecDB(db); eee;
    }

error:
    QSqlDatabase::removeDatabase("compute_mz_lockmass");
    return e;
}

Err applyCoefficientsToByspec(const QString &filename,
                              const CacheFileManagerInterface &cacheFileManager, int filesId)
{
    Err e = kNoErr;

    {
        MzCalibration mzCal(cacheFileManager);

        QSqlDatabase db = QSqlDatabase::addDatabase(kQSQLITE, "apply_mz_lockmass");
        db.setDatabaseName(filename);
        if (!db.open()) {
            warningMs() << "Could not open file:" << filename << endl;
            warningMs() << db.lastError().text();
            e = kBadParameterError; eee;
        }
        e = mzCal.readFromDB(db, filesId); eee;

        QSqlQuery q_select = makeQuery(db, true);
        QSqlQuery q_update = makeQuery(db, true);

        //Adjust all blobs
        //Adjust all observed m/z

        e = QEXEC_CMD(q_select, "SELECT Id, ObservedMz, RetentionTime, MetaText FROM Spectra"); eee;
        e = QPREPARE(q_update, "UPDATE Spectra SET ObservedMz=? WHERE Id = ?"); eee;

        transaction_begin(db);
        while (q_select.next()) {
            qlonglong id = q_select.value(0).toLongLong();
            QVariant obsMzVar = q_select.value(1);
            QVariant retTimeVar = q_select.value(2);
            QString metaText = q_select.value(3).toString();
            bool ok = false;
            double obsmz = obsMzVar.toDouble(&ok);
            double retTime = retTimeVar.toDouble();
            if (metaText.contains("<RetentionTimeUnit>second</RetentionTimeUnit>") ||
                metaText.contains("<RetentionTimeUnit>seconds</RetentionTimeUnit>"))
            {
                retTime = retTime / 60.0;
            }
            if (ok) {
                double cali_obsmz = mzCal.calibrate(obsmz, retTime);
                q_update.bindValue(0, cali_obsmz);
                q_update.bindValue(1, id);
                e = QEXEC_NOARG(q_update); eee;
            }
        }

        if (1) {
            PlotBase plot_scan;
            std::vector<double> xlist;
            std::vector<float> ylist;
            QByteArray xba, yba;

            e = QEXEC_CMD(q_select,
                "SELECT k.Id, PeaksMz, PeaksIntensity, s.RetentionTime, s.MetaText \
                FROM Peaks k \
                LEFT JOIN Spectra s ON s.PeaksId = k.Id;"); eee;

            e = QPREPARE(q_update, "UPDATE Peaks SET PeaksMz=?, PeaksIntensity=? WHERE Id = ?"); eee;


            while (q_select.next()) {
                qlonglong peaksId = q_select.value(0).toLongLong();
                double retTime = q_select.value(3).toDouble();
                QString metaText = q_select.value(4).toString();
                if (metaText.contains("<RetentionTimeUnit>second</RetentionTimeUnit>") ||
                    metaText.contains("<RetentionTimeUnit>seconds</RetentionTimeUnit>"))
                {
                    retTime = retTime / 60.0;
                }

                e = byteArrayToPlotBase(q_select.value(1).toByteArray(), q_select.value(2).toByteArray(), &plot_scan, true); eee;

                mzCal.calibrate(retTime, plot_scan);

                pointsToByteArray_DoubleFloat_careful(plot_scan.getPointList(), xlist, ylist, xba, yba);
                q_update.bindValue(0, xba);
                q_update.bindValue(1, yba);
                q_update.bindValue(2, peaksId);
                e = QEXEC_NOARG(q_update); eee;
            }
        }
        transaction_end(db);
    }

error:
    QSqlDatabase::removeDatabase("apply_mz_lockmass");
    return e;
}

Err MzCalibration::_findCalibrationScansFilter(QSqlDatabase &db, QString * outFilterString) const
{
    Err e = kNoErr;
    const QString lockmass_scans_filter_default = "s.MetaText LIKE '%<Calibration>dynamic</Calibration>%'";
    *outFilterString = lockmass_scans_filter_default;

    QString select_cmd = QString("SELECT COUNT(Id) FROM Spectra WHERE %1").arg(lockmass_scans_filter_default);

    QSqlQuery q = makeQuery(db, true);
    e = QEXEC_CMD(q, select_cmd); eee;
    if (!q.next()) {
        e = kBadParameterError; ree;
    }
    int count = q.value(0).toInt();
    //If no calibration file, it must be not Waters (or none to begin with).
    //Let's try and search for calibration on MS1 scans (e.g. Bruker)
    if (count <= 0) {
        *outFilterString = "s.MSLevel = 1";
    }

error:
    return e;
}

#ifdef PMI_MS_MZCALIBRATION_USE_MSREADER_TO_READ
Err MzCalibration::computeCoefficientsFromScanList(MSReader * ms, const QList<msreader::ScanInfoWrapper> & _list)
{
    Err e = kNoErr;

    clearCoefficients();

    PlotBase plot_recal, plot_recal_median, plot_recal_average;
    QList<double> massList = m_options.lockMassList();
    double found_peak_ratio = 0;

    if (massList.size() <= 0) {
        cerr << "no mass list found " << endl;
        return e;
    }

    PlotBase plot_scan;
    const double lockMassMz = massList.first();
    const double ppmTolerance = m_options.ppmTolerance().toDouble();
    double mzTolerance = lockMassMz / 1000000 * ppmTolerance;
    double minMz = lockMassMz - mzTolerance;
    double maxMz = lockMassMz + mzTolerance;
    const QList<msreader::ScanInfoWrapper> list = _list;
    int list_increment = 1;

    //To do this properly, we should change dither to make it fit to the given tick count size.  But it's not a requirement.
    //Note: we use tick_scale to make sure we don't try and change resolution for size of 101 when lock_mass_desired_time_tick_count is 100.
    static double tick_scale = 1.6;
    if ( (m_options.lock_mass_desired_time_tick_count > 0)
          && (list.size() > (int)m_options.lock_mass_desired_time_tick_count * tick_scale) )
    {
        list_increment = list.size() / m_options.lock_mass_desired_time_tick_count;
        if (list_increment <= 0) list_increment = 1;
    }
    int found_peak_count = 0, total_peak_count = 0;
    for (int i = 0; i < list.size(); i += list_increment) {
        const msreader::ScanInfoWrapper & obj = list[i];

        plot_scan.clear();
        double scanTime = obj.scanInfo.retTimeMinutes;
        point2d lockMassPoint;
        point2dList plist;

        bool do_centroding = true;
        e = ms->getScanData(obj.scanNumber, &plot_scan.getPointList(), do_centroding, NULL); eee;
        plist = plot_scan.getPointsBetween(minMz, maxMz, true);

        total_peak_count++;
        if (plist.size() > 0 ) {
            found_peak_count++;
            //Note: these are already centroided peaks.  This will still work if it's in profile, but probably we want properly centroied points here.
            point2d time_diff;
            lockMassPoint = largest(plist);
            time_diff.rx() = scanTime;
            time_diff.ry() = lockMassPoint.x() - lockMassMz;
            plot_recal.addPoint(time_diff);
        }
    }
    if (total_peak_count > 0) {
        found_peak_ratio = found_peak_count / (double) total_peak_count;
    } else {
        found_peak_ratio = 0;
    }

    if (found_peak_ratio > 0.6 && total_peak_count > 5) {
        e = medianFilter(plot_recal, m_options.lock_mass_moving_average_width, plot_recal_median); eee;
        e = computeMovingAverage(plot_recal_median, m_options.lock_mass_moving_median_width, plot_recal_average); eee;
        m_coeffList.push_back(plot_recal_average); eee;

        if (m_options.lock_mass_debug_output) {
            QList<PlotBase> list;
            list.push_back(plot_recal);
            list.push_back(plot_recal_median);
            list.push_back(plot_recal_average);
            QStringList titles;

            titles << "time" << "extracted_mz" << "median_filtered" << "average_filters"
                << "lock_mass_list" << m_options.lock_mass_list.c_str()
                << "lock_mass_ppm_tolerance" << m_options.lock_mass_ppm_tolerance.c_str()
                << "lock_mass_moving_median_width" << QString::number(m_options.lock_mass_moving_median_width)
                << "lock_mass_moving_average_width" << QString::number(m_options.lock_mass_moving_average_width)
                ;

            QString filename = "mass_error_stats.csv";
            e = printDebugCal_same_length(filename, list, titles); eee_absorb;
        }

        _extract_first_lockmass();
    } else {
        debugMs() << "Not enough lock mass values found.  No calibration done.  found_peak_count,total_peak_count,found_peak_ratio = " << found_peak_count << total_peak_count << found_peak_ratio;
    }

error:
    return e;
}
#endif //PMI_MS_MZCALIBRATION_USE_MSREADER_TO_READ

//This is used in byonic_solutions
Err MzCalibration::computeCoefficientsFromByspecDB(QSqlDatabase &db)
{
    Err e = kNoErr;

    if (m_cacheFileManager == nullptr) {
        rrr(kError);
    }

    clearCoefficients();

    //Just handle Waters with "<Calibration>dynamic</Calibration>" in them
    QSqlQuery q = makeQuery(db, true);
    QString cmd = "\
        SELECT k.PeaksMz, k.PeaksIntensity, s.RetentionTime, s.MetaText, s.FilesId, s.ScanNumber \
        FROM Peaks k \
        JOIN Spectra s ON s.PeaksId = k.Id \
        WHERE [lock_mass_scan_filter] \
        ORDER BY s.FilesId, s.RetentionTime \
        ;";
    PlotBase plot_recal, plot_recal_median, plot_recal_average;
    QList<double> massList = m_options.lockMassList();
    QString lock_mass_scan_filter = m_options.lock_mass_scan_filter.c_str();
    QString lockmass_scans_filter_default;
    int found_peak_count = 0, total_peak_count = 0;
    double found_peak_ratio = 0;


    if (massList.size() <= 0) {
        cerr << "no mass list found " << endl;
        return e;
    }

    e = _findCalibrationScansFilter(db, &lockmass_scans_filter_default);

    if (lock_mass_scan_filter.trimmed().length() > 0) {
        cmd = cmd.replace("[lock_mass_scan_filter]", lock_mass_scan_filter.trimmed());
    }
    else {
        cmd = cmd.replace("[lock_mass_scan_filter]", lockmass_scans_filter_default);
    }

#ifdef PMI_MS_MZCALIBRATION_USE_MSREADER_TO_READ
    const QScopedPointer<CacheFileManager> cacheFileManager(new CacheFileManager());

    cacheFileManager->setSearchPaths(m_cacheFileManager->searchPaths());
    cacheFileManager->setSavePath(m_cacheFileManager->savePath());
    cacheFileManager->setSourcePath(db.databaseName());

    msreader::MSReaderByspec ms_byspec(*cacheFileManager.data());

    e = ms_byspec.indirectOpen_becareful(db); eee;
#endif

    {
        PlotBase plot_scan;
        const double lockMassMz = massList.first();
        const double ppmTolerance = m_options.ppmTolerance().toDouble();
        double mzTolerance = lockMassMz / 1000000 * ppmTolerance;
        double minMz = lockMassMz - mzTolerance;
        double maxMz = lockMassMz + mzTolerance;

        e = QEXEC_CMD(q, cmd); eee;
        while (q.next()) {
            plot_scan.clear();
            double scanTime = q.value(2).toDouble();
            point2d lockMassPoint;
            QString metaText = q.value(3).toString();
            QVariant filesId = q.value(4);
            bool ok = false;
            if (filesId.toInt(&ok)) {
                if (m_filesId < 0) {
                    m_filesId = filesId.toInt();
                }
                else {
                    if (m_filesId != filesId.toInt()) {
                        cerr << "Current implementation cannot handle more than one MS file.  There appears to be more than one for the given file." << endl;
                        e = kBadParameterError; eee;
                    }
                }
            }
            else {
                static int count = 0;
                if (count++ < 5) {
                    cerr << "warning, filesId not found for this given byspec file" << endl;
                }
            }
            total_peak_count++;
            point2dList plist;
            if (metaText.contains("<RetentionTimeUnit>second</RetentionTimeUnit>") ||
                metaText.contains("<RetentionTimeUnit>seconds</RetentionTimeUnit>"))
            {
                scanTime = scanTime / 60.0;
            }
#ifdef PMI_MS_MZCALIBRATION_USE_MSREADER_TO_READ
            bool do_centroding = false;
            msreader::ScanInfo scanInfo;
            int scanNumber = q.value(5).toInt();
            e = ms_byspec.getScanData(scanNumber, &plot_scan.getPointList(), do_centroding, NULL); eee;
            e = ms_byspec.getScanInfo(scanNumber, &scanInfo); eee;
            if (scanInfo.peakMode == msreader::PeakPickingProfile){
                PlotBase centroidPlot;
                e = gaussianSmooth(plot_scan, 0.03); eee;
                plot_scan.makeCentroidedPoints(&centroidPlot.getPointList());
                plot_scan = centroidPlot;
            }
#else
            e = byteArrayToPlotBase(q.value(0).toByteArray(), q.value(1).toByteArray(), &plot_scan, true); eee;
#endif

            plist = plot_scan.getPointsBetween(minMz, maxMz, true);

            if (plist.size() > 0 ) {
                found_peak_count++;
                //Note: these are already centroided peaks.  This will still work if it's in profile, but probably we want properly centroied points here.
                point2d time_diff;
                lockMassPoint = largest(plist);
                time_diff.rx() = scanTime;
                time_diff.ry() = lockMassPoint.x() - lockMassMz;
                plot_recal.addPoint(time_diff);
            }
        }
        if (total_peak_count > 0) {
            found_peak_ratio = found_peak_count / (double) total_peak_count;
        } else {
            found_peak_ratio = 0;
        }

    }

    if (found_peak_ratio > 0.6 && total_peak_count > 5) {
        e = medianFilter(plot_recal, m_options.lock_mass_moving_average_width, plot_recal_median); eee;
        e = computeMovingAverage(plot_recal_median, m_options.lock_mass_moving_median_width, plot_recal_average); eee;
        m_coeffList.push_back(plot_recal_average); eee;

        if (m_options.lock_mass_debug_output) {
            QList<PlotBase> list;
            list.push_back(plot_recal);
            list.push_back(plot_recal_median);
            list.push_back(plot_recal_average);
            QStringList titles;

            titles << "time" << "extracted_mz" << "median_filtered" << "average_filters"
                << "lock_mass_list" << m_options.lock_mass_list.c_str()
                << "lock_mass_ppm_tolerance" << m_options.lock_mass_ppm_tolerance.c_str()
                << "lock_mass_moving_median_width" << QString::number(m_options.lock_mass_moving_median_width)
                << "lock_mass_moving_average_width" << QString::number(m_options.lock_mass_moving_average_width)
                ;

            QString filename = "mass_error_stats.csv";
            e = QEXEC_CMD(q, "SELECT Filename FROM Files"); eee;
            if (q.next()) {
                filename = q.value(0).toString();
                if (filename.trimmed().size() > 0) {
                    if (filename.endsWith("\\") || filename.endsWith("/")) {
                        filename.chop(1);
                    }
                    filename = QFileInfo(filename).fileName();
                    filename = filename.trimmed() + ".mass_error.csv";
                }
            }
            e = printDebugCal_same_length(filename, list, titles); eee_absorb;
        }

        e = writeToDB(db); eee;
        _decimateCoefficientPlotSize(50);
        _extract_first_lockmass();
    } else {
        debugMs() << "Not enough lock mass values found.  No calibration done.  found_peak_count,total_peak_count,found_peak_ratio = " << found_peak_count << total_peak_count << found_peak_ratio;
    }

error:
    return e;
}

//Reduce the number of points to make faster acesss
Err MzCalibration::_decimateCoefficientPlotSize(int expected_reduced_number_of_points)
{
    Err e = kNoErr;
    for (int i = 0; i < m_coeffList.size(); i++) {
        PlotBase & plot = m_coeffList[i];
        if (plot.getPointListSize() > expected_reduced_number_of_points) {
            PlotBase tmp;
            e = plot.makeResampledPlotTargetSize(expected_reduced_number_of_points, tmp); eee;
            cout << "decimating calibration plot from " << plot.getPointListSize() << " to " << tmp.getPointListSize() << endl;
            plot = tmp;
        }
    }
error:
    return e;
}


void MzCalibration::clearCoefficients()
{
    m_coeffList.clear();
    m_filesId = -1;
}


void MzCalibration::setupPreviewCoeffs(double m00, double m11, double m22)
{
    clearCoefficients();

    m_coeffList.push_back(PlotBase());
    m_coeffList.push_back(PlotBase());
    m_coeffList.push_back(PlotBase());

    m_coeffList[0].addPoint(point2d(0, -m00));
    m_coeffList[1].addPoint(point2d(0, -m11));
    m_coeffList[2].addPoint(point2d(0, -m22));
}


///////////////////////////////////////////////////////
//
// MzCalibrationManager class
//
///////////////////////////////////////////////////////

MzCalibrationManager::MzCalibrationManager(const CacheFileManagerInterface &cacheFileManager)
    : m_cacheFileManager(&cacheFileManager)
{
}

/*!
 * \brief Removes extra '/' or '\' of paths. E.g. C:\data\bla.raw\ --> C:\data\bla.raw
 * \param path
 * \return
 */
inline QString removeSlashAtEnd(const QString & path)
{
    QString str = path.trimmed();
    if (str.endsWith("/") || str.endsWith("\\")) {
        str.chop(1);
    }
    return str;
}

Err MzCalibrationManager::loadFromDatabase(QSqlDatabase & db)
{
    Err e = kNoErr;
    QString columnName_filename = "Name";  //Byologic and Byomap have different column name for Files.
    QStringList colNameList, tableNameList;
    QString command;

    QSqlQuery q = makeQuery(db, true);

    e = GetTableColumnNames(db, "Files", colNameList); eee;
    if (colNameList.contains("Filename")) {
        columnName_filename = "Filename";
    }
    e = GetSQLiteTableNames(db, tableNameList); eee;
    if (!tableNameList.contains(kMzCalibration)) {
        debugMs() << "No MzCalibration found. No calibration to load.";
        return e;
    }


    command = QString(
        "SELECT z.FilesId, f.%1 "
        "FROM MzCalibration z "
        "JOIN Files f ON f.Id = z.FilesId")
        .arg(columnName_filename);

    e = QEXEC_CMD(q, command); eee;

    //TODO: handle 'AltRawFilename' case. See getValidRawFilenames()

    while(q.next()) {
        MzCalibration mzCal(*m_cacheFileManager);
        int filesId = q.value(0).toInt();
        QString filename = q.value(1).toString();
        e = mzCal.readFromDB(db, filesId); eee;
        add(filename, mzCal);
    }

error:
    return e;
}

Err MzCalibrationManager::loadFromDatabase(const QString & filename)
{
    Err e = kNoErr;
    {
        QSqlDatabase db;
        e = addDatabaseAndOpen("MzCalibrationManager_loadFromDatabase", filename, db); eee;
        e = loadFromDatabase(db); eee;
    }

error:
    QSqlDatabase::removeDatabase("MzCalibrationManager_loadFromDatabase");
    return e;
}


Err MzCalibrationManager::saveToDatabase(QSqlDatabase & db) const
{
    Err e = kNoErr;
    foreach(const MzCalibration & mzcal, m_fileToCalibration) {
        e = mzcal.writeToDB(db); eee;
    }
error:
    return e;
}

Err MzCalibrationManager::saveToDatabase(sqlite3 * db) const
{
    Err e = kNoErr;
    foreach(const MzCalibration & mzcal, m_fileToCalibration) {
        e = mzcal.writeToDB(db); eee;
    }
error:
    return e;
}

MzCalibrationManager::const_iter
MzCalibrationManager::get(QString _filename, bool * found) const
{
    QString filename = removeSlashAtEnd(_filename);
    const_iter iter = m_fileToCalibration.constFind(filename);
    if (iter != m_fileToCalibration.constEnd()) {
        *found = true;
    } else {
        *found = false;
    }
    return iter;
}

void MzCalibrationManager::add(const QString & _filename, const MzCalibration & obj)
{
    QString filename = removeSlashAtEnd(_filename);
    if (m_fileToCalibration.contains(filename)) {
        debugMs() << "Note that file has already been added to MzCalibrationManager. Replacing existing one. File=" << filename;
    }
    m_fileToCalibration[filename] = obj;
}

/////////////////////////////////

inline Err findByspec(QSqlQuery & q_queries, QString * byspec_file)
{
    Err e = kNoErr;
    QString byspec_filename, filetype;

    //Get the referenced byspec file
    e = QEXEC_CMD(q_queries, "SELECT Id,Filename,Type FROM Files"); eee;
    if (q_queries.next()) {
        byspec_filename = q_queries.value(1).toString();
        filetype = q_queries.value(2).toString();
        *byspec_file = byspec_filename;
    }
    else {
        e = kSQLiteMissingContentError; eee;
    }

error:
    return e;
}


Err read_lock_mass_from_byspec(QString filename, MzCalibration & precursor_calibration, MzCalibration & fragment_calibration)
{
    Err e = kNoErr;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(kQSQLITE, "extract_lock_mass_calibration");
        db.setDatabaseName(filename);
        if (!db.open()) {
            warningMs() << "Could not open file:" << filename << endl;
            warningMs() << db.lastError().text();
            e = kBadParameterError; eee;
        }
        QString byspec_filename;

        {
            QSqlQuery q = makeQuery(db, true); eee;
            e = findByspec(q, &byspec_filename); eee;
        }

        db.close();
        db.setDatabaseName(byspec_filename);
        if (!db.open()) {
            warningMs() << "Could not open file:" << byspec_filename << endl;
            warningMs() << db.lastError().text();
            e = kBadParameterError; eee;
        }

        //Note(2015/09/18, Yong): We assume we have the same fragmentation pattern for both ms1 and ms2, which is true for QTOF machines (Waters)
        e = precursor_calibration.readFromDB(db, 1); eee;
        e = fragment_calibration.readFromDB(db, 1); eee;
    }

error:
    QSqlDatabase::removeDatabase("extract_lock_mass_calibration");
    return e;
}


_PMI_END
