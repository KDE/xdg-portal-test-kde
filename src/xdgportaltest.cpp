/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2022 Red Hat Inc
 * SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>
 * SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
 */

#include "xdgportaltest.h"

#include <QBuffer>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
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

#include <KIO/OpenUrlJob>
#include <KNotification>
#include <KWindowSystem>

#include <gst/gst.h>
#include <optional>

#include "dropsite/dropsitewindow.h"
#include <globalshortcuts_portal_interface.h>
#include <portalsrequest_interface.h>

#include "xdgexporterv2.h"

Q_LOGGING_CATEGORY(XdgPortalTestKde, "xdg-portal-test-kde")

Q_DECLARE_METATYPE(XdgPortalTest::Stream);
Q_DECLARE_METATYPE(XdgPortalTest::Streams);

struct PortalIcon {
    QString str;
    QDBusVariant data;

    static void registerDBusType()
    {
        qDBusRegisterMetaType<PortalIcon>();
    }
};
Q_DECLARE_METATYPE(PortalIcon);

QDBusArgument &operator<<(QDBusArgument &argument, const PortalIcon &icon)
{
    argument.beginStructure();
    argument << icon.str << icon.data;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalIcon &icon)
{
    argument.beginStructure();
    argument >> icon.str >> icon.data;
    argument.endStructure();
    return argument;
}

/// a(sa{sv})
using Shortcuts = QList<QPair<QString, QVariantMap>>;

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
    case KWindowSystem::Platform::Wayland:
        if (!m_xdgExported) {
            qDebug() << "nope!";
            return {};
        }
        return QLatin1String("wayland:") + *m_xdgExported->handle();
    case KWindowSystem::Platform::Unknown:
        break;
    }
    return {};
}

