/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrzej Żuraw azuraw@milosolutions.com
 */

#ifndef COMINITIALIZER_H
#define COMINITIALIZER_H

#include <pmi_common_core_mini_export.h>
#include <pmi_core_defs.h>

#include <QFlags>

_PMI_BEGIN

/*!
 * \brief A handler of Windows COM
 * It calls CoInitializeEx on construction and CoUninitialize on destruction.
 * Use objects of this class in top level blocks of thread worker functions.
 */
class PMI_COMMON_CORE_MINI_EXPORT ComInitializer
{
public:
    /*!
     * \brief The Com library initialization options
     */
    enum class Option {
        InitializeForNonMainThread
        = 0x01, /*!< Com is initialized only if the current thread is not the main thread */
        InitializeForAllThreads
        = 0x02 /*!< Com is always initialized, regardless of the current thread */
    };
    Q_DECLARE_FLAGS(Options, Option);

    /*!
     * \brief Constructor - calls CoInitializeEx
     *
     * \param options - initialization options
     */
    explicit ComInitializer(Options options = Option::InitializeForAllThreads);
    /*!
     * \brief Destructor - calls CoUninitialize (only if CoInitializeEx was called)
     */
    ~ComInitializer();

private:
    void initialize();
    bool m_initialized = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ComInitializer::Options)

_PMI_END

#endif // COMINITIALIZER_H
