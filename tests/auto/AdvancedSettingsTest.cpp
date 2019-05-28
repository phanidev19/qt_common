#include <QtTest>

#include "AdvancedSettings.h"
#include "pmi_core_defs.h"

_PMI_BEGIN


class AdvancedSettingsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testPlotFragmentsFontProperties();
    void testPMiAdvancedSettingsIniFile();
private:
    struct ValueWithType
    {
        QString type;
        QString value;
    };
    ValueWithType createValueWithType(const QString &type, const QString &value) const
    {
        ValueWithType val;
        val.type = type;
        val.value = value;

        return val;
    }
    bool canBeConverted(const QVariant &value, const QString &type) const;
};


void AdvancedSettingsTest::testPlotFragmentsFontProperties()
{
    // it should be monospaced font
    QFont font = get_plot_fragments_main_font();
    QCOMPARE(font.styleHint(), QFont::TypeWriter);

    QFontMetrics fm(font);
    int charWidth = fm.width(QChar('A'));

    // check if all upper-case chars have same width
    for (char c = 'A'; c < 'Z'; c++) {
        QCOMPARE(fm.width(c), charWidth);
    }
}

bool AdvancedSettingsTest::canBeConverted(const QVariant &value, const QString &type) const
{
    bool ok = true;
    if (type == "int") {
        value.toInt(&ok);
    } else if (type == "double"){
        value.toDouble(&ok);
    }

    return ok;
}

void AdvancedSettingsTest::testPMiAdvancedSettingsIniFile()
{
    AdvancedSettings *as = AdvancedSettings::Instance();
    as->load(QDir::toNativeSeparators(QFile::decodeName(PMI_TEST_FILES_DATA_DIR "/AdvancedSettings/test.ini")), true);
    qDebug () << PMI_TEST_FILES_DATA_DIR;
    //some values should be overwriten
    as->load(QDir::toNativeSeparators(QFile::decodeName(PMI_TEST_FILES_DATA_DIR "/AdvancedSettings/test_2.ini")), false);

    QHash<QString, ValueWithType> values;
    values.insert("ValueWithoutGroup_1", createValueWithType("double", "101.0"));
    values.insert("ValueWithoutGroup_2", createValueWithType("double", "2.0"));
    values.insert("Group_1/ValueWithGroup_1", createValueWithType("string", "test"));
    values.insert("Group_2/ValueWithGroup_2", createValueWithType("int", "123"));
    values.insert("Group_2/ValueWithGroup_3", createValueWithType("double", "10123.456"));

    QCOMPARE(as->size(), values.size());
    for (const QString &key : values.keys()) {
        QCOMPARE(as->contains(key), true);
        QCOMPARE(canBeConverted(as->value(key, QVariant()), values[key].type), true);
        QCOMPARE(as->value(key, QVariant()).toString(), values[key].value);
    }
}

_PMI_END

QTEST_MAIN(pmi::AdvancedSettingsTest)

#include "AdvancedSettingsTest.moc"
