// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2016 The Qt Company Ltd. <https://www.qt.io/licensing/>
// SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

#include "droparea.h"

#include <QDragEnterEvent>
#include <QMimeData>

#include <KUrlMimeData>

DropArea::DropArea(QWidget *parent)
    : QLabel(parent)
{
    setFrameStyle(QFrame::Sunken | QFrame::StyledPanel);
    setAlignment(Qt::AlignCenter);
    setAcceptDrops(true);
    setAutoFillBackground(true);
    clear();
}

void DropArea::dragEnterEvent(QDragEnterEvent *event)
{
    setText(tr("<drop content>"));
    setBackgroundRole(QPalette::Highlight);

    event->acceptProposedAction();
    Q_EMIT changed(event->mimeData());
}

void DropArea::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void DropArea::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();

    if (mimeData->hasImage()) {
        setPixmap(qvariant_cast<QPixmap>(mimeData->imageData()));
    } else if (mimeData->hasHtml()) {
        setText(mimeData->html());
        setTextFormat(Qt::RichText);
    } else if (mimeData->hasText()) {
        setText(mimeData->text());
        setTextFormat(Qt::PlainText);
    } else if (mimeData->hasUrls()) {
        QList<QUrl> urlList = KUrlMimeData::urlsFromMimeData(mimeData);
        QString text;
        for (int i = 0; i < urlList.size() && i < 32; ++i) {
            text += urlList.at(i).path() + QLatin1Char('\n');
        }
        setText(text);
    } else {
        setText(tr("Cannot display data"));
    }

    setBackgroundRole(QPalette::Dark);
    event->acceptProposedAction();
}

void DropArea::dragLeaveEvent(QDragLeaveEvent *event)
{
    clear();
    event->accept();
}

void DropArea::clear()
{
    setText(tr("<drop content>"));
    setBackgroundRole(QPalette::Dark);

    Q_EMIT changed();
}
