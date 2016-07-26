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

#include <QFile>

Q_LOGGING_CATEGORY(PortalTestKde, "portal-test-kde")

PortalTest::PortalTest(QWidget *parent, Qt::WindowFlags f)
    : QMainWindow(parent, f)
    , m_mainWindow(new Ui::PortalTest)
{
    QLoggingCategory::setFilterRules(QStringLiteral("portal-test-kde.debug = true"));

    m_mainWindow->setupUi(this);

    m_mainWindow->sandboxLabel->setText(isRunningSandbox() ? QLatin1String("yes") : QLatin1String("no"));
}

PortalTest::~PortalTest()
{
    delete m_mainWindow;
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
