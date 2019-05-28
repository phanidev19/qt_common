/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef GlycanEditorDialog_H
#define GlycanEditorDialog_H

#include <QDialog>
#include <QMap>

class QLabel;
class QLineEdit;

class GlycanEditorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GlycanEditorDialog(QWidget *parent = nullptr);
    ~GlycanEditorDialog();

    void startEdit(const QString &glycan);

    QString glycan() const;

    static bool isGlycanValid(const QString &glycan);

private:
    void initUi();
    void updateTotalMass();

private:
    QLabel *m_totalMassLabel = nullptr;
    QMap<int, QLineEdit *> m_elementsEditors;
    QLineEdit *m_additionalMassEditor;
};

#endif // GlycanEditorDialog_H
