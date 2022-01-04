# flatpak-portal-test-kde

A simple test application for [Flatpak](http://www.flatpak.org) portals and Qt flatpak platform plugin.

The portal interfaces are defined in [xdg-desktop-portal](https://github.com/flatpak/xdg-desktop-portal).

A Qt/KDE implementation can be found in [xdg-desktop-portal-kde](https://cgit.kde.org/xdg-desktop-portal-kde.git/).

To produce a flatpak of portal-test run the following:

```
    cd flatpak-build
    ./build.sh
    flatpak remote-add --user --no-gpg-verify portal-test-kde file:///$PWD/repo
    flatpak install --user portal-test-kde org.kde.portal-test-kde
```

run with:
```
    flatpak run org.kde.portal-test-kde
```
The test expects the xdg-desktop-portal service (and a backend, such as xdg-desktop-portal-kde) to be available on the session bus.
