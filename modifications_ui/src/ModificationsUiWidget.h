/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef MODIFICATIONSUIWIDGET_H
#define MODIFICATIONSUIWIDGET_H

#include "pmi_modifications_ui_export.h"

#include <QWidget>

class ModificationInfoModel;
class UnimodModifications;

class QComboBox;
class QGridLayout;
class QPlainTextEdit;
class QScrollArea;

/*!
 * \brief ModificationsUiWidget widget provides user interface for editing modifications list.
 */
class PMI_MODIFICATIONS_UI_EXPORT ModificationsUiWidget : public QWidget
{
    Q_OBJECT
public:
    /*!
     * \param proteomeDiscoverer_2_0 stays for backward compatibility
     * \param proteomeDiscoverer_1_4 stays for backward compatibility
     * \todo refactor proteomeDiscoverer_2_0 and proteomeDiscoverer_1_4 papameters and replace them
     * by more meaningful parameters
     */
    explicit ModificationsUiWidget(bool proteomeDiscoverer_2_0 = false,
                                   bool proteomeDiscoverer_1_4 = false, QWidget *parent = nullptr);
    ~ModificationsUiWidget();

    /*!
     * \brief set modifications strings in Byonic format.
     * \param strings list of strings in Byonic format
     */
    void setModStrings(const QStringList &strings);
    /*!
     * \return current window state in Byonic format
     */
    QStringList modStrings() const;
    /*!
     * \return applied window state in Byonic format by Ok button clicked or initial state or
     * strings set by setGlycansStrings call (which was last)
     */
    QStringList appliedModStrings() const;

Q_SIGNALS:
    /*!
     * \brief emits when Ok button is clicked
     */
    void accepted();
    /*!
     * \brief emits when Cancel button is clicked
     */
    void rejected();
    /*!
     * \brief emits when m_proteomeDiscoverer_1_4=true and Save button is clicked
     * \param filename
     */
    void saveAndExitClicked(const QString &filename);

protected:
    virtual bool eventFilter(QObject *object, QEvent *event) override;

private:
    Q_SLOT void addModificationInfoControl();
    void addModificationInfoControl(ModificationInfoModel *model);

    void removeAllModifications();

private:
    friend class ModificationInfoItem;

    void initUi();
    void initModificationsUi();
    void initButtons();

    QGridLayout *modificationsLayout();
    int modificationRow(ModificationInfoItem *item) const;

private:
    bool m_proteomeDiscoverer_2_0 = false;
    bool m_proteomeDiscoverer_1_4 = false;

    QGridLayout *m_modificationsLayout = nullptr;

    QSharedPointer<UnimodModifications> m_modifications;
    QList<ModificationInfoItem *> m_modificationItems;

    QScrollArea *m_modificationsScrollArea = nullptr;
    QPlainTextEdit *m_textEdit = nullptr;

    bool m_doNotCreateEmptyRow = false;

    QStringList m_appliedModStrings;
};

#endif // MODIFICATIONSUIWIDGET_H
