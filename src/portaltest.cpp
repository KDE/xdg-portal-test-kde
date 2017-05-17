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

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QPainter>
#include <QPdfWriter>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTemporaryFile>
#include <QWindow>

#include <KNotification>

Q_LOGGING_CATEGORY(PortalTestKde, "portal-test-kde")

PortalTest::PortalTest(QWidget *parent, Qt::WindowFlags f)
    : QMainWindow(parent, f)
    , m_mainWindow(new Ui::PortalTest)
{
    QLoggingCategory::setFilterRules(QStringLiteral("portal-test-kde.debug = true"));

    m_mainWindow->setupUi(this);

    m_mainWindow->sandboxLabel->setText(isRunningSandbox() ? QLatin1String("yes") : QLatin1String("no"));
    m_mainWindow->printWarning->setText(QLatin1String("Select an image in PNG format using FileChooser part!!"));

    QMenu *menu = new QMenu(this);
    menu->addAction(QIcon::fromTheme(QLatin1String("application-exit")), QLatin1String("Quit"), qApp, &QApplication::quit);

    QSystemTrayIcon *trayIcon = new QSystemTrayIcon(QIcon::fromTheme(QLatin1String("kde")), this);
    trayIcon->setContextMenu(menu);
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated, [this] (QSystemTrayIcon::ActivationReason reason) {
        switch (reason) {
            case QSystemTrayIcon::Unknown:
                m_mainWindow->systrayLabel->setText(QLatin1String("Unknown reason"));
                break;
            case QSystemTrayIcon::Context:
                m_mainWindow->systrayLabel->setText(QLatin1String("The context menu for the system tray entry was requested"));
                break;
            case QSystemTrayIcon::DoubleClick:
                m_mainWindow->systrayLabel->setText(QLatin1String("The system tray entry was double clicked"));
                break;
            case QSystemTrayIcon::Trigger:
                m_mainWindow->systrayLabel->setText(QLatin1String("The system tray entry was clicked"));
                show();
                break;
            case QSystemTrayIcon::MiddleClick:
                m_mainWindow->systrayLabel->setText(QLatin1String("The system tray entry was clicked with the middle mouse button"));
                break;
        }
    });

    connect(m_mainWindow->inhibit, &QPushButton::clicked, this, &PortalTest::inhibitRequested);
    connect(m_mainWindow->uninhibit, &QPushButton::clicked, this, &PortalTest::uninhibitRequested);
    connect(m_mainWindow->openFile, &QPushButton::clicked, this, &PortalTest::openFileRequested);
    connect(m_mainWindow->saveFile, &QPushButton::clicked, this, &PortalTest::saveFileRequested);
    connect(m_mainWindow->notifyButton, &QPushButton::clicked, this, &PortalTest::sendNotification);
    connect(m_mainWindow->printButton, &QPushButton::clicked, this, &PortalTest::printDocument);
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
    fileDialog->setMimeTypeFilters(QStringList { QLatin1String("text/plain"), QLatin1String("image/png") } );
    fileDialog->setLabelText(QFileDialog::Accept, QLatin1String("Open (portal)"));
    fileDialog->setModal(false);
    fileDialog->setWindowTitle(QLatin1String("Flatpak test - open dialog"));

    if (fileDialog->exec() == QDialog::Accepted) {
        if (!fileDialog->selectedFiles().isEmpty()) {
            m_mainWindow->selectedFiles->setText(fileDialog->selectedFiles().join(QLatin1String(", ")));
            if (fileDialog->selectedFiles().first().endsWith(QLatin1String(".png"))) {
                m_mainWindow->printButton->setEnabled(true);
                m_mainWindow->printWarning->setVisible(false);
            } else {
                m_mainWindow->printButton->setEnabled(false);
                m_mainWindow->printWarning->setVisible(true);
            }
        }
        fileDialog->deleteLater();
    }
}

void PortalTest::gotPrintResponse(uint response, const QVariantMap &results)
{
    // TODO do cleaning
    qWarning() << response << results;
}

