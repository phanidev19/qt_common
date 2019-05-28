/*
 * Copyright (C) 2015 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */


#include <MzCalibration.h>

#include <CacheFileManager.h>
#include <PMiTestUtils.h>

#include <pmi_core_defs.h>
#include <qt_string_utils.h>

#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <sqlite3.h>

namespace QTest {

template<typename T>
bool qCompare(const QList<T> &t1, const QStringList &t2, const char *actual,
                    const char *expected, const char *file, int line)
{
    QList<T> list2;
    for(const QString &s : t2) {
        list2 += QVariant(s).value<T>();
    }
    //qDebug() << QString::number(t1[0], 'g', 12) << t2 << QString::number(list2[0], 'g', 12);
    return QTest::qCompare(t1, list2, actual, expected, file, line);
}

//! To improve precision of printing
inline char* toString(const QPointF &p)
{
    return qstrdup(qPrintable(QString::fromLatin1("QPointF(%1,%2)")
            .arg(p.x(), 0, 'g', 16).arg(p.y(), 0, 'g', 16)));
}

bool qCompare(const QPointF &t1, const QStringList &t2, const char *actual,
                    const char *expected, const char *file, int line)
{
    if (t2.count() != 2) {
        return false;
    }
    bool ok1, ok2;
    const QPointF p2(t2[0].toDouble(&ok1), t2[1].toDouble(&ok2));
    return QTest::qCompare(t1, p2, actual, expected, file, line);
}

}

_PMI_BEGIN

class MzCalibrationTest : public QObject
{
    Q_OBJECT
public:
    explicit MzCalibrationTest(const QStringList &args);

private Q_SLOTS:
    void testReadFromDB();
    void testWriteToDBFile();
    void testWriteToDBSqlite();
private:
    void readFromDB(const QString &filename, MzCalibration *cal, bool *ok);

private:
    const QString m_fname;
    const bool m_isValid;
    const QScopedPointer<CacheFileManager> m_cacheFileManager;
};

MzCalibrationTest::MzCalibrationTest(const QStringList &args)
    : m_fname(args[0])
    , m_isValid(args[1] == "1")
    , m_cacheFileManager(new CacheFileManager())
{
    m_cacheFileManager->setSourcePath(m_fname);
}

void MzCalibrationTest::readFromDB(const QString &filename, MzCalibration *cal, bool *ok)
{
    *ok = false;
    QFileInfo fi(filename);
    QVERIFY2(fi.isReadable(), qPrintable(QString::fromLatin1("Reading file '%1'.").arg(filename)));
    Err result = cal->readFromDB(filename, 1 /*OK?*/);
    if (m_isValid) {
        QCOMPARE(result, kNoErr);
    } else {
        qDebug() << result;
        QVERIFY(result != kNoErr);
    }
    *ok = true;
}

void MzCalibrationTest::testReadFromDB()
{
    MzCalibration cal(*m_cacheFileManager.data());
    bool ok;
    readFromDB(m_fname, &cal, &ok);
    if (!ok) {
        return;
    }
#if 0 // TODO
    QString verificationFname(m_fname + ".expect");
    QSettings verify(verificationFname, QSettings::IniFormat);
    QVERIFY2(QSettings::NoError == verify.status(),
             qPrintable(QString::fromLatin1("Reading verification file '%1'.").arg(verificationFname)));
    const MzCalibrationOptions& options = cal.options();

#define COMPARE(field, convert1, convert2) \
        QCOMPARE(convert1(options. ## field), verify.value(# field).convert2())
    COMPARE(lock_mass_list, QString::fromStdString, toString);
    COMPARE(lock_mass_ppm_tolerance, QString::fromStdString, toString);
    COMPARE(lock_mass_scan_filter, QString::fromStdString, toString);
    COMPARE(lock_mass_moving_average_width, , toInt);
    COMPARE(lock_mass_moving_median_width, , toInt);
    COMPARE(lock_mass_debug_output, , toInt);
    COMPARE(lock_mass_use_ppm_for_one_point, , toInt);
    COMPARE(lock_mass_desired_time_tick_count, , toInt);
    COMPARE(lockMassList(), , value<QStringList>);
#undef COMPARE

    QCOMPARE(cal.coefficientPlots().count(), 1);
    const PlotBase plot(cal.coefficientPlots().first());
    QCOMPARE(plot.getPointList().size(), verify.value("points_count").toUInt());
    QCOMPARE(plot.getMinPoint(), verify.value("min_point").toStringList());
    QCOMPARE(plot.getMaxPoint(), verify.value("max_point").toStringList());
#endif
}

void MzCalibrationTest::testWriteToDBFile()
{
    if (!m_isValid) {
        QSKIP("Won't write invalid object");
    }
    MzCalibration cal(*m_cacheFileManager.data());
    bool ok;
    readFromDB(m_fname, &cal, &ok);
    if (!ok) {
        return;
    }
    QFileInfo fi(m_fname);
    const QString path(QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/MzCalibrationTest/"));
    QDir().mkdir(path);
    const QString outFilename = path + fi.baseName() + "-writeQtSqlTest." + fi.completeSuffix();
    QCOMPARE(cal.writeToDB(outFilename), kNoErr);
}

void MzCalibrationTest::testWriteToDBSqlite()
{
    if (!m_isValid) {
        QSKIP("Won't write invalid object");
    }
    MzCalibration cal(*m_cacheFileManager.data());
    bool ok;
    readFromDB(m_fname, &cal, &ok);
    if (!ok) {
        return;
    }
    QFileInfo fi(m_fname);
    const QString path(QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/MzCalibrationTest/"));
    QDir().mkdir(path);
    const QString outFilename = path + fi.baseName() + "-writeSqlite3Test." + fi.completeSuffix();
    sqlite3 *db;
    int rc = sqlite3_open(QFile::encodeName(outFilename).constData(), &db);
    if (rc != SQLITE_OK) {
        QString msg = QString::fromUtf8(sqlite3_errmsg(db));
        sqlite3_close(db);
        QFAIL(qPrintable(QString("sqlite3_open(): can't open database %1: %2").arg(outFilename).arg(msg)));
    }
    const Err writeResult = cal.writeToDB(db);
    if (writeResult != kNoErr) {
        sqlite3_close(db);
        QCOMPARE(writeResult, kNoErr);
    }
    QCOMPARE(sqlite3_close(db), SQLITE_OK);
}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::MzCalibrationTest, QStringList() << "file.byspec2" << "valid[0|1]")

#include "MzCalibrationTest.moc"
