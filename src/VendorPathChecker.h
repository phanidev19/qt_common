#ifndef VENDORPATHCHECKER_H
#define VENDORPATHCHECKER_H

#include <pmi_common_ms_export.h>
#include <pmi_core_defs.h>
#include <QObject>
#include <QString>

_PMI_BEGIN

class PMI_COMMON_MS_EXPORT VendorPathChecker
{
public:
    enum ReaderBrukerFormat {
        ReaderBrukerFormatUnknown = 0,
        ReaderBrukerFormatFID,
        ReaderBrukerFormatYEP,
        ReaderBrukerFormatBAF,
        ReaderBrukerFormatU2,
        ReaderBrukerFormatBAF_and_U2,
        ReaderBrukerFormatTims,
    };

    enum ReaderAgilentFormat {
        ReaderAgilentFormatUnknown = 0,
        ReaderAgilentFormatAcqData,
    };

    enum ReaderWatersFormat {
        ReaderWatersFormatUnknown = 0,
        ReaderWatersFormatValid,
    };

    enum ReaderShimadzuFormat {
        ReaderShimadzuFormatUnknown = 0,
        ReaderShimadzuFormatValid,
    };

    enum ReaderAbiFormat {
        ReaderAbiFormatUnknown = 0,
        ReaderAbiFormatValid,
    };

    static ReaderAgilentFormat formatAgilent(const QString &filename);
    static ReaderBrukerFormat formatBruker(const QString &path);
    static ReaderWatersFormat formatWaters(const QString &path);
    static ReaderShimadzuFormat formatShimadzu(const QString &path);
    static ReaderAbiFormat formatAbi(const QString &path);

    enum CheckMethod {
        // fast and dumb
        CheckMethodStringOnlyAndFast,
        // slow, but correct by checking file system
        CheckMethodCheckPathExists
    };

    static bool isVendorPath(const QString &path, CheckMethod method);
    static bool isMassSpectrumPath(const QString &path, CheckMethod method);
    static bool isThermoPath(const QString &path, CheckMethod method);

    static bool isBrukerAnalysisBaf(const QString &path);

    // TODO: move vendor checker to VendorPathChecker

    static bool isBrukerPath(const QString &path);

    // TODO: make distinction between bruker and agilent
    static bool isAgilentPath(const QString &path);

    static bool isWatersFile(const QString &path);

    static bool isABSCIEXFile(const QString &path);

    static bool isThermoFile(const QString &path);

    static bool isShimadzuFile(const QString &path);

    static bool isVendorFile(const QString &path);
};

_PMI_END

#endif // VENDORPATHCHECKER_H
