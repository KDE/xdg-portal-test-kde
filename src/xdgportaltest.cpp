/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2022 Red Hat Inc
 * SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>
 * SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
 */

#include "xdgportaltest.h"
#include "ui_xdgportaltest.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusUnixFileDescriptor>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPdfWriter>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTemporaryFile>
#include <QWindow>
#include <KRun>
#include <QDesktopServices>

#include <KWindowSystem>
#include <KNotification>

#include <gst/gst.h>

Q_LOGGING_CATEGORY(XdgPortalTestKde, "xdg-portal-test-kde")

Q_DECLARE_METATYPE(XdgPortalTest::Stream);
Q_DECLARE_METATYPE(XdgPortalTest::Streams);

const QDBusArgument &operator >> (const QDBusArgument &arg, XdgPortalTest::Stream &stream)
{
    arg.beginStructure();
    arg >> stream.node_id;

    arg.beginMap();
    while (!arg.atEnd()) {
        QString key;
        QVariant map;
        arg.beginMapEntry();
        arg >> key >> map;
        arg.endMapEntry();
        stream.map.insert(key, map);
    }
    arg.endMap();
    arg.endStructure();

    return arg;
}

static QString desktopPortalService()
{
    return QStringLiteral("org.freedesktop.portal.Desktop");
}

static QString desktopPortalPath()
{
    return QStringLiteral("/org/freedesktop/portal/desktop");
}

static QString portalRequestInterface()
{
    return QStringLiteral("org.freedesktop.portal.Request");
}

static QString portalRequestResponse()
{
    return QStringLiteral("Response");
}

QString XdgPortalTest::parentWindowId() const
{
    switch (KWindowSystem::platform()) {
    case KWindowSystem::Platform::X11:
        return QLatin1String("x11:") + QString::number(winId());
    // TODO case KWindowSystem::Platform::Wayland:
    case KWindowSystem::Platform::Unknown:
        break;
    }
    return {};
}

XdgPortalTest::XdgPortalTest(QWidget *parent, Qt::WindowFlags f)
    : QMainWindow(parent, f)
    , m_mainWindow(new Ui::XdgPortalTest)
    , m_sessionTokenCounter(0)
    , m_requestTokenCounter(0)
{
    QLoggingCategory::setFilterRules(QStringLiteral("xdg-portal-test-kde.debug = true"));

    m_mainWindow->setupUi(this);

    m_mainWindow->sandboxLabel->setText(isRunningSandbox() ? QLatin1String("yes") : QLatin1String("no"));
    m_mainWindow->printWarning->setText(QLatin1String("Select an image in JPG format using FileChooser part!!"));

    auto menubar = new QMenuBar(this);
    setMenuBar(menubar);

    auto menu = new QMenu(QLatin1String("File"), menubar);
    menu->addAction(QIcon::fromTheme(QLatin1String("application-exit")), QLatin1String("Quit"), qApp, &QApplication::quit);
    menubar->insertMenu(nullptr, menu);

    auto trayIcon = new QSystemTrayIcon(QIcon::fromTheme(QLatin1String("kde")), this);
    trayIcon->setContextMenu(menu);
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated, this, [this] (QSystemTrayIcon::ActivationReason reason) {
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

    connect(m_mainWindow->krun, &QPushButton::clicked, this, [this] {
        new KRun(m_mainWindow->kurlrequester->url(), this);
    });
    connect(m_mainWindow->openurl, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(m_mainWindow->kurlrequester->url());
    });
    connect(m_mainWindow->inhibit, &QPushButton::clicked, this, &XdgPortalTest::inhibitRequested);
    connect(m_mainWindow->uninhibit, &QPushButton::clicked, this, &XdgPortalTest::uninhibitRequested);
    connect(m_mainWindow->openFile, &QPushButton::clicked, this, &XdgPortalTest::openFileRequested);
    connect(m_mainWindow->openFileModal, &QPushButton::clicked, this, &XdgPortalTest::openFileModalRequested);
    connect(m_mainWindow->saveFile, &QPushButton::clicked, this, &XdgPortalTest::saveFileRequested);
    connect(m_mainWindow->openDir, &QPushButton::clicked, this, &XdgPortalTest::openDirRequested);
    connect(m_mainWindow->openDirModal, &QPushButton::clicked, this, &XdgPortalTest::openDirModalRequested);
    connect(m_mainWindow->notifyButton, &QPushButton::clicked, this, &XdgPortalTest::sendNotification);
    connect(m_mainWindow->notifyPixmapButton, &QPushButton::clicked, this, &XdgPortalTest::sendNotificationPixmap);
    connect(m_mainWindow->printButton, &QPushButton::clicked, this, &XdgPortalTest::printDocument);
    connect(m_mainWindow->requestDeviceAccess, &QPushButton::clicked, this, &XdgPortalTest::requestDeviceAccess);
    connect(m_mainWindow->screenShareButton, &QPushButton::clicked, this, &XdgPortalTest::requestScreenSharing);
    connect(m_mainWindow->screenshotButton, &QPushButton::clicked, this, &XdgPortalTest::requestScreenshot);
    connect(m_mainWindow->accountButton, &QPushButton::clicked, this, &XdgPortalTest::requestAccount);
    connect(m_mainWindow->appChooserButton, &QPushButton::clicked, this, &XdgPortalTest::chooseApplication);

    connect(m_mainWindow->openFileButton, &QPushButton::clicked, this, [this] () {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_mainWindow->selectedFiles->text().split(",").first()));
    });

    gst_init(nullptr, nullptr);
}

