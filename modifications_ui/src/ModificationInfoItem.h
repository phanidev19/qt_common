/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef MODIFICATIONINFOITEM_H
#define MODIFICATIONINFOITEM_H

#include "ModificationInfoModel.h"

#include <QObject>

class QAction;
class QComboBox;
class QGridLayout;
class QLineEdit;
class QPushButton;
class ModificationsUiWidget;
class ModificationComboBox;
class MultiselectComboBox;

class ModificationInfoItem : public QObject
{
    Q_OBJECT
public:
    explicit ModificationInfoItem(ModificationsUiWidget *uiWidget,
                                  ModificationInfoModel *modificationInfoModel);
    ~ModificationInfoItem();

    ModificationInfoModel *model();

    void createWidgets();

private:
    void createModificationsComboBox();
    void createTargetsComboBox();
    void createFineControlComboBox();
    Q_SLOT void createAttributeEditor();

    void trySetCurrentModification(QComboBox *modificationsComboBox);
    void trySetCurrentFine(QComboBox *fineComboBox, QComboBox *modMaxComboBox);
    void trySetAttribute(QLineEdit *attributeEditor);

    QGridLayout *layout() const;

private:
    ModificationsUiWidget *m_uiWidget = nullptr;

    QScopedPointer<ModificationInfoModel> m_modificationInfoModel;

    QScopedPointer<ModificationComboBox> m_modificationsComboBox;
    QScopedPointer<MultiselectComboBox> m_targetsComboBox;
    QScopedPointer<QComboBox> m_fineControlComboBox;
    QScopedPointer<QComboBox> m_modMaxComboBox;
    QScopedPointer<QPushButton> m_menuButton;
    QScopedPointer<QLineEdit> m_attributeEditor;

    QScopedPointer<QAction> m_invertModificationAction;
};

#endif // MODIFICATIONINFOITEM_H
