#include "VendorPathChecker.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

_PMI_BEGIN

const QLatin1String WATERS_EXTENSION("raw");
const QLatin1String BYSPEC2_EXTENSION("byspec2");
const QLatin1String SHIMADZU_EXTENSION("lcd");
const QLatin1String ABI_EXTENSION("wiff");

static bool checkIfDirectory(const QString &filename)
{
    return QFileInfo(filename).isDir();
}

static bool checkIfDirectoryExist(const QString &filename)
{
    return QDir(filename).exists();
}

static bool checkIfFileExist(const QString &filename)
{
    if (filename.isEmpty())
        return false;

    QFileInfo fileInfo(filename);
    return fileInfo.isFile() && fileInfo.exists();
}

static bool checkExtension(const QString &file, const QString &extension)
{
    return QFileInfo(file).suffix().toLower() == extension;
}

static QString prepareU2FilePath(const QString &string)
{
    QString newValue(string);
    QString sourceDirectory(QDir(newValue).dirName());

    if (sourceDirectory.isEmpty() || !sourceDirectory.endsWith(".d", Qt::CaseInsensitive))
        return QString();

    sourceDirectory.replace(".d", ".u2");
    newValue.append(QString("/%1").arg(sourceDirectory));

    return newValue;
}

VendorPathChecker::ReaderAgilentFormat VendorPathChecker::formatAgilent(const QString &filename)
{
    if (checkIfDirectory(filename)) {
        QDirIterator it(filename, QDirIterator::FollowSymlinks);

        while (it.hasNext()) {
            QString next = it.next();

            if (next.contains("/AcqData", Qt::CaseInsensitive))
                return ReaderAgilentFormatAcqData;
        }
    } else if (checkIfFileExist(filename)
               && (filename.contains(".d/AcqData/mspeak.bin", Qt::CaseInsensitive)
                   || filename.contains(".d/AcqData/msprofile.bin", Qt::CaseInsensitive))) {
        return ReaderAgilentFormatAcqData;
    }
    return ReaderAgilentFormatUnknown;
}

