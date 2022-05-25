// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2016 The Qt Company Ltd. <https://www.qt.io/licensing/>
// SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

#include <QtWidgets>

#include <KUrlMimeData>

#include "droparea.h"
#include "dropsitewindow.h"

DropSiteWindow::DropSiteWindow(QWidget *parent)
    : QWidget(parent)
{
    abstractLabel =
        new QLabel(tr("This example accepts drags from other "
                      "applications and displays the MIME types "
                      "provided by the drag object."));
    abstractLabel->setWordWrap(true);
    abstractLabel->adjustSize();

    dropArea = new DropArea;
    connect(dropArea, &DropArea::changed, this, &DropSiteWindow::updateFormatsTable);

    QStringList labels;
    labels << tr("Format") << tr("Content");

    formatsTable = new QTableWidget;
    formatsTable->setColumnCount(2);
    formatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    formatsTable->setHorizontalHeaderLabels(labels);
    formatsTable->horizontalHeader()->setStretchLastSection(true);

    clearButton = new QPushButton(tr("Clear"));
    copyButton = new QPushButton(tr("Copy"));

    buttonBox = new QDialogButtonBox;
    buttonBox->addButton(clearButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(copyButton, QDialogButtonBox::ActionRole);
    copyButton->setVisible(false);

    connect(clearButton, &QAbstractButton::clicked, dropArea, &DropArea::clear);
    connect(copyButton, &QAbstractButton::clicked, this, &DropSiteWindow::copy);

    auto  mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(abstractLabel);
    mainLayout->addWidget(dropArea);
    mainLayout->addWidget(formatsTable);
    mainLayout->addWidget(buttonBox);
}

void DropSiteWindow::updateFormatsTable(const QMimeData *mimeData)
{
    formatsTable->setRowCount(0);
    copyButton->setEnabled(false);
    if (!mimeData) {
        return;
    }

    const QStringList formats = mimeData->formats();
    for (const QString &format : formats) {
        auto formatItem = new QTableWidgetItem(format);
        formatItem->setFlags(Qt::ItemIsEnabled);
        formatItem->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);

        QString text;
        if (format == QLatin1String("text/plain")) {
            text = mimeData->text().simplified();
        } else if (format == QLatin1String("text/html")) {
            text = mimeData->html().simplified();
        } else if (format == QLatin1String("text/uri-list")) {
            QList<QUrl> urlList = KUrlMimeData::urlsFromMimeData(mimeData);
            for (int i = 0; i < urlList.size() && i < 32; ++i) {
                text.append(urlList.at(i).toString() + QLatin1Char(' '));
            }
        } else {
            QByteArray data = mimeData->data(format);
            for (int i = 0; i < data.size() && i < 32; ++i) {
                text.append(QStringLiteral("%1 ").arg(uchar(data[i]), 2, 16, QLatin1Char('0')).toUpper());
            }
        }

        int row = formatsTable->rowCount();
        formatsTable->insertRow(row);
        formatsTable->setItem(row, 0, new QTableWidgetItem(format));
        formatsTable->setItem(row, 1, new QTableWidgetItem(text));
    }

    formatsTable->resizeColumnToContents(0);
    copyButton->setEnabled(formatsTable->rowCount() > 0);
}

void DropSiteWindow::copy()
{
    QString text;
    for (int row = 0, rowCount = formatsTable->rowCount(); row < rowCount; ++row) {
        text += formatsTable->item(row, 0)->text() + ": " + formatsTable->item(row, 1)->text() + '\n';
    }
    QGuiApplication::clipboard()->setText(text);
}
