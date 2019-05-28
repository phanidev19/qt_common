/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Marcin Bartoszek mbartoszek@milosolutions.com
*/

#ifndef DATE_TIME_UTILS_H
#define DATE_TIME_UTILS_H

#include <pmi_common_core_mini_export.h>
#include <pmi_core_defs.h>
#include <QDateTime>

_PMI_BEGIN

/*!
 * \brief The DateTimeUtils class handles conversion from and to DateTime, Time, Date in the default
 *        format (Qt::ISODate).
 * It should be used in any portable document formats and settings.
 */
class PMI_COMMON_CORE_MINI_EXPORT DateTimeUtils
{
public:
    static QString toString(const QDateTime &dateTime);
    static QDateTime dateTimeFromString(const QString &string);

    static QString toString(const QTime &time);
    static QTime timeFromString(const QString &string);

    static QString toString(const QDate &date);
    static QDate dateFromString(const QString &string);

private:
    DateTimeUtils();
};

_PMI_END

#endif // DATE_TIME_UTILS_H
