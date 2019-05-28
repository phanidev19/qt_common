/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef GLYCANSUIWIDGET_H
#define GLYCANSUIWIDGET_H

#include "pmi_modifications_ui_export.h"

#include <QWidget>

class GlycanEditorDialog;
class GlycanInfoModel;
class UnimodModifications;

class QComboBox;
class QGridLayout;
class QPlainTextEdit;
class QScrollArea;

/*!
 * \brief GlycansUiWidget widget provides user interface for editing glycans list.
 */
class PMI_MODIFICATIONS_UI_EXPORT GlycansUiWidget : public QWidget
{
    Q_OBJECT
public:
    /*!
     * \param proteomeDiscoverer_2_0 stays for backward compatibility
     * \param proteomeDiscoverer_1_4 stays for backward compatibility
     * \todo refactor proteomeDiscoverer_2_0 and proteomeDiscoverer_1_4 papameters and replace them
     * by more meaningful parameters
     */
    explicit GlycansUiWidget(bool proteomeDiscoverer_2_0 = false,
                             bool proteomeDiscoverer_1_4 = false, QWidget *parent = nullptr);
    ~GlycansUiWidget();

    /*!
     * \brief set paths to glycan databases
     * \param paths list of filesystem paths to GlycanDB files and folders with files. Files in
     * folders are added not recursively.
     */
    void setGlycanDbPaths(const QStringList &paths);

    /*!
     * \brief set glycan strings in Byonic format.
     * \param strings list of strings in Byonic format
     */
    void setGlycansStrings(const QStringList &strings);

    /*!
     * \return current window state in Byonic format
     */
    QStringList glycansStrings() const;

    /*!
     * \return applied window state in Byonic format by Ok button clicked or initial state or
     * strings set by setGlycansStrings call (which was last)
     */
    QStringList appliedGlycansStrings() const;

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
    void saveAndExitClicked(const QString& filename);

protected:
    GlycanEditorDialog *glycanEditorDialog();

    virtual bool eventFilter(QObject *object, QEvent *event) override;

private:
    Q_SLOT void addGlycanDbInfoControl();
    Q_SLOT void addGlycanNonDbInfoControl();
    void addGlycanInfoControl(GlycanInfoModel *model);

    void removeAllGlycans();

private:
    friend class GlycanInfoItem;

    void initUi();
    void initGlycansUi();
    void initButtons();

    QGridLayout *glycanLayout();
    int glycanRow(GlycanInfoItem *item) const;

private:
    bool m_proteomeDiscoverer_2_0 = false;
    bool m_proteomeDiscoverer_1_4 = false;

    QGridLayout *m_glycansLayout = nullptr;

    QList<GlycanInfoItem *> m_glycanDbItems;
    QList<GlycanInfoItem *> m_glycanItems;

    QScrollArea *m_glycansScrollArea = nullptr;
    QPlainTextEdit *m_textEdit = nullptr;

    bool m_doNotCreateEmptyRow = false;

    QStringList m_appliedModStrings;

    QList<QPair<QString, QString>> m_glycanDbFiles;

    GlycanEditorDialog *m_glycanEditorDialog = nullptr;
};

#endif // GLYCANSUIWIDGET_H
