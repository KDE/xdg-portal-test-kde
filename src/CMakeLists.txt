set(xdg_portal_test_kde_SRCS
    main.cpp
    xdgportaltest.cpp
    data/data.qrc
    dropsite/dropsitewindow.cpp
    dropsite/droparea.cpp
)

ki18n_wrap_ui(xdg_portal_test_kde_SRCS
    xdgportaltest.ui
)

include_directories(${GSTREAMER_INCLUDE_DIRS})

qt_add_dbus_interface(xdg_portal_test_kde_SRCS org.freedesktop.portal.GlobalShortcuts.xml globalshortcuts_portal_interface)
qt_add_dbus_interface(xdg_portal_test_kde_SRCS org.freedesktop.portal.Request.xml portalsrequest_interface)

add_executable(xdg-portal-test-kde ${xdg_portal_test_kde_SRCS})

target_link_libraries(xdg-portal-test-kde
    Qt::Core
    Qt::DBus
    Qt::Widgets
    KF5::I18n
    KF5::KIOFileWidgets
    KF5::Notifications
    KF5::WindowSystem
    KF5::WaylandClient
    GLIB2::GLIB2
    ${GSTREAMER_LIBRARIES}
)

install(TARGETS xdg-portal-test-kde DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
