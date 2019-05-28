#include <QString>
#include <QtTest>

#include <QMetaEnum>

#include "VendorPathChecker.h"

using namespace pmi;

#include "PMiTestUtils.h"

class VendorPathCheckerGadget : public VendorPathChecker
{
    Q_GADGET
    Q_ENUMS(ReaderBrukerFormat)
    Q_ENUMS(ReaderAgilentFormat)
    Q_ENUMS(ReaderWatersFormat)
};
Q_DECLARE_METATYPE(VendorPathCheckerGadget)


class VendorPathCheckerTest : public QObject
{
    Q_OBJECT

public:
    VendorPathCheckerTest(const QStringList &args);

private Q_SLOTS:
    void testBrukerYepValidFile1();
    void testBrukerInvalidFile1();
    void testBrukerInvalidFile2();
    void testBrukerFidValidFile1();
    void testBrukerBafValidFile1();
    void testBrukerFidValidFile2();
    void testBrukerInvalidFile3();
    void testBrukerYepValidFile2();
    void testBrukerBafValidFile2();
    void testBrukerFidValidFile3();
    void testAgilentInvalidFile1();
    void testAgilentInvalidFile2();
    void testAgilentInvalidFile3();
    void testAgilentAcqDataValidFile1();
    void testAgilentAcqDataValidFile2();
    void testAgilentInvalidFile4();
    void testAgilentAcqDataValidFile3();
    void testAgilentAcqDataValidFile4();
    void testWatersValidFile1();
    void testWatersInvalidFile1();
    void testWatersInvalidFile2();
    void testWatersInvalidFile3();
    void testWatersInvalidFile4();
    void testWatersInvalidFile5();
    void testWatersInvalidFile6();
    void testWatersValidFile2();
    void testShimadzuValidFile1();
    void testShimadzuInvalidFile0();
    void testShimadzuInvalidFile1();
    void testShimadzuInvalidFile2();
    void testAbiValidFile1();
    void testAbiInvalidFile0();
    void testAbiInvalidFile1();
    void testAbiInvalidFile2();

private:
    const QString m_dataFolderPath = QFile::decodeName(PMI_TEST_FILES_DATA_DIR) + QLatin1String("/VendorPathCheckerTest/");
    const QString m_remoteDataPath = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR)
            + QLatin1String("/VendorPathCheckerTest");
};

template<typename EnumType>
static QString enumToString(const QMetaObject &mo, const char *enumName, EnumType enumValue)
{
    int index = mo.indexOfEnumerator(enumName);
    QMetaEnum metaEnum = mo.enumerator(index);

    return metaEnum.valueToKey(enumValue);
}

template<typename EnumType>
static QString getEnumName(EnumType (functionPointer)(const QString &))
{
    void *inputFunctionPtr = reinterpret_cast<void*>(functionPointer);
    void *formatAgilentPtr = reinterpret_cast<void*>(&VendorPathChecker::formatAgilent);
    void *formatBrukerPtr = reinterpret_cast<void*>(&VendorPathChecker::formatBruker);
    void *formatWatersPtr = reinterpret_cast<void *>(&VendorPathChecker::formatWaters);

    if(inputFunctionPtr == formatAgilentPtr)
        return "ReaderAgilentFormat";
    else if(inputFunctionPtr == formatBrukerPtr)
        return "ReaderBrukerFormat";
    else if (inputFunctionPtr == formatWatersPtr) {
        return QLatin1String("ReaderWatersFormat");
    }

    return QString();
}

template<typename EnumType>
static EnumType checkAndShowTestResult(const QString &path,
                                       EnumType (functionPointer)(const QString &))
{
    qDebug()<<"Checking path: "<<path;
    EnumType result = functionPointer(path);
    qDebug() << "Test function: "
             << VendorPathCheckerGadget::isVendorPath(
                    path, VendorPathCheckerGadget::CheckMethodCheckPathExists);
    qDebug() << "Test result: "
             << enumToString(VendorPathCheckerGadget::staticMetaObject,
                             getEnumName(functionPointer).toLocal8Bit(), result);
    return result;
}

VendorPathCheckerTest::VendorPathCheckerTest(const QStringList &args)
{

}

void VendorPathCheckerTest::testBrukerYepValidFile1()
{
    const QString path = m_remoteDataPath + "/Sample_1-A.1_01_985.d";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatBruker)
            == VendorPathChecker::ReaderBrukerFormatYEP);
}

