/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#include <MSReaderMainWindow.h>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MSReaderMainWindow mainWindow;
    mainWindow.showMaximized();

    return app.exec();
}