XdgPortalTest::~XdgPortalTest()
{
    delete m_mainWindow;
}

void XdgPortalTest::notificationActivated(uint action)
{
    m_mainWindow->notificationResponse->setText(QString("Action number %1 activated").arg(QString::number(action)));
}

void XdgPortalTest::openFileRequested()
{
    auto fileDialog = new QFileDialog(this);
    fileDialog->setFileMode(QFileDialog::ExistingFiles);
    fileDialog->setLabelText(QFileDialog::Accept, QLatin1String("Open (portal)"));
    fileDialog->setModal(false);
    fileDialog->setWindowTitle(QLatin1String("Flatpak test - open dialog"));
    fileDialog->setMimeTypeFilters(QStringList { QLatin1String("text/plain"), QLatin1String("image/jpeg") } );
    connect(fileDialog, &QFileDialog::accepted, this, [this, fileDialog] () {
        if (!fileDialog->selectedFiles().isEmpty()) {
            m_mainWindow->selectedFiles->setText(fileDialog->selectedFiles().join(QLatin1String(", ")));
            if (fileDialog->selectedFiles().first().endsWith(QLatin1String(".jpg"))) {
                m_mainWindow->printButton->setEnabled(true);
                m_mainWindow->printWarning->setVisible(false);
            } else {
                m_mainWindow->printButton->setEnabled(false);
                m_mainWindow->printWarning->setVisible(true);
            }
        }
        m_mainWindow->openFileButton->setEnabled(true);
        fileDialog->deleteLater();
    });
    fileDialog->show();
}

void XdgPortalTest::openFileModalRequested()
{
    auto fileDialog = new QFileDialog(this);
    fileDialog->setFileMode(QFileDialog::ExistingFiles);
    fileDialog->setNameFilter(QLatin1String("*.txt"));
    fileDialog->setLabelText(QFileDialog::Accept, QLatin1String("Open (portal)"));
    fileDialog->setModal(false);
    fileDialog->setWindowTitle(QLatin1String("Flatpak test - open dialog"));

    if (fileDialog->exec() == QDialog::Accepted) {
        if (!fileDialog->selectedFiles().isEmpty()) {
            m_mainWindow->selectedFiles->setText(fileDialog->selectedFiles().join(QLatin1String(", ")));
            if (fileDialog->selectedFiles().first().endsWith(QLatin1String(".jpg"))) {
                m_mainWindow->printButton->setEnabled(true);
                m_mainWindow->printWarning->setVisible(false);
            } else {
                m_mainWindow->printButton->setEnabled(false);
                m_mainWindow->printWarning->setVisible(true);
            }
        }
        m_mainWindow->openFileButton->setEnabled(true);
        fileDialog->deleteLater();
    }
}

void XdgPortalTest::openDirRequested()
{
    auto fileDialog = new QFileDialog(this);
    fileDialog->setFileMode(QFileDialog::Directory);
    fileDialog->setLabelText(QFileDialog::Accept, QLatin1String("Open (portal)"));
    fileDialog->setModal(false);
    fileDialog->setWindowTitle(QLatin1String("Flatpak test - open directory dialog"));

    connect(fileDialog, &QFileDialog::accepted, this, [this, fileDialog] () {
        m_mainWindow->selectedDir->setText(fileDialog->selectedFiles().join(QLatin1String(", ")));
        fileDialog->deleteLater();
    });
    fileDialog->show();
}