void VendorPathCheckerTest::testBrukerInvalidFile1()
{
    const QString path = m_remoteDataPath + "/reader_bruker_test.data";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatBruker)
            == VendorPathChecker::ReaderBrukerFormatUnknown);
}

void VendorPathCheckerTest::testBrukerInvalidFile2()
{
    const QString path = m_remoteDataPath + "/Reader_ABI_Test.data";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatBruker)
            == VendorPathChecker::ReaderBrukerFormatUnknown);
}

void VendorPathCheckerTest::testBrukerFidValidFile1()
{
    const QString path = m_dataFolderPath + "reader_bruker_test.data/100 fmol BSA";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatBruker)
            == VendorPathChecker::ReaderBrukerFormatFID);
}

void VendorPathCheckerTest::testBrukerBafValidFile1()
{
    const QString path = m_remoteDataPath + "/CsI_Pos_0_G1_000003.d";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatBruker)
            == VendorPathChecker::ReaderBrukerFormatBAF);
}

void VendorPathCheckerTest::testBrukerFidValidFile2()
{
    const QString path = m_dataFolderPath + "reader_bruker_test.data/100 fmol BSA/0_B1";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatBruker)
            == VendorPathChecker::ReaderBrukerFormatFID);
}

void VendorPathCheckerTest::testBrukerInvalidFile3()
{
    QVERIFY(checkAndShowTestResult(m_remoteDataPath, &VendorPathChecker::formatBruker)
            == VendorPathChecker::ReaderBrukerFormatUnknown);
}

void VendorPathCheckerTest::testBrukerYepValidFile2()
{
    const QString path = m_remoteDataPath + "/Sample_1-A.1_01_985.d/analysis.yep";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatBruker)
            == VendorPathChecker::ReaderBrukerFormatYEP);
}

void VendorPathCheckerTest::testBrukerBafValidFile2()
{
    const QString path = m_remoteDataPath + "/CsI_Pos_0_G1_000003.d/analysis.baf";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatBruker)
            == VendorPathChecker::ReaderBrukerFormatBAF);
}

void VendorPathCheckerTest::testBrukerFidValidFile3()
{
    const QString path = m_dataFolderPath + "/Reader_Bruker_Test.data/100 fmol BSA/0_B1/1/1SRef/fid";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatBruker)
            == VendorPathChecker::ReaderBrukerFormatFID);
}

void VendorPathCheckerTest::testAgilentInvalidFile1()
{
    const QString path = m_dataFolderPath + "Reader_Thermo_Test.data";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatAgilent)
            == VendorPathChecker::ReaderAgilentFormatUnknown);
}

void VendorPathCheckerTest::testAgilentInvalidFile2()
{
    QVERIFY(checkAndShowTestResult(m_dataFolderPath, &VendorPathChecker::formatAgilent)
            == VendorPathChecker::ReaderAgilentFormatUnknown);
}

void VendorPathCheckerTest::testAgilentInvalidFile3()
{
    const QString path = m_dataFolderPath + "reader_agilent_test.data";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatAgilent)
            == VendorPathChecker::ReaderAgilentFormatUnknown);
}

void VendorPathCheckerTest::testAgilentAcqDataValidFile1()
{
    const QString path = m_dataFolderPath + "reader_agilent_test.data/gfb_4scan_timesegs_1530_100ng.d/acqdata/"
                                   "MSPeak.bin";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatAgilent)
            == VendorPathChecker::ReaderAgilentFormatAcqData);
}

void VendorPathCheckerTest::testAgilentAcqDataValidFile2()
{
    const QString path = m_dataFolderPath + "reader_agilent_test.data/gfb_4scan_timesegs_1530_100ng.d/acqdata";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatAgilent)
            == VendorPathChecker::ReaderAgilentFormatAcqData);
}

void VendorPathCheckerTest::testAgilentInvalidFile4()
{
    const QString path = "C:/";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatAgilent)
            == VendorPathChecker::ReaderAgilentFormatUnknown);
}

void VendorPathCheckerTest::testAgilentAcqDataValidFile3()
{
    const QString path = m_dataFolderPath + "Reader_Agilent_Test.data/RS080806_APCI_PIscan_CE35.d";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatAgilent)
            == VendorPathChecker::ReaderAgilentFormatAcqData);
}

void VendorPathCheckerTest::testAgilentAcqDataValidFile4()
{
    const QString path = m_dataFolderPath + "Reader_Agilent_Test.data/reserpine-MS2sim-010.d";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatAgilent)
            == VendorPathChecker::ReaderAgilentFormatAcqData);
}