XdgPortalTest::XdgPortalTest(QWidget *parent, Qt::WindowFlags f)
    : QMainWindow(parent, f)
    , m_mainWindow(std::make_unique<Ui::XdgPortalTest>())
    , m_sessionTokenCounter(0)
    , m_requestTokenCounter(0)
{
    qDBusRegisterMetaType<Shortcuts>();
    qDBusRegisterMetaType<QPair<QString,QVariantMap>>();

    QLoggingCategory::setFilterRules(QStringLiteral("xdg-portal-test-kde.debug = true"));
    PortalIcon::registerDBusType();

    m_mainWindow->setupUi(this);

    auto dropSiteLayout = new QVBoxLayout(m_mainWindow->dropSite);
    auto dropSite = new DropSiteWindow(m_mainWindow->dropSite);
    dropSiteLayout->addWidget(dropSite);

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
        auto job = new KIO::OpenUrlJob(m_mainWindow->kurlrequester->url());
        job->start();
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
    connect(m_mainWindow->notifyWithDefault, &QPushButton::clicked, this, &XdgPortalTest::sendNotificationDefault);
    connect(m_mainWindow->printButton, &QPushButton::clicked, this, &XdgPortalTest::printDocument);
    connect(m_mainWindow->requestDeviceAccess, &QPushButton::clicked, this, &XdgPortalTest::requestDeviceAccess);
    connect(m_mainWindow->screenShareButton, &QPushButton::clicked, this, &XdgPortalTest::requestScreenSharing);
    connect(m_mainWindow->screenshotButton, &QPushButton::clicked, this, &XdgPortalTest::requestScreenshot);
    connect(m_mainWindow->accountButton, &QPushButton::clicked, this, &XdgPortalTest::requestAccount);
    connect(m_mainWindow->appChooserButton, &QPushButton::clicked, this, &XdgPortalTest::chooseApplication);
    connect(m_mainWindow->webAppButton, &QPushButton::clicked, this, &XdgPortalTest::addLauncher);
    connect(m_mainWindow->removeWebAppButton, &QPushButton::clicked, this, &XdgPortalTest::removeLauncher);

    // launcher buttons only work correctly inside sandboxes
    m_mainWindow->webAppButton->setEnabled(isRunningSandbox());
    m_mainWindow->removeWebAppButton->setEnabled(isRunningSandbox());
    connect(m_mainWindow->configureShortcuts, &QPushButton::clicked, this, &XdgPortalTest::configureShortcuts);

    connect(m_mainWindow->openFileButton, &QPushButton::clicked, this, [this] () {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_mainWindow->selectedFiles->text().split(",").first()));
    });

    m_shortcuts = new OrgFreedesktopPortalGlobalShortcutsInterface(QLatin1String("org.freedesktop.portal.Desktop"),
                                                                      QLatin1String("/org/freedesktop/portal/desktop"),
                                                                      QDBusConnection::sessionBus(), this);

    connect(m_shortcuts, &OrgFreedesktopPortalGlobalShortcutsInterface::Activated, this, [this] (const QDBusObjectPath &session_handle, const QString &shortcut_id, qulonglong timestamp, const QVariantMap &options) {
        qDebug() << "activated" << session_handle.path() << shortcut_id << timestamp << options;
        m_mainWindow->shortcutState->setText(QStringLiteral("Active!"));
    });
    connect(m_shortcuts, &OrgFreedesktopPortalGlobalShortcutsInterface::Deactivated, this, [this] {
        m_mainWindow->shortcutState->setText(QStringLiteral("Deactivated!"));
    });

    Shortcuts initialShortcuts = {
        { QStringLiteral("AwesomeTrigger"), { { QStringLiteral("description"), QStringLiteral("Awesome Description") } } }
    };
    QDBusArgument arg;
    arg << initialShortcuts;
    auto reply = m_shortcuts->CreateSession({
        { QLatin1String("session_handle_token"), "XdpPortalTest" },
        { QLatin1String("handle_token"), getRequestToken() },
        { QLatin1String("shortcuts"), QVariant::fromValue(arg) },
    });
    reply.waitForFinished();
    if (reply.isError()) {
        qWarning() << "Couldn't get reply";
        qWarning() << "Error:" << reply.error().message();
        m_mainWindow->shortcutsDescriptions->setText(reply.error().message());
    } else {
        QDBusConnection::sessionBus().connect(QString(),
                                            reply.value().path(),
                                            QLatin1String("org.freedesktop.portal.Request"),
                                            QLatin1String("Response"),
                                            this,
                                            SLOT(gotGlobalShortcutsCreateSessionResponse(uint,QVariantMap)));
    }

    gst_init(nullptr, nullptr);

    m_xdgExporter.reset(new XdgExporterV2);
    m_xdgExported = m_xdgExporter->exportWidget(this);
}

XdgPortalTest::~XdgPortalTest()
{
}

void XdgPortalTest::notificationActivated(const QString &action)
{
    m_mainWindow->notificationResponse->setText(QString("%1 activated").arg(action));
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
        QDBusUnixFileDescriptor descriptor(tempFile.handle());

        QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                            desktopPortalPath(),
                                                            QLatin1String("org.freedesktop.portal.Print"),
                                                            QLatin1String("Print"));

        message << parentWindowId() << QLatin1String("Print dialog") << QVariant::fromValue<QDBusUnixFileDescriptor>(descriptor) << QVariantMap{{QLatin1String("token"), results.value(QLatin1String("token")).toUInt()}, { QLatin1String("handle_token"), getRequestToken() }};

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
    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.Inhibit"),
                                                          QLatin1String("Inhibit"));
    // flags: 1 (logout) & 2 (user switch) & 4 (suspend) & 8 (idle)
    message << parentWindowId() << 8U << QVariantMap({{QLatin1String("reason"), QLatin1String("Testing inhibition")}});

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
    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.Print"),
                                                          QLatin1String("PreparePrint"));
    // TODO add some default configuration to verify it's read/parsed properly
    message << parentWindowId() << QLatin1String("Prepare print") << QVariantMap() << QVariantMap() << QVariantMap{ {QLatin1String("handle_token"), getRequestToken()} };

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
        watcher->deleteLater();
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
    auto notify = new KNotification(QLatin1String("notification"));
    connect(m_mainWindow->notifyCloseButton, &QPushButton::clicked, notify, &KNotification::close);
    connect(notify, &KNotification::closed, this, [this] () {
        m_mainWindow->notifyCloseButton->setDisabled(true);
    });

    notify->setFlags(KNotification::DefaultEvent);
    notify->setTitle(QLatin1String("Notification test"));
    notify->setText(QLatin1String("<html><b>Hello world!!<b><html>"));
    KNotificationAction *action1 = notify->addAction(QStringLiteral("Action 1"));
    KNotificationAction *action2 = notify->addAction(QStringLiteral("Action 2"));
    connect(action1, &KNotificationAction::activated, this, [this, action1] () {
        this->notificationActivated(action1->label());
    });
    connect(action2, &KNotificationAction::activated, this, [this, action2] () {
        this->notificationActivated(action2->label());
    });
    notify->setIconName(QLatin1String("applications-development"));

    m_mainWindow->notifyCloseButton->setEnabled(true);
    notify->sendEvent();
}

