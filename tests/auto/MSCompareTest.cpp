#include <QtTest>
#include "pmi_core_defs.h"

#include "PlotBase.h"

_PMI_BEGIN

class MSCompareTest : public QObject
{
    Q_OBJECT

public: 

private:

private Q_SLOTS:
void testCompareContrastMSFilesGivenIdenticalFiles();
private:

};

void MSCompareTest::testCompareContrastMSFilesGivenIdenticalFiles()
{
    PlotBase plotBase;

// pwiz file to be checked: 
//	C:\Users\sam\Desktop\KVC0918201601AFAMM.raw.pwiz.byspec2"
}

_PMI_END

QTEST_MAIN(pmi::MSCompareTest)

#include "MSCompareTest.moc"
