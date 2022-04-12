/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2022 Red Hat Inc
 * SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>
 * SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
 */

#ifndef XDG_PORTAL_TEST_KDE_H
#define XDG_PORTAL_TEST_KDE_H

#include <QDBusObjectPath>
#include <QFlags>
#include <QLoggingCategory>
#include <QMainWindow>

class QDBusMessage;

namespace Ui
{
class XdgPortalTest;
}

Q_DECLARE_LOGGING_CATEGORY(XdgPortalTestKde)

class XdgPortalTest : public QMainWindow
{
    Q_OBJECT
public:
    typedef struct {
        uint node_id;
        QVariantMap map;
    } Stream;
    typedef QList<Stream> Streams;

    XdgPortalTest(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~XdgPortalTest();

public Q_SLOTS:
    void gotCreateSessionResponse(uint response, const QVariantMap &results);
    void gotSelectSourcesResponse(uint response, const QVariantMap &results);
    void gotStartResponse(uint response, const QVariantMap &results);
    void gotPrintResponse(uint response, const QVariantMap &results);
    void gotPreparePrintResponse(uint response, const QVariantMap &results);
    void gotScreenshotResponse(uint response, const QVariantMap &results);
    void gotAccountResponse(uint response, const QVariantMap &results);
    void inhibitRequested();
    void uninhibitRequested();
    void notificationActivated(uint action);
    void openFileRequested();
    void openFileModalRequested();
    void openDirRequested();
    void openDirModalRequested();
    void printDocument();
    void requestDeviceAccess();
    void saveFileRequested();
    void sendNotification();
    void sendNotificationPixmap();
    void requestScreenSharing();
    void requestScreenshot();
    void requestAccount();
    void chooseApplication();
    void gotApplicationChoice(uint response, const QVariantMap &results);
    void addLauncher();
    void gotLauncher(uint response, const QVariantMap &results);
    void removeLauncher();

private:
    bool isRunningSandbox();
    QString getSessionToken();
    QString getRequestToken();

    QString parentWindowId() const;

    QDBusObjectPath m_inhibitionRequest;
    QString m_session;
    Ui::XdgPortalTest * m_mainWindow;
    uint m_sessionTokenCounter;
    uint m_requestTokenCounter;
};

#endif // XDG_PORTAL_TEST_KDE_H


