# flatpak-portal-test-kde

A simple test application for [Flatpak](http://www.flatpak.org) portals.

The portal interfaces are defined in [xdg-desktop-portal](https://github.com/flatpak/xdg-desktop-portal).

A Qt/KDE implementation can be found in [xdg-desktop-portal-kde](https://github.com/grulja/xdg-desktop-portal-kde).

To use this test, use the build script in flatpak-build/ to produce a flatpak of portal-test, then install it with

    flatpak remote-add --user portal-test-kde file:///path/to/repo
    flatpak install --user portal-test-kde org.kde.PortalTest

and run it with

    flatpak run --env=QT_LOGGING_RULES=qt.qpa.qflatpak*.debug=true org.kde.PortalTest -platform flatpak

The test expects the xdg-desktop-portal service (and a backend, such as xdg-desktop-portal-kde) to be available on the session bus.
