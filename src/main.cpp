/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2022 Red Hat Inc
 * SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>
 */

#include <QApplication>

#include <KAboutData>

#include "xdgportaltest.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    KAboutData about(QStringLiteral("xdg-portal-test-kde"), QStringLiteral("Portal Test KDE"), QString());
    KAboutData::setApplicationData(about);

    XdgPortalTest xdgPortalTest;
    xdgPortalTest.show();

    return a.exec();
}