VendorPathChecker::ReaderBrukerFormat VendorPathChecker::formatBruker(const QString &path)
{
    QString sourcePath(path);

    // Make sure target "path" is actually a directory since
    // all Bruker formats are directory-based
    if (!checkIfDirectory(sourcePath)) {
        // No needed to check if file not exist.
        if (!checkIfFileExist(sourcePath))
            return ReaderBrukerFormatUnknown;

        QFileInfo fileInfo(sourcePath);
        // Special cases for identifying direct paths to fid/Analysis.yep/Analysis.baf/.U2
        // Note that direct paths to baf or u2 will fail to find a baf/u2 hybrid source
        QString leaf = fileInfo.fileName();
        QString extension = fileInfo.suffix();
        leaf = leaf.toLower();

        if (leaf == "fid"
            && !checkIfFileExist(QString(fileInfo.absolutePath()).append("/analysis.baf")))
            return ReaderBrukerFormatFID;
        else if (extension == ".u2")
            return ReaderBrukerFormatU2;
        else if (leaf == "analysis.yep")
            return ReaderBrukerFormatYEP;
        else if (leaf == "analysis.baf")
            return ReaderBrukerFormatBAF;
        else
            return ReaderBrukerFormatUnknown;
    }

    // TODO: 1SRef is not the only possible substring below, get more examples!

    // Check for fid-based data;
    // Every directory within the queried directory should have a "1/1SRef"
    // subdirectory with a fid file in it, but we check only the first non-dotted
    // directory for efficiency. This can fail, but those failures are acceptable.
    // Alternatively, a directory closer to the fid file can be identified.
    // Caveat: BAF files may be accompanied by a fid, skip these cases! (?)
    QDirIterator it(sourcePath, QDirIterator::FollowSymlinks);

    while (it.hasNext()) {

        QString next = it.next();

        if (next.endsWith("/.") || next.endsWith("/.."))
            continue;

        if (checkIfFileExist(QString(next).append("/1/1SRef/fid"))
            || checkIfFileExist(QString(next).append("/1SRef/fid"))
            || checkIfFileExist(QString(next).append("/1/1SLin/fid"))
            || checkIfFileExist(QString(next).append("/1SLin/fid"))
            || checkIfFileExist(QString(next).append("/1/1Ref/fid"))
            || checkIfFileExist(QString(next).append("/1Ref/fid"))
            || checkIfFileExist(QString(next).append("/1/1Lin/fid"))
            || checkIfFileExist(QString(next).append("/1Lin/fid"))
            || (checkIfDirectoryExist(QString(next).append("/fid"))
                && !checkIfFileExist(QString(next).append("/Analysis.baf"))
                && !checkIfFileExist(QString(next).append("/analysis.baf")))
            || (checkIfDirectoryExist(QString(sourcePath).append("/fid"))
                && !checkIfFileExist(QString(sourcePath).append("/Analysis.baf"))
                && !checkIfFileExist(QString(sourcePath).append("/analysis.baf"))))
            return ReaderBrukerFormatFID;
    }

    // Check for yep-based data;
    // The directory should have a file named "Analysis.yep"
    if (checkIfFileExist(QString(sourcePath).append("/Analysis.yep")))
        return ReaderBrukerFormatYEP;

    // Check for baf-based data;
    // The directory should have a file named "Analysis.baf"
    else if (checkIfFileExist(QString(sourcePath).append("/Analysis.baf"))) {

        // Check for baf/u2 hybrid data
        if (checkIfFileExist(prepareU2FilePath(sourcePath)))
            return ReaderBrukerFormatBAF_and_U2;
        else
            return ReaderBrukerFormatBAF;
    }
    // Check for u2-based data;
    // The directory should have a file named "<directory-name - ".d">.u2"
    else if (checkIfFileExist(prepareU2FilePath(sourcePath))) {
        return ReaderBrukerFormatU2;
    }
    // Check for tdf-based data;
    // The directory should have a file named "analysis.tdf"
    else if ((checkIfFileExist(QString(sourcePath).append("/analysis.tdf"))
              || checkIfFileExist(QString(sourcePath).append("/Analysis.tdf")))
             && (checkIfFileExist(QString(sourcePath).append("/analysis.tdf_bin"))
                 || checkIfFileExist(QString(sourcePath).append("/Analysis.tdf_bin")))) {
        return ReaderBrukerFormatTims;
    }
    return ReaderBrukerFormatUnknown;
}

// clean up bruker 'analysis.baf' file to folder before calling this function.
static bool isCompatibleVendorFileType(const QString &filename)
{
    static const QStringList suffixes = QStringList()
        << "raw"
        << "mzml"
        << "mzxml"
        << "d" << ABI_EXTENSION << BYSPEC2_EXTENSION << SHIMADZU_EXTENSION;
    return suffixes.contains(QFileInfo(filename).suffix().toLower());
}

// TODO(Ivan Skiba 2017-05-24): Waters format specs are required for additional validation
// improvements.
VendorPathChecker::ReaderWatersFormat VendorPathChecker::formatWaters(const QString &sourcePath)
{
    if (!checkExtension(sourcePath, WATERS_EXTENSION) || !checkIfDirectory(sourcePath)) {
        return ReaderWatersFormatUnknown;
    }

    const QLatin1String WATERS_DATA_EXTENSION("dat");
    QDirIterator it(sourcePath, QDir::Files);

    while (it.hasNext()) {
        if (checkExtension(it.next(), WATERS_DATA_EXTENSION)) {
            return ReaderWatersFormatValid;
        }
    }

    return ReaderWatersFormatUnknown;
}

VendorPathChecker::ReaderShimadzuFormat VendorPathChecker::formatShimadzu(const QString &path)
{
    return (checkExtension(path, SHIMADZU_EXTENSION) && checkIfFileExist(path))
        ? ReaderShimadzuFormatValid
        : ReaderShimadzuFormatUnknown;
}

VendorPathChecker::ReaderAbiFormat VendorPathChecker::formatAbi(const QString &path)
{
    // TODO: do better checks; also check for .wiff.scan
    return (checkExtension(path, ABI_EXTENSION) && checkIfFileExist(path))
        ? ReaderAbiFormatValid
        : ReaderAbiFormatUnknown;
}