void XdgPortalTest::openDirModalRequested()
{
    auto fileDialog = new QFileDialog(this);
    fileDialog->setFileMode(QFileDialog::Directory);
    fileDialog->setLabelText(QFileDialog::Accept, QLatin1String("Open (portal)"));
    fileDialog->setModal(false);
    fileDialog->setWindowTitle(QLatin1String("Flatpak test - open directory dialog"));

    if (fileDialog->exec() == QDialog::Accepted) {
        m_mainWindow->selectedDir->setText(fileDialog->selectedFiles().join(QLatin1String(", ")));
        fileDialog->deleteLater();
    }
}

void XdgPortalTest::gotPrintResponse(uint response, const QVariantMap &results)
{
    // TODO do cleaning
    qWarning() << response << results;
}

void XdgPortalTest::gotPreparePrintResponse(uint response, const QVariantMap &results)
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

        writer.setPageSize(QPageSize(QPageSize::A4));

        painter.drawPixmap(QPoint(0,0), QPixmap(m_mainWindow->selectedFiles->text()));
        painter.end();

        // Send it back for printing
        const QString parentWindowId = QLatin1String("x11:") + QString::number(winId());
        QDBusUnixFileDescriptor descriptor(tempFile.handle());

        QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                            desktopPortalPath(),
                                                            QLatin1String("org.freedesktop.portal.Print"),
                                                            QLatin1String("Print"));

        message << parentWindowId << QLatin1String("Print dialog") << QVariant::fromValue<QDBusUnixFileDescriptor>(descriptor) << QVariantMap{{QLatin1String("token"), results.value(QLatin1String("token")).toUInt()}, { QLatin1String("handle_token"), getRequestToken() }};

        QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
        auto watcher = new QDBusPendingCallWatcher(pendingCall);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this] (QDBusPendingCallWatcher *watcher) {
            QDBusPendingReply<QDBusObjectPath> reply = *watcher;
            if (reply.isError()) {
                qWarning() << "Couldn't get reply";
                qWarning() << "Error: " << reply.error().message();
            } else {
                QDBusConnection::sessionBus().connect(desktopPortalService(),
                                                    reply.value().path(),
                                                    portalRequestInterface(),
                                                    portalRequestResponse(),
                                                    this,
                                                    SLOT(gotPrintResponse(uint,QVariantMap)));
            }
        });
    } else {
        qWarning() << "Failed to print selected document";
    }
}

void XdgPortalTest::inhibitRequested()
{
    const QString parentWindowId = QLatin1String("x11:") + QString::number(winId());

    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.Inhibit"),
                                                          QLatin1String("Inhibit"));
    // flags: 1 (logout) & 2 (user switch) & 4 (suspend) & 8 (idle)
    message << parentWindowId << 8U << QVariantMap({{QLatin1String("reason"), QLatin1String("Testing inhibition")}});

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this] (QDBusPendingCallWatcher *watcher) {
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

void XdgPortalTest::uninhibitRequested()
{
    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          m_inhibitionRequest.path(),
                                                          portalRequestInterface(),
                                                          QLatin1String("Close"));
    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    m_mainWindow->inhibitLabel->setText(QLatin1String("Not inhibited"));
    m_mainWindow->inhibit->setEnabled(true);
    m_mainWindow->uninhibit->setEnabled(false);
    m_inhibitionRequest = QDBusObjectPath();
}

void XdgPortalTest::printDocument()
{
    const QString parentWindowId = QLatin1String("x11:") + QString::number(winId());

    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.Print"),
                                                          QLatin1String("PreparePrint"));
    // TODO add some default configuration to verify it's read/parsed properly
    message << parentWindowId << QLatin1String("Prepare print") << QVariantMap() << QVariantMap() << QVariantMap{ {QLatin1String("handle_token"), getRequestToken()} };

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this] (QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        } else {
            QDBusConnection::sessionBus().connect(desktopPortalService(),
                                                  reply.value().path(),
                                                  portalRequestInterface(),
                                                  portalRequestResponse(),
                                                  this,
                                                  SLOT(gotPreparePrintResponse(uint,QVariantMap)));
        }
    });
}

void XdgPortalTest::requestDeviceAccess()
{
    qWarning() << "Request device access";
    const QString device = m_mainWindow->deviceCombobox->currentIndex() == 0 ? QLatin1String("microphone") :
                                                                               m_mainWindow->deviceCombobox->currentIndex() == 1 ? QLatin1String("speakers") : QLatin1String("camera");


    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.Device"),
                                                          QLatin1String("AccessDevice"));
    message << (uint)QApplication::applicationPid() << QStringList {device} << QVariantMap();

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [] (QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        } else {
            qWarning() << reply.value().path();
        }
    });
}

