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

#ifndef PORTAL_TEST_KDE_H
#define PORTAL_TEST_KDE_H

#include <QMainWindow>
#include <QLoggingCategory>

namespace Ui
{
class PortalTest;
}

Q_DECLARE_LOGGING_CATEGORY(PortalTestKde)

class PortalTest : public QMainWindow
{
    Q_OBJECT
public:
    PortalTest(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~PortalTest();

public Q_SLOTS:
    void notificationActivated(uint action);
    void openFileRequested();
    void saveFileRequested();
    void sendNotification();
private:
    bool isRunningSandbox();

    Ui::PortalTest * m_mainWindow;
};

#endif // PORTAL_TEST_KDE_H


