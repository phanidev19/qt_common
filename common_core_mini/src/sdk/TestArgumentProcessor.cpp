/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Jaros³aw Staniek jstaniek@milosolutions.com
*/


#include "TestArgumentProcessor.h"

#include <QtGlobal>
#include <QStringList>
#include <iostream>

#include <QFileInfo>

_PMI_BEGIN

static void showExtraHelp(char *argv[], const QStringList &privateArgNames)
{
    std::cout << qPrintable(QObject::tr("Usage: %1 [QtTest_options] <test_arguments>")
        .arg(QFileInfo(argv[0]).fileName())) << std::endl << std::endl;
    std::cout << qPrintable(QObject::tr("This test application requires exactly %1 test arguments: %2.")
        .arg(privateArgNames.count()).arg(privateArgNames.join(", "))) << std::endl;
    std::cout << qPrintable(QObject::tr("For more information read section \"%1\" at %2")
        .arg("7.6. Running tests with required arguments")
        .arg("https://bitbucket.org/ykil/qt_apps_libs/src/develop/TEST_GUIDE.md?"
        "fileviewer=file-view-default"
        "#markdown-header-76-running-tests-with-required-arguments"))
        << std::endl << std::endl;
}

PMI_COMMON_CORE_MINI_EXPORT bool TestArgumentProcessor::processArguments(int argc, char *argv[], const QStringList &privateArgNames,
    int *realArgc, QStringList *privateArgs)
{
    bool hasPrivateArgNames = true;
    if (argc > 1) {
        // Special options, in these cases tests won't be executed so private arg names are not needed:
        const QByteArray arg(argv[1]);
        if (arg == "-functions" // Returns a list of current test functions
            || arg == "-datatags" // Returns a list of current data tags.
            || arg == "-help")
        {
            *realArgc = argc;
            hasPrivateArgNames = false;
            if (arg == "-help") {
                showExtraHelp(argv, privateArgNames);
                std::cout << qPrintable(QObject::tr("Qt Test's help:")) << std::endl;
            }
        }
    }
    if (hasPrivateArgNames) {
        if ((argc - 1) < privateArgNames.count()) {
            std::cout << qPrintable(QObject::tr("Incorrect number of test arguments.")) << std::endl;
            showExtraHelp(argv, privateArgNames);
            return false;
        }
        *realArgc = argc - privateArgNames.count(); // eat the private args
    }
    for (int i = 0; i < privateArgNames.count(); ++i) {
        *privateArgs += (hasPrivateArgNames ? QFile::decodeName(argv[*realArgc + i]) : QString());
        // (if !hasPrivateArgNames, just add empty args; needed because the test class may depend on it and crash otherwise)
    }
    return true;
}

_PMI_END