void XdgPortalTest::sendNotificationPixmap()
{
    auto notify = new KNotification(QLatin1String("notification"));
    connect(m_mainWindow->notifyCloseButton, &QPushButton::clicked, notify, &KNotification::close);
    connect(notify, &KNotification::closed, this, [this] () {
        m_mainWindow->notifyCloseButton->setDisabled(true);
    });

    notify->setFlags(KNotification::DefaultEvent);
    notify->setTitle(QLatin1String("Notification test"));
    notify->setText(QLatin1String("<html><b>Hello world!!<b><html>"));
    KNotificationAction *action1 = notify->addAction(QStringLiteral("Action 1"));
    KNotificationAction *action2 = notify->addAction(QStringLiteral("Action 2"));
    connect(action1, &KNotificationAction::activated, this, [this, action1] () {
        this->notificationActivated(action1->label());
    });
    connect(action2, &KNotificationAction::activated, this, [this, action2] () {
        this->notificationActivated(action2->label());
    });

    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::red);

    notify->setPixmap(pixmap);

    m_mainWindow->notifyCloseButton->setEnabled(true);
    notify->sendEvent();
}

void XdgPortalTest::sendNotificationDefault()
{
    auto notify = new KNotification(QLatin1String("notification"));
    connect(m_mainWindow->notifyCloseButton, &QPushButton::clicked, notify, &KNotification::close);
    connect(notify, &KNotification::closed, this, [this] () {
        m_mainWindow->notifyCloseButton->setDisabled(true);
    });

    notify->setFlags(KNotification::DefaultEvent);
    notify->setTitle(QLatin1String("Notification test"));
    notify->setText(QLatin1String("<html><b>Hello world!!<b><html>"));
    KNotificationAction *action1 = notify->addAction(QStringLiteral("Action 1"));
    KNotificationAction *action2 = notify->addAction(QStringLiteral("Action 2"));
    KNotificationAction *actionDefault = notify->addDefaultAction(QStringLiteral("Default action"));
    connect(action1, &KNotificationAction::activated, this, [this, action1] () {
        this->notificationActivated(action1->label());
    });
    connect(action2, &KNotificationAction::activated, this, [this, action2] () {
        this->notificationActivated(action2->label());
    });
    connect(actionDefault, &KNotificationAction::activated, this, [this, actionDefault] () {
        this->notificationActivated(actionDefault->label());
    });

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
    message << parentWindowId() << QVariantMap{{QLatin1String("interactive"), true}, {QLatin1String("handle_token"), getRequestToken()}};

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
    message << parentWindowId() << QVariantMap{{QLatin1String("interactive"), true}, {QLatin1String("handle_token"), getRequestToken()}};

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
            << parentWindowId()
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
    for (const auto &stream : streams) {
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

void XdgPortalTest::addLauncher()
{
    qDebug() << getSessionToken() << getRequestToken();
    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.DynamicLauncher"),
                                                          QLatin1String("PrepareInstall"));

    QBuffer buffer;
    static constexpr auto maxSize = 512;
    QIcon::fromTheme("utilities-terminal").pixmap(maxSize, maxSize).save(&buffer,"PNG");
    PortalIcon icon {QStringLiteral("bytes"), QDBusVariant(buffer.buffer())};

    message << parentWindowId() << QStringLiteral("Patschen")
            << QVariant::fromValue(QDBusVariant(QVariant::fromValue(icon)))
            << QVariantMap {{QStringLiteral("launcher_type"), 2U},
                            {QStringLiteral("target"), QStringLiteral("https://kde.org")},
                            {QStringLiteral("editable_icon"), true}};

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this] (QDBusPendingCallWatcher *watcher) {
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
                                                  SLOT(gotLauncher(uint,QVariantMap)));
        }
    });
}