static bool format_opensource(const QString &sourcePath)
{
    // TODO: do better checks; also check for .wiff.scan
    if ((sourcePath.endsWith(".mzml", Qt::CaseInsensitive)
         || sourcePath.endsWith(".mzxml", Qt::CaseInsensitive))
        && checkIfFileExist(sourcePath)) {
        return true;
    }
    return false;
}

static bool format_thermo(const QString &sourcePath, bool checkExistence)
{
    // TODO: do better checks; also check for .wiff.scan
    if (!sourcePath.endsWith(".raw", Qt::CaseInsensitive)) {
        return false;
    }

    if (checkExistence && !checkIfFileExist(sourcePath)) {
        return false;
    }

    return true;
}

bool VendorPathChecker::isVendorPath(const QString &path, CheckMethod method)
{
    // Check path by string only
    if (!isCompatibleVendorFileType(path)) {
        return false;
    }

    if (method == CheckMethodStringOnlyAndFast) {
        return true;
    }

    // CheckMethodCheckPathExists -- check for folder and paths
    if (!QFile(path).exists()) {
        return false;
    }

    if (format_opensource(path)) {
        return true;
    }

    if (formatAbi(path)) {
        return true;
    }

    if (format_thermo(path, method == CheckMethodCheckPathExists)) {
        return true;
    }

    if (formatShimadzu(path)) {
        return true;
    }

    if (VendorPathChecker::formatWaters(path) != ReaderWatersFormatUnknown) {
        return true;
    }

    // do slower checks later

    if (VendorPathChecker::formatAgilent(path) != ReaderAgilentFormatUnknown) {
        return true;
    }

    if (VendorPathChecker::formatBruker(path) != ReaderBrukerFormatUnknown) {
        return true;
    }

    return false;
}

static bool isCompatiblePmiFileType(const QString &file)
{
    return checkExtension(file, BYSPEC2_EXTENSION);
}

static bool format_byspec2(const QString &path)
{
    // TODO(Ivan Skiba 2017-05-23): improve byspec2 file type validation
    if (checkExtension(path, BYSPEC2_EXTENSION) && checkIfFileExist(path)) {
        return true;
    }

    return false;
}

bool VendorPathChecker::isMassSpectrumPath(const QString &path, CheckMethod method)
{
    if (isVendorPath(path, method)) {
        return true;
    }

    if (!isCompatiblePmiFileType(path)) {
        return false;
    }

    if (method == CheckMethodStringOnlyAndFast) {
        return true;
    }

    if (format_byspec2(path)) {
        return true;
    }

    return false;
}

bool VendorPathChecker::isThermoPath(const QString &path, VendorPathChecker::CheckMethod method)
{
    return format_thermo(path, method == CheckMethodCheckPathExists);
}

bool VendorPathChecker::isBrukerAnalysisBaf(const QString &path)
{
    QDir inputDirectory(path);
    if (inputDirectory.exists("analysis.baf")) {
        return true;
    }
    return false;
}

bool VendorPathChecker::isBrukerPath(const QString &path)
{
    if (path.endsWith(".d", Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

// TODO: make distinction between bruker and agilent

bool VendorPathChecker::isAgilentPath(const QString &path)
{
    return isBrukerPath(path);
}

bool VendorPathChecker::isWatersFile(const QString &path)
{
    QFileInfo info(path);
    if (info.isDir()) {
        if (path.endsWith(".raw", Qt::CaseInsensitive)) {
            if (QDir(path).exists("_FUNC001.DAT")) {
                return true;
            }
        }
    }

    return false;
}

bool VendorPathChecker::isABSCIEXFile(const QString &path)
{
    if (path.endsWith(".wiff", Qt::CaseInsensitive)) {
        return true;
    }

    return false;
}

bool VendorPathChecker::isThermoFile(const QString &path)
{
    QFileInfo info(path);
    if (info.isFile()) {
        if (path.endsWith(".raw", Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

bool VendorPathChecker::isShimadzuFile(const QString &path)
{
    return path.endsWith(".lcd", Qt::CaseInsensitive);
}

bool VendorPathChecker::isVendorFile(const QString &path)
{
    return (isThermoFile(path) || isWatersFile(path) || isBrukerPath(path) || isABSCIEXFile(path)
            || isAgilentPath(path) || isShimadzuFile(path));
}

_PMI_END
