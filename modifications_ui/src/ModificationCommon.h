/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef MODIFICATIONCOMMON_H
#define MODIFICATIONCOMMON_H

#include "ModificationInfoModel.h"

#include <QObject>

class QComboBox;
class QGridLayout;

void insertRow(QGridLayout *layout, int rowIndex);
void removeRow(QGridLayout *layout, int rowIndex);

bool setCurrentComboBoxIndexByRootIndex(QComboBox *comboBox, Qt::ItemDataRole role);

#endif // MODIFICATIONCOMMON_H