void XdgPortalTest::gotLauncher(uint response, const QVariantMap &results)
{
    qDebug() << response << results;

    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.DynamicLauncher"),
                                                          QLatin1String("Install"));

    QFile desktopFile(":/data/patschen.desktop");
    Q_ASSERT(desktopFile.open(QFile::ReadOnly));
    auto data = desktopFile.readAll();

    message << results.value(QStringLiteral("token")) << QStringLiteral("org.kde.xdg-portal-test-kde.patschen.desktop")
            << QString::fromUtf8(data) << QVariantMap {};

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        }
    });
}

void XdgPortalTest::removeLauncher()
{
    QDBusMessage message = QDBusMessage::createMethodCall(desktopPortalService(),
                                                          desktopPortalPath(),
                                                          QLatin1String("org.freedesktop.portal.DynamicLauncher"),
                                                          QLatin1String("Uninstall"));


    message << QStringLiteral("org.kde.xdg-portal-test-kde.patschen.desktop") << QVariantMap {};

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        }
    });
}

void XdgPortalTest::gotGlobalShortcutsCreateSessionResponse(uint res, const QVariantMap& results)
{
    if (res != 0) {
        qWarning() << "failed to create a global shortcuts session" << res << results;
        return;
    }

    m_globalShortcutsSession = QDBusObjectPath(results["session_handle"].toString());

    auto reply = m_shortcuts->ListShortcuts(m_globalShortcutsSession, {});
    reply.waitForFinished();
    if (reply.isError()) {
        qWarning() << "failed to call ListShortcuts" << reply.error();
        return;
    }

    auto req = new OrgFreedesktopPortalRequestInterface(QLatin1String("org.freedesktop.portal.Desktop"),
                                                        reply.value().path(), QDBusConnection::sessionBus(), this);

    // BindShortcuts and ListShortcuts answer the same
    connect(req, &OrgFreedesktopPortalRequestInterface::Response, this, &XdgPortalTest::gotListShortcutsResponse);
    connect(req, &OrgFreedesktopPortalRequestInterface::Response, req, &QObject::deleteLater);
}

void XdgPortalTest::gotListShortcutsResponse(uint code, const QVariantMap& results)
{
    if (code != 0) {
        qDebug() << "failed to get the list of shortcuts" << code << results;
        return;
    }

    if (!results.contains("shortcuts")) {
        qWarning() << "no shortcuts reply" << results;
        return;
    }

    Shortcuts s;
    const auto arg = results["shortcuts"].value<QDBusArgument>();
    arg >> s;
    QString desc;
    for (auto it = s.cbegin(), itEnd = s.cend(); it != itEnd; ++it) {
        desc += i18n("%1: %2 %3", it->first, it->second["description"].toString(), it->second["trigger_description"].toString());
    }
    m_mainWindow->shortcutsDescriptions->setText(desc);
}

void XdgPortalTest::configureShortcuts()
{
    auto reply = m_shortcuts->BindShortcuts(m_globalShortcutsSession, {},  parentWindowId(), { { "handle_token", getRequestToken() } });
    reply.waitForFinished();
    if (reply.isError()) {
        qWarning() << "failed to call BindShortcuts" << reply.error();
        return;
    }

    auto req = new OrgFreedesktopPortalRequestInterface(QLatin1String("org.freedesktop.portal.Desktop"),
                                                        reply.value().path(), QDBusConnection::sessionBus(), this);

    // BindShortcuts and ListShortcuts answer the same
    connect(req, &OrgFreedesktopPortalRequestInterface::Response, this, &XdgPortalTest::gotListShortcutsResponse);
}
