set(xdg_portal_test_kde_SRCS
    main.cpp
    xdgportaltest.cpp
)

ki18n_wrap_ui(xdg_portal_test_kde_SRCS
    xdgportaltest.ui
)

include_directories(${GSTREAMER_INCLUDE_DIRS})

add_executable(xdg-portal-test-kde ${xdg_portal_test_kde_SRCS})

target_link_libraries(xdg-portal-test-kde
    Qt5::Core
    Qt5::DBus
    Qt5::Widgets
    KF5::I18n
    KF5::KIOFileWidgets
    KF5::Notifications
    KF5::WindowSystem
    GLIB2::GLIB2
    ${GSTREAMER_LIBRARIES}
)

install(TARGETS xdg-portal-test-kde DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
