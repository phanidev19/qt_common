/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef GLYCANINFOITEM_H
#define GLYCANINFOITEM_H

#include "GlycanInfoModel.h"

#include <QObject>

class QAction;
class QComboBox;
class QGridLayout;
class QLineEdit;
class QPushButton;
class GlycansUiWidget;

class GlycanInfoItem : public QObject
{
    Q_OBJECT
public:
    explicit GlycanInfoItem(GlycansUiWidget *uiWidget, GlycanInfoModel *glycanInfoModel);
    ~GlycanInfoItem();

    GlycanInfoModel *model();

    void createWidgets();

private:
    void createGlycanTypeComboBox();
    void createGlycanDbComboBox();
    void createGlycanEditor();
    void createFineControlComboBox();

    void trySetCurrentGlycanType(QComboBox *glycanTypeComboBox);
    void trySetCurrentGlycanDb(QComboBox *glycanDbComboBox);
    void trySetCurrentGlycan(QLineEdit *glycanEditor);
    void trySetCurrentFine(QComboBox *fineComboBox, QComboBox *modMaxComboBox);

    Q_SLOT void onGlycanEdited();
    Q_SLOT void onGlycanEditCanceled();

    QGridLayout *layout() const;

private:
    GlycansUiWidget *m_uiWidget = nullptr;
    QScopedPointer<GlycanInfoModel> m_glycanInfoModel;

    QScopedPointer<QComboBox> m_glycanTypeComboBox;
    QScopedPointer<QComboBox> m_glycanDbComboBox;
    QScopedPointer<QLineEdit> m_glycanEditor;
    QScopedPointer<QPushButton> m_glycanEditButton;
    QScopedPointer<QComboBox> m_fineControlComboBox;
    QScopedPointer<QComboBox> m_modMaxComboBox;
    QScopedPointer<QPushButton> m_deleteButton;
};

#endif // GLYCANINFOITEM_H
