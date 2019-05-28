/*
 * Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef PMI_TEST_UTILS_H
#define PMI_TEST_UTILS_H

#define NOMINMAX // Suppress min/max definition from Windows.h so std::numeric_limits works

#include "ComInitializer.h"

#include <common_errors.h>
#include <pmi_common_core_mini_export.h>

#include <QtTest>

#include <Windows.h>
#include <objbase.h>

#include "TestArgumentProcessor.h"

namespace QTest {

//! Overload to QVERIFY() works for functions that return pmi::Err.
//! This way QCOMPARE(someFunc(), kNoErr) is not needed.
inline bool qVerify(pmi::Err e, const char *statementStr, const char *description,
                    const char *file, int line)
{
    return QTest::qVerify(e == pmi::kNoErr, statementStr, description, file, line);
}

//! Overload to make QCOMPARE(QString, const char*) work
inline bool qCompare(const QString &t1, const char *t2, const char *actual,
                    const char *expected, const char *file, int line)
{
    return QTest::qCompare(t1, QString::fromLatin1(t2), actual, expected, file, line);
}

} // QtTest

_PMI_BEGIN

typedef void (*ExtraFunction)();

//! Helper to implement main() that eats private test's arguments
template<typename TestClass, typename AppClass>
int mainTestApplication(int argc, char *argv[], const QStringList &privateArgNames,
                        ExtraFunction extraAppInit, ExtraFunction extraAppDone)
{
    int realArgc;
    QStringList privateArgs;
    if (!TestArgumentProcessor::processArguments(argc, argv, privateArgNames, &realArgc, &privateArgs)) {
        return 1;
    }
    AppClass app(realArgc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    if (extraAppInit) {
        extraAppInit();
    }
    TestClass tc(privateArgs);
    QTEST_SET_MAIN_SOURCE_PATH
    const int result = QTest::qExec(&tc, realArgc, argv);
    if (extraAppDone) {
        extraAppDone();
    }
    return result;
}

#ifdef QT_WIDGETS_LIB
    //! Implements a main() function that instantiates @a AppClass application object and @a TestClass test object.
    //! @a AppClass can be QApplication; for QAL it can be PmiApplication if needed.
    //! Use PmiApplication if CoInitialize/CoUninitialize() calls are needed.
    //! Executes all tests in the order they were defined. Use this macro to build stand-alone executables.
    //! This macro replaces QTEST_MAIN. In addition to @a TestClass class name it also takes second
    //! parameter @a privateArgNames which is a list of command line arguments that are passed
    //! to the test object. The list should be of QStringList type and is used when tests arguments
    //! are defined in CMake files.
    //! @see PMI_TEST_GUILESS_MAIN_WITH_ARGS
    #define PMI_TEST_MAIN_WITH_ARGS(TestClass, AppClass, privateArgNames) \
    QT_BEGIN_NAMESPACE \
    QTEST_ADD_GPU_BLACKLIST_SUPPORT_DEFS \
    QT_END_NAMESPACE \
    void extraGuiAppInit() \
    { \
        QTEST_DISABLE_KEYPAD_NAVIGATION \
        QTEST_ADD_GPU_BLACKLIST_SUPPORT \
    } \
    int main(int argc, char *argv[]) \
    { \
        return pmi::mainTestApplication<TestClass, AppClass>( \
            argc, argv, privateArgNames, extraGuiAppInit, nullptr); \
    }
#elif defined(QT_GUI_LIB)
    //! @todo Add PMI_TEST_MAIN_WITH_ARGS if we ever need tests for non-QWidget GUI app
#endif // QT_GUI_LIB

//! Implements a main() function that instantiates a QCoreApplication object and the TestClass.
//! Executes all tests in the order they were defined. Use this macro to build stand-alone executables.
//! This macro replaces QTEST_GUILESS_MAIN. In addition to @a TestClass class name it also takes second
//! parameter @a privateArgNames which is a list of command line arguments that are passed
//! to the test object. The list should be of QStringList type and is used when tests arguments
//! are defined in CMake files.
//! @see PMI_TEST_MAIN_WITH_ARGS
#define PMI_TEST_GUILESS_MAIN_WITH_ARGS(TestClass, privateArgNames) \
int main(int argc, char *argv[]) \
{ \
    pmi::ComInitializer comInitializer; \
    return pmi::mainTestApplication<TestClass, QCoreApplication>( \
        argc, argv, privateArgNames, nullptr, nullptr); \
}

_PMI_END

#endif