void XdgPortalTest::saveFileRequested()
{
    auto fileDialog = new QFileDialog(this);
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

void XdgPortalTest::sendNotification()
{
    auto notify = new KNotification(QLatin1String("notification"), this);
    connect(notify, static_cast<void (KNotification::*)(uint)>(&KNotification::activated), this, &XdgPortalTest::notificationActivated);
    connect(m_mainWindow->notifyCloseButton, &QPushButton::clicked, notify, &KNotification::close);
    connect(notify, &KNotification::closed, this, [this] () {
        m_mainWindow->notifyCloseButton->setDisabled(true);
    });

    notify->setFlags(KNotification::DefaultEvent);
    notify->setTitle(QLatin1String("Notification test"));
    notify->setText(QLatin1String("<html><b>Hello world!!<b><html>"));
    notify->setActions(QStringList { QStringLiteral("Action 1"), QStringLiteral("Action 2")});
    notify->setIconName(QLatin1String("applications-development"));

    m_mainWindow->notifyCloseButton->setEnabled(true);
    notify->sendEvent();
}

void XdgPortalTest::sendNotificationPixmap()
{
    auto notify = new KNotification(QLatin1String("notification"), this);
    connect(notify, static_cast<void (KNotification::*)(uint)>(&KNotification::activated), this, &XdgPortalTest::notificationActivated);
    connect(m_mainWindow->notifyCloseButton, &QPushButton::clicked, notify, &KNotification::close);
    connect(notify, &KNotification::closed, this, [this] () {
        m_mainWindow->notifyCloseButton->setDisabled(true);
    });

    notify->setFlags(KNotification::DefaultEvent);
    notify->setTitle(QLatin1String("Notification test"));
    notify->setText(QLatin1String("<html><b>Hello world!!<b><html>"));
    notify->setActions(QStringList { QStringLiteral("Action 1"), QStringLiteral("Action 2")});

    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::red);

    notify->setPixmap(pixmap);

    m_mainWindow->notifyCloseButton->setEnabled(true);
    notify->sendEvent();
}

void XdgPortalTest::requestScreenSharing()
{
    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.ScreenCast"),
                                                          QLatin1String("CreateSession"));

    message << QVariantMap { { QLatin1String("session_handle_token"), getSessionToken() }, { QLatin1String("handle_token"), getRequestToken() } };

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this] (QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        } else {
            QDBusConnection::sessionBus().connect(desktopPortalService(),
                                                reply.value().path(),
                                                portalRequestInterface(),
                                                portalRequestResponse(),
                                                this,
                                                SLOT(gotCreateSessionResponse(uint,QVariantMap)));
        }
    });
}

void XdgPortalTest::requestScreenshot()
{
    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.Screenshot"),
                                                          QLatin1String("Screenshot"));
    // TODO add some default configuration to verify it's read/parsed properly
    message << QLatin1String("x11:") << QVariantMap{{QLatin1String("interactive"), true}, {QLatin1String("handle_token"), getRequestToken()}};

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this] (QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        } else {
            QDBusConnection::sessionBus().connect(desktopPortalService(),
                                                  reply.value().path(),
                                                  portalRequestInterface(),
                                                  portalRequestResponse(),
                                                  this,
                                                  SLOT(gotScreenshotResponse(uint,QVariantMap)));
        }
    });
}

void XdgPortalTest::requestAccount()
{
    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.Account"),
                                                          QLatin1String("GetUserInformation"));
    // TODO add some default configuration to verify it's read/parsed properly
    message << QLatin1String("x11:") << QVariantMap{{QLatin1String("interactive"), true}, {QLatin1String("handle_token"), getRequestToken()}};

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this] (QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        } else {
            QDBusConnection::sessionBus().connect(desktopPortalService(),
                                                  reply.value().path(),
                                                  portalRequestInterface(),
                                                  portalRequestResponse(),
                                                  this,
                                                  SLOT(gotAccountResponse(uint,QVariantMap)));
        }
    });
}

void XdgPortalTest::gotCreateSessionResponse(uint response, const QVariantMap &results)
{
    if (response != 0) {
        qWarning() << "Failed to create session: " << response;
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.ScreenCast"),
                                                          QLatin1String("SelectSources"));

    m_session = results.value(QLatin1String("session_handle")).toString();

    message << QVariant::fromValue(QDBusObjectPath(m_session))
            << QVariantMap { { QLatin1String("multiple"), false},
                             { QLatin1String("types"), (uint)m_mainWindow->screenShareCombobox->currentIndex() + 1},
                             { QLatin1String("handle_token"), getRequestToken() } };

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this] (QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        } else {
            QDBusConnection::sessionBus().connect(desktopPortalService(),
                                                reply.value().path(),
                                                portalRequestInterface(),
                                                portalRequestResponse(),
                                                this,
                                                SLOT(gotSelectSourcesResponse(uint,QVariantMap)));
        }
    });
}

