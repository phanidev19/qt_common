/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrzej Żuraw azuraw@milosolutions.com
 */

#include "ComInitializer.h"

#include <QApplication>
#include <QDebug>
#include <QThread>

#include <objBase.h>

_PMI_BEGIN

ComInitializer::ComInitializer(Options options)
{
    if (options & Option::InitializeForAllThreads) {
        initialize();
    } else if (options & Option::InitializeForNonMainThread) {
        if (qApp->thread() != QThread::currentThread()) {
            initialize();
        }
    }
}

ComInitializer::~ComInitializer()
{
    if (m_initialized) {
        // Each successful call to CoInitialize or CoInitializeEx, including any call that returns
        // S_FALSE, must be balanced by a corresponding call to CoUninitialize
        CoUninitialize();
    }
}

void ComInitializer::initialize()
{
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        qCritical().noquote().nospace()
            << "CoInitializeEx failed: 0x" << QString::number(hr, 16);
    }
    m_initialized = true;
}

_PMI_END
