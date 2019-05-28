/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Marcin Bartoszek mbartoszek@milosolutions.com
*/

#include "DateTimeUtils.h"

_PMI_BEGIN

DateTimeUtils::DateTimeUtils()
{
}

QString DateTimeUtils::toString(const QDateTime &dateTime)
{
    return dateTime.toString(Qt::ISODate);
}

QDateTime DateTimeUtils::dateTimeFromString(const QString &string)
{
    return QDateTime::fromString(string, Qt::ISODate);
}

QString DateTimeUtils::toString(const QTime &time)
{
    return time.toString(Qt::ISODate);
}

QTime DateTimeUtils::timeFromString(const QString &string)
{
    return QTime::fromString(string, Qt::ISODate);
}

QString DateTimeUtils::toString(const QDate &date)
{
    return date.toString(Qt::ISODate);
}

QDate DateTimeUtils::dateFromString(const QString &string)
{
    return QDate::fromString(string, Qt::ISODate);
}

_PMI_END