void VendorPathCheckerTest::testWatersValidFile1()
{
    const QString path = m_dataFolderPath + "reader_waters_test.data/FakeValid000.raw";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatWaters)
            == VendorPathChecker::ReaderWatersFormatValid);
}

void VendorPathCheckerTest::testWatersInvalidFile1()
{
    const QString path = m_dataFolderPath + "reader_waters_test.data/Invalid000.raw";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatWaters)
            == VendorPathChecker::ReaderWatersFormatUnknown);
}

void VendorPathCheckerTest::testWatersInvalidFile2()
{
    const QString path = m_dataFolderPath + "reader_waters_test.data/Invalid001.raw";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatWaters)
            == VendorPathChecker::ReaderWatersFormatUnknown);
}

void VendorPathCheckerTest::testWatersInvalidFile3()
{
    const QString path = m_dataFolderPath + "reader_waters_test.data/Invalid002.raw";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatWaters)
            == VendorPathChecker::ReaderWatersFormatUnknown);
}

void VendorPathCheckerTest::testWatersInvalidFile4()
{
    const QString path = m_dataFolderPath + "reader_waters_test.data/Invalid003.raw";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatWaters)
            == VendorPathChecker::ReaderWatersFormatUnknown);
}

void VendorPathCheckerTest::testWatersInvalidFile5()
{
    const QString path = m_dataFolderPath + "reader_waters_test.data/NonExistent.raw";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatWaters)
            == VendorPathChecker::ReaderWatersFormatUnknown);
}

void VendorPathCheckerTest::testWatersInvalidFile6()
{
    const QString path = m_dataFolderPath + "reader_waters_test.data/Invalid004.raw";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatWaters)
            == VendorPathChecker::ReaderWatersFormatUnknown);
}

void VendorPathCheckerTest::testWatersValidFile2()
{
    const QString path = m_dataFolderPath + "reader_waters_test.data/FakeValid001.raw";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatWaters)
            == VendorPathChecker::ReaderWatersFormatValid);
}

void VendorPathCheckerTest::testShimadzuValidFile1() {
    const QString path = m_dataFolderPath + "reader_Shimadzu_test.data/valid_01.lcd";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatShimadzu)
        == VendorPathChecker::ReaderShimadzuFormatValid);
}

void VendorPathCheckerTest::testShimadzuInvalidFile0() {
    // not existent file
    const QString path = m_dataFolderPath + "reader_Shimadzu_test.data/invalid_00.lcd";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatShimadzu)
        == VendorPathChecker::ReaderShimadzuFormatUnknown);
}

void VendorPathCheckerTest::testShimadzuInvalidFile1() {
    const QString path = m_dataFolderPath + "reader_Shimadzu_test.data/invalid_01.ext";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatShimadzu)
        == VendorPathChecker::ReaderShimadzuFormatUnknown);
}

void VendorPathCheckerTest::testShimadzuInvalidFile2() {
    const QString path = m_dataFolderPath + "reader_Shimadzu_test.data/invalid_02.lcd";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatShimadzu)
        == VendorPathChecker::ReaderShimadzuFormatUnknown);
}

void VendorPathCheckerTest::testAbiValidFile1() {
    const QString path = m_dataFolderPath + "reader_Abi_test.data/valid_01.wiff";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatAbi)
        == VendorPathChecker::ReaderAbiFormatValid);
}

void VendorPathCheckerTest::testAbiInvalidFile0() {
    // not existent file
    const QString path = m_dataFolderPath + "reader_Abi_test.data/invalid_00.wiff";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatAbi)
        == VendorPathChecker::ReaderAbiFormatUnknown);
}

void VendorPathCheckerTest::testAbiInvalidFile1() {
    const QString path = m_dataFolderPath + "reader_Abi_test.data/invalid_01.ext";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatAbi)
        == VendorPathChecker::ReaderAbiFormatUnknown);
}

void VendorPathCheckerTest::testAbiInvalidFile2() {
    const QString path = m_dataFolderPath + "reader_Abi_test.data/invalid_02.wiff";
    QVERIFY(checkAndShowTestResult(path, &VendorPathChecker::formatAbi)
        == VendorPathChecker::ReaderAbiFormatUnknown);
}

PMI_TEST_GUILESS_MAIN_WITH_ARGS(VendorPathCheckerTest, QStringList() << "Remote Data Folder")

#include "VendorPathCheckerTest.moc"