void XdgPortalTest::gotSelectSourcesResponse(uint response, const QVariantMap &results)
{
    if (response != 0) {
        qWarning() << "Failed to select sources: " << response;
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.ScreenCast"),
                                                          QLatin1String("Start"));

    message << QVariant::fromValue(QDBusObjectPath(m_session))
            << QString() // parent_window
            << QVariantMap { { QLatin1String("handle_token"), getRequestToken() } };

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this] (QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        } else {
            QDBusConnection::sessionBus().connect(desktopPortalService(),
                                                reply.value().path(),
                                                portalRequestInterface(),
                                                portalRequestResponse(),
                                                this,
                                                SLOT(gotStartResponse(uint,QVariantMap)));
        }
    });
}

void XdgPortalTest::gotStartResponse(uint response, const QVariantMap &results)
{
    if (response != 0) {
        qWarning() << "Failed to start: " << response;
    }

    Streams streams = qdbus_cast<Streams>(results.value(QLatin1String("streams")));
    Q_FOREACH (Stream stream, streams) {
        QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                              desktopPortalPath(),
                                                              QLatin1String("org.freedesktop.portal.ScreenCast"),
                                                              QLatin1String("OpenPipeWireRemote"));

        message << QVariant::fromValue(QDBusObjectPath(m_session)) << QVariantMap();

        QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
        pendingCall.waitForFinished();
        QDBusPendingReply<QDBusUnixFileDescriptor> reply = pendingCall.reply();
        if (reply.isError()) {
            qWarning() << "Failed to get fd for node_id " << stream.node_id;
        }

        QString gstLaunch = QString("pipewiresrc fd=%1 path=%2 ! videoconvert ! xvimagesink").arg(reply.value().fileDescriptor()).arg(stream.node_id);
        GstElement *element = gst_parse_launch(gstLaunch.toUtf8(), nullptr);
        gst_element_set_state(element, GST_STATE_PLAYING);
    }
}

void XdgPortalTest::gotScreenshotResponse(uint response, const QVariantMap& results)
{
    qWarning() << "Screenshot response: " << response << results;
    if (!response) {
        if (results.contains(QLatin1String("uri"))) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(results.value(QLatin1String("uri")).toString()));
        }
    } else {
        qWarning() << "Failed to take screenshot";
    }
}

void XdgPortalTest::gotAccountResponse(uint response, const QVariantMap& results)
{
    qWarning() << "Account response: " << response << results;
    if (!response) {
        QString resultsString = QStringLiteral("Response is:\n");
        const auto resultKeys = results.keys();
        for (const auto &key : resultKeys) {
            resultsString += "    " + key + ": " + results.value(key).toString() + "\n";
        }
        m_mainWindow->accountResultsLabel->setText(resultsString);
    } else {
        qWarning() << "Failed to get account information";
    }
}

bool XdgPortalTest::isRunningSandbox()
{
    QString runtimeDir = qgetenv("XDG_RUNTIME_DIR");

    if (runtimeDir.isEmpty()) {
        return false;
    }

    QFile file(runtimeDir + QLatin1String("/flatpak-info"));

    return file.exists();
}

QString XdgPortalTest::getSessionToken()
{
    m_sessionTokenCounter += 1;
    return QString("u%1").arg(m_sessionTokenCounter);
}

QString XdgPortalTest::getRequestToken()
{
    m_requestTokenCounter += 1;
    return QString("u%1").arg(m_requestTokenCounter);
}

void XdgPortalTest::chooseApplication()
{
    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QStringLiteral("org.freedesktop.portal.OpenURI"),
                                                          QStringLiteral("OpenURI"));

    message << parentWindowId() << QStringLiteral("https://kde.org") << QVariantMap{{QStringLiteral("ask"), true}};

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        } else {
            QDBusConnection::sessionBus().connect(desktopPortalService(),
                                                  reply.value().path(),
                                                  portalRequestInterface(),
                                                  portalRequestResponse(),
                                                  this,
                                                  SLOT(gotApplicationChoice(uint,QVariantMap)));
        }
    });
}

void XdgPortalTest::gotApplicationChoice(uint response, const QVariantMap &results)
{
    qDebug() << response << results;
}