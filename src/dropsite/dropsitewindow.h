// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2016 The Qt Company Ltd. <https://www.qt.io/licensing/>
// SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

#pragma once

#include <QWidget>

class QDialogButtonBox;
class QLabel;
class QMimeData;
class QPushButton;
class QTableWidget;
class DropArea;

class DropSiteWindow : public QWidget
{
    Q_OBJECT

public:
    explicit DropSiteWindow(QWidget *parent = nullptr);

public Q_SLOTS:
    void updateFormatsTable(const QMimeData *mimeData);
    void copy();

private:
    DropArea *dropArea;
    QLabel *abstractLabel;
    QTableWidget *formatsTable;

    QPushButton *clearButton;
    QPushButton *copyButton;
    QDialogButtonBox *buttonBox;
};
