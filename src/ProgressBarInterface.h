/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef PROGRESS_BAR_INTERFACE_H
#define PROGRESS_BAR_INTERFACE_H

#include "pmi_core_defs.h"

#include "pmi_common_ms_export.h"

#include <QSharedPointer>


// Convenience macro improving readability
#define NoProgress QSharedPointer<pmi::ProgressBarInterface>()

/*!
 *  Example usage:
 *
 *  void calculate(QSharedPointer<ProgressBarInterface> progress)
 *  {
 *      // some preparations not covered by progress
 *      if (progress) {
 *          progress->setText("Calculating...");
 *      }
 *      ProgressContext progressContextOverall(2, progress);
 *      {
 *          ProgressContext progressContext(4, progress);
 *          for (int i = 0; i < 4; ++i, ++progressContext) {
 *          	// calculation step...
 *	        }
 *      }
 *      ++progressContextOverall;
 *
 *      {
 *          ProgressContext progressContext(6, progress);
 *          for (int i = 0; i < 6; ++i, ++progressContext) {
 *               // different calculation step...
 *          }
 *      }
 *      ++progressContextOverall;
 *  }
 *  @note One should avoid using push() and pop() methods. Use RAII ProgressContext instead.
 */

_PMI_BEGIN

class PMI_COMMON_MS_EXPORT ProgressBarInterface {

public:    
    virtual void push(int max) = 0;
    virtual void pop() = 0;
    virtual double incrementProgress(int inc = 1) = 0;

    //! \brief action that is about to be executed
    virtual void setText(const QString &text) = 0;

    virtual void refreshUI() = 0;

    virtual bool userCanceled() const = 0;

};

_PMI_END

#endif // PROGRESS_BAR_INTERFACE_H
