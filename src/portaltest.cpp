/*
 * Copyright © 2016 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Jan Grulich <jgrulich@redhat.com>
 */

#include "portaltest.h"
#include "ui_portaltest.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>

#include <KNotification>

Q_LOGGING_CATEGORY(PortalTestKde, "portal-test-kde")

PortalTest::PortalTest(QWidget *parent, Qt::WindowFlags f)
    : QMainWindow(parent, f)
    , m_mainWindow(new Ui::PortalTest)
{
    QLoggingCategory::setFilterRules(QStringLiteral("portal-test-kde.debug = true"));

    m_mainWindow->setupUi(this);

    m_mainWindow->sandboxLabel->setText(isRunningSandbox() ? QLatin1String("yes") : QLatin1String("no"));

    connect(m_mainWindow->openFile, &QPushButton::clicked, this, &PortalTest::openFileRequested);
    connect(m_mainWindow->saveFile, &QPushButton::clicked, this, &PortalTest::saveFileRequested);
    connect(m_mainWindow->notifyButton, &QPushButton::clicked, this, &PortalTest::sendNotification);
}

PortalTest::~PortalTest()
{
    delete m_mainWindow;
}

void PortalTest::notificationActivated(uint action)
{
    m_mainWindow->notificationResponse->setText(QString("Action number %1 activated").arg(QString::number(action)));
}

void PortalTest::openFileRequested()
{
    QFileDialog *fileDialog = new QFileDialog(this);
    fileDialog->setFileMode(QFileDialog::ExistingFiles);
//     fileDialog->setNameFilters(QStringList { QLatin1String("Fooo (*.txt *.patch)"), QLatin1String("Text (*.doc *.docx)"), QLatin1String("Any file (*)") });
    fileDialog->setMimeTypeFilters(QStringList { QLatin1String("text/plain"), QLatin1String("image/png") } );
    fileDialog->setLabelText(QFileDialog::Accept, QLatin1String("Open (portal)"));
    fileDialog->setModal(false);
    fileDialog->setWindowTitle(QLatin1String("Flatpak test - open dialog"));

    if (fileDialog->exec() == QDialog::Accepted) {
        if (!fileDialog->selectedFiles().isEmpty()) {
            m_mainWindow->selectedFiles->setText(fileDialog->selectedFiles().join(QLatin1String(", ")));
        }
        fileDialog->deleteLater();
    }
}

void PortalTest::saveFileRequested()
{
    QFileDialog *fileDialog = new QFileDialog(this);
    fileDialog->setAcceptMode(QFileDialog::AcceptSave);
    fileDialog->setLabelText(QFileDialog::Accept, QLatin1String("Save (portal)"));
    fileDialog->setNameFilters(QStringList { QLatin1String("Fooo (*.txt *.patch)"), QLatin1String("Text (*.doc *.docx)"), QLatin1String("Any file (*)") });
    fileDialog->setModal(true);
    fileDialog->setDirectory(QStandardPaths::standardLocations(QStandardPaths::ConfigLocation).last());
    fileDialog->selectFile(QLatin1String("test.txt"));
    fileDialog->setWindowTitle(QLatin1String("Flatpak test - save dialog"));

    if (fileDialog->exec() == QDialog::Accepted) {
        if (!fileDialog->selectedFiles().isEmpty()) {
            m_mainWindow->selectedFiles->setText(fileDialog->selectedFiles().join(QLatin1String(", ")));
        }
        fileDialog->deleteLater();
    }
}

void PortalTest::sendNotification()
{
    KNotification *notify = new KNotification(QLatin1String("notification"), this);
    connect(notify, static_cast<void (KNotification::*)(uint)>(&KNotification::activated), this, &PortalTest::notificationActivated);
    connect(m_mainWindow->notifyCloseButton, &QPushButton::clicked, notify, &KNotification::close);
    connect(notify, &KNotification::closed, [this] () {
        m_mainWindow->notifyCloseButton->setDisabled(true);
    });

    notify->setFlags(KNotification::DefaultEvent);
    notify->setTitle(QLatin1String("Notification test"));
    notify->setText(QLatin1String("<html><b>Hello world!!<b><html>"));
    notify->setActions(QStringList { i18n("Action 1"), i18n("Action 2")});
    notify->setIconName(QLatin1String("applications-development"));

    m_mainWindow->notifyCloseButton->setEnabled(true);
    notify->sendEvent();
}

bool PortalTest::isRunningSandbox()
{
    QString runtimeDir = qgetenv("XDG_RUNTIME_DIR");

    if (runtimeDir.isEmpty()) {
        return false;
    }

    QFile file(runtimeDir + QLatin1String("/flatpak-info"));

    return file.exists();
}

