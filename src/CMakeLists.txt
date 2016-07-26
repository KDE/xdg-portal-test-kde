set(flatpak_portal_test_kde_SRCS
    main.cpp
    portaltest.cpp
)

ki18n_wrap_ui(flatpak_portal_test_kde_SRCS
    portaltest.ui
)

add_executable(portal-test-kde ${flatpak_portal_test_kde_SRCS})

target_link_libraries(portal-test-kde
    Qt5::Core
    Qt5::DBus
    Qt5::Widgets
    KF5::I18n
    KF5::KIOFileWidgets
)

install(TARGETS portal-test-kde DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})