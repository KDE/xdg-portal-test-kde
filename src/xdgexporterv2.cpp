/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>
 */

#include "xdgexporterv2.h"

#include <qpa/qplatformnativeinterface.h>
#include <QDebug>
#include <QGuiApplication>
#include <QWindow>
#include <QWidget>

class WidgetWatcher : public QObject
{
public:
    WidgetWatcher(XdgExporterV2 *exporter, XdgExportedV2 *toExport, QWidget *theWidget)
        : QObject(theWidget)
        , m_toExport(toExport)
        , m_widget(theWidget)
        , m_exporter(exporter)
    {
        theWidget->installEventFilter(this);
    }

    ~WidgetWatcher() {
        delete m_toExport;
    }

    bool eventFilter(QObject * watched, QEvent * event) override {
        Q_ASSERT(watched == parent());
        if (event->type() == QEvent::PlatformSurface) {
            m_toExport->setWindow(m_widget->windowHandle());
        }
        return false;
    }

    QPointer<XdgExportedV2> m_toExport;
    QWidget *const m_widget;
    XdgExporterV2 *const m_exporter;
};

XdgExportedV2::XdgExportedV2(XdgExporterV2* exporter)
    : QtWayland::zxdg_exported_v2()
    , m_exporter(exporter)
{
}

XdgExportedV2::~XdgExportedV2()
{
    destroy();
}

void XdgExportedV2::setWindow(QWindow* window) {
    Q_ASSERT(window);

    useWindow(window);
    connect(window, &QWindow::visibilityChanged, this, [this, window] (QWindow::Visibility visibility) {
        if (visibility != QWindow::Hidden) {
            useWindow(window);
        }
    });
    connect(window, &QWindow::destroyed, this, &QObject::deleteLater);
}

void XdgExportedV2::useWindow(QWindow* window)
{
    QPlatformNativeInterface *nativeInterface = qGuiApp->platformNativeInterface();
    auto surface = static_cast<wl_surface *>(nativeInterface->nativeResourceForWindow("surface", window));
    if (surface) {
        auto tl = m_exporter->export_toplevel(surface);
        if (tl) {
            init(tl);
        } else {
            qDebug() << "could not export top level";
        }
    } else {
        qDebug() << "could not get surface";
    }
}

std::optional<QString> XdgExportedV2::handle() const
{
    return m_handle;
}

///////////////////////////////////////////////////////////

XdgExporterV2::XdgExporterV2()
    : QWaylandClientExtensionTemplate(ZXDG_EXPORTER_V2_DESTROY_SINCE_VERSION)
{
    initialize();

    if (!isInitialized()) {
        qWarning() << "Remember requesting the interface on your desktop file: X-KDE-Wayland-Interfaces=zkde_screencast_unstable_v1";
    }
    Q_ASSERT(isInitialized());
}

XdgExporterV2::~XdgExporterV2()
{
    destroy();
}

XdgExportedV2* XdgExporterV2::exportWindow(QWindow* window)
{
    if (!window) {
        qDebug() << "no window!";
        return nullptr;
    }

    auto exported = new XdgExportedV2(this);
    exported->setWindow(window);
    return exported;
}

XdgExportedV2* XdgExporterV2::exportWidget(QWidget* widget)
{
    if (widget->windowHandle()) {
        return exportWindow(widget->windowHandle());
    }
    auto nakedExporter = new XdgExportedV2(this);
    new WidgetWatcher(this, nakedExporter, widget);
    return nakedExporter;
}
