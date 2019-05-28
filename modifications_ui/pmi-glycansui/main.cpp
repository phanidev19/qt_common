/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "GlycansUiMainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTextStream>

int main(int argc, char *argv[])
{
    bool proteomeDiscoverer_2_0 = false;
    bool proteomeDiscoverer_1_4 = true;

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("PMi-GlycansUI"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("PMi-GlycansUI"));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption stdinOption(QStringList() << QStringLiteral("read_stdin"),
                                         QStringLiteral("Read data from stdin"));
    parser.addOption(stdinOption);

    const QCommandLineOption uiposOption(QStringList() << QStringLiteral("ui_pos"),
                                         QStringLiteral("Window position"), QStringLiteral("X,Y"));
    parser.addOption(uiposOption);

    parser.addPositionalArgument(QStringLiteral("glycan_db_path"),
                                 QStringLiteral("Path to Glycans database folder"));

    parser.process(app);

    const bool readFromStdin = parser.isSet(stdinOption);
    QStringList modStrings;
    if (readFromStdin) {
        proteomeDiscoverer_1_4 = false;
    }

    QStringList glycanDbPaths;

    if (!parser.positionalArguments().isEmpty()) {
        const QString dbPathDelimiter = QStringLiteral("|");
        const QString glycanDbPathsStr = parser.positionalArguments()[0];

        if (glycanDbPathsStr.startsWith(dbPathDelimiter)) {
            const QStringList paths = glycanDbPathsStr.split(dbPathDelimiter);
            for (auto path : paths) {
                if (!path.trimmed().isEmpty()) {
                    glycanDbPaths << path.trimmed();
                }
            }
        } else {
            glycanDbPaths << glycanDbPathsStr;
        }
    }

    GlycansUiMainWindow mainWindow(proteomeDiscoverer_2_0, proteomeDiscoverer_1_4, readFromStdin,
                                   glycanDbPaths);

    if (parser.isSet(uiposOption)) {
        const QString posStr = parser.value(uiposOption);
        const QStringList posValues = posStr.split(QStringLiteral(","));
        if (posValues.count() == 2) {
            mainWindow.move(posValues[0].toInt(), posValues[1].toInt());
        }
    }

    mainWindow.show();
    const int result = app.exec();

    const QStringList outputModStrings = mainWindow.glycansStrings();
    QTextStream outputStream(stdout);
    outputStream << outputModStrings.join(QStringLiteral("\n")).toLatin1().data();
    outputStream << "\n";
    outputStream.flush();

    return 0;
}
