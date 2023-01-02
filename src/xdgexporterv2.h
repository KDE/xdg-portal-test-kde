/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>
 */

#pragma once

#include <QPointer>
#include <QWaylandClientExtensionTemplate>
#include "qwayland-xdg-foreign-unstable-v2.h"
#include <optional>

class QWindow;
class XdgExporterV2;

class XdgExportedV2 : public QObject, public QtWayland::zxdg_exported_v2
{
public:
    XdgExportedV2(XdgExporterV2* exporter);
    ~XdgExportedV2();

    std::optional<QString> handle() const;
    void setWindow(QWindow *window);

private:
    void zxdg_exported_v2_handle(const QString &handle) override {
        m_handle = handle;
    }
    void useWindow(QWindow *window);

    std::optional<QString> m_handle;
    XdgExporterV2 *const m_exporter;
};

class XdgExporterV2 : public QWaylandClientExtensionTemplate<XdgExporterV2>, public QtWayland::zxdg_exporter_v2
{
public:
    XdgExporterV2();
    ~XdgExporterV2();

    XdgExportedV2 *exportWindow(QWindow *window);
    XdgExportedV2 *exportWidget(QWidget *widget);
};
