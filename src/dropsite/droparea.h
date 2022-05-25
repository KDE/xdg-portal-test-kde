// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2016 The Qt Company Ltd. <https://www.qt.io/licensing/>
// SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

#pragma once

#include <QLabel>

class QMimeData;

class DropArea : public QLabel
{
    Q_OBJECT

public:
    explicit DropArea(QWidget *parent = nullptr);

public Q_SLOTS:
    void clear();

Q_SIGNALS:
    void changed(const QMimeData *mimeData = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QLabel *label = nullptr;
};