void PortalTest::gotPreparePrintResponse(uint response, const QVariantMap &results)
{
    if (!response) {
        QVariantMap settings;
        QVariantMap pageSetup;

        QDBusArgument dbusArgument = results.value(QLatin1String("settings")).value<QDBusArgument>();
        dbusArgument >> settings;

        QDBusArgument dbusArgument1 = results.value(QLatin1String("page-setup")).value<QDBusArgument>();
        dbusArgument1 >> pageSetup;

        QTemporaryFile tempFile;
        tempFile.setAutoRemove(false);
        if (!tempFile.open()) {
            qWarning() << "Couldn't generate pdf file";
            return;
        }

        QPdfWriter writer(tempFile.fileName());
        QPainter painter(&writer);

        if (pageSetup.contains(QLatin1String("Orientation"))) {
            const QString orientation = pageSetup.value(QLatin1String("Orientation")).toString();
            if (orientation == QLatin1String("portrait") || orientation == QLatin1String("revers-portrait")) {
                writer.setPageOrientation(QPageLayout::Portrait);
            } else if (orientation == QLatin1String("landscape") || orientation == QLatin1String("reverse-landscape")) {
                writer.setPageOrientation(QPageLayout::Landscape);
            }
        }

        if (pageSetup.contains(QLatin1String("MarginTop")) &&
            pageSetup.contains(QLatin1String("MarginBottom")) &&
            pageSetup.contains(QLatin1String("MarginLeft")) &&
            pageSetup.contains(QLatin1String("MarginRight"))) {
            const int marginTop = pageSetup.value(QLatin1String("MarginTop")).toInt();
            const int marginBottom = pageSetup.value(QLatin1String("MarginBottom")).toInt();
            const int marginLeft = pageSetup.value(QLatin1String("MarginLeft")).toInt();
            const int marginRight = pageSetup.value(QLatin1String("MarginRight")).toInt();
            writer.setPageMargins(QMarginsF(marginLeft, marginTop, marginRight, marginBottom), QPageLayout::Millimeter);
        }

        // TODO num-copies, pages

        writer.setPageSize(QPagedPaintDevice::A4);

        painter.drawPixmap(QPoint(0,0), QPixmap(m_mainWindow->selectedFiles->text()));
        painter.end();

        // Send it back for printing
        const QString parentWindowId = QLatin1String("x11:") + QString::number(winId());
        QDBusUnixFileDescriptor descriptor(tempFile.handle());

        QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String("org.freedesktop.portal.Desktop"),
                                                            QLatin1String("/org/freedesktop/portal/desktop"),
                                                            QLatin1String("org.freedesktop.portal.Print"),
                                                            QLatin1String("Print"));

        message << parentWindowId << QLatin1String("Print dialog") << QVariant::fromValue<QDBusUnixFileDescriptor>(descriptor) << QVariantMap{{QLatin1String("token"), results.value(QLatin1String("token")).toUInt()}};

        QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pendingCall);
        connect(watcher, &QDBusPendingCallWatcher::finished, [this] (QDBusPendingCallWatcher *watcher) {
            QDBusPendingReply<QDBusObjectPath> reply = *watcher;
            if (reply.isError()) {
                qWarning() << "Couldn't get reply";
                qWarning() << "Error: " << reply.error().message();
            } else {
                QDBusConnection::sessionBus().connect(QString(),
                                                    reply.value().path(),
                                                    QLatin1String("org.freedesktop.portal.Request"),
                                                    QLatin1String("Response"),
                                                    this,
                                                    SLOT(gotPrintResponse(uint,QVariantMap)));
            }
        });
    } else {
        qWarning() << "Failed to print selected document";
    }
}

void PortalTest::inhibitRequested()
{
    const QString parentWindowId = QLatin1String("x11:") + QString::number(winId());

    QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String("org.freedesktop.portal.Desktop"),
                                                          QLatin1String("/org/freedesktop/portal/desktop"),
                                                          QLatin1String("org.freedesktop.portal.Inhibit"),
                                                          QLatin1String("Inhibit"));
    // flags: 1 (logout) & 2 (user switch) & 4 (suspend) & 8 (idle)
    message << parentWindowId << (uint)8 << QVariantMap({{QLatin1String("reason"), QLatin1String("Testing inhibition")}});

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pendingCall);
    connect(watcher, &QDBusPendingCallWatcher::finished, [this] (QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        } else {
            qWarning() << reply.value().path();
            m_mainWindow->inhibitLabel->setText(QLatin1String("Inhibited"));
            m_mainWindow->inhibit->setEnabled(false);
            m_mainWindow->uninhibit->setEnabled(true);
            m_inhibitionRequest = reply.value();
        }
    });
}

void PortalTest::uninhibitRequested()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String("org.freedesktop.portal.Desktop"),
                                                          m_inhibitionRequest.path(),
                                                          QLatin1String("org.freedesktop.portal.Request"),
                                                          QLatin1String("Close"));
    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    m_mainWindow->inhibitLabel->setText(QLatin1String("Not inhibited"));
    m_mainWindow->inhibit->setEnabled(true);
    m_mainWindow->uninhibit->setEnabled(false);
    m_inhibitionRequest = QDBusObjectPath();
}

void PortalTest::printDocument()
{
    const QString parentWindowId = QLatin1String("x11:") + QString::number(winId());

    QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String("org.freedesktop.portal.Desktop"),
                                                          QLatin1String("/org/freedesktop/portal/desktop"),
                                                          QLatin1String("org.freedesktop.portal.Print"),
                                                          QLatin1String("PreparePrint"));
    // TODO add some default configuration to verify it's read/parsed properly
    message << parentWindowId << QLatin1String("Prepare print") << QVariantMap() << QVariantMap() << QVariantMap();

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pendingCall);
    connect(watcher, &QDBusPendingCallWatcher::finished, [this] (QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        } else {
            QDBusConnection::sessionBus().connect(QString(),
                                                  reply.value().path(),
                                                  QLatin1String("org.freedesktop.portal.Request"),
                                                  QLatin1String("Response"),
                                                  this,
                                                  SLOT(gotPreparePrintResponse(uint,QVariantMap)));
        }
    });
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

