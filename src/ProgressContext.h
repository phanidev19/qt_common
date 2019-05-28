/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Steven Pease spease@proteinmetrics.com
 */

#ifndef PROGRESSCONTEXT_H_
#define PROGRESSCONTEXT_H_

#include "pmi_core_defs.h"

#include "pmi_common_ms_export.h"

#include <QSharedPointer>

_PMI_BEGIN

class ProgressBarInterface;

class PMI_COMMON_MS_EXPORT ProgressContext {
public:
    ProgressContext(int maxValue, QSharedPointer<ProgressBarInterface> progress)
        :m_progress(progress)
    {
        if(!m_progress.isNull()) {
            m_progress->push(maxValue);
        }
    }

    ProgressContext(int maxValue, const ProgressContext &progressContext)
        :m_progress(progressContext.progress())
    {
        if(!m_progress.isNull()) {
            m_progress->push(maxValue);
        }
    }

    ~ProgressContext() {
        if(!m_progress.isNull()) {
            m_progress->pop();
        }
    }

    ProgressContext &operator++() {
        if(!m_progress.isNull()) {
            m_progress->incrementProgress();
        }

        return (*this);
    }

    void setText(const QString &text) {
        if (!m_progress.isNull()) {
            m_progress->setText(text);
        }
    }

protected:
    QSharedPointer<ProgressBarInterface> progress() const {
        return m_progress;
    }
private:
    QSharedPointer<ProgressBarInterface> m_progress;
};

_PMI_END

#endif  // PROGRESSCONTEXT_H_
