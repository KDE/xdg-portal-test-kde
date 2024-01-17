# xdg-portal-test-kde

A simple test application for [Flatpak](http://www.flatpak.org) portals and Qt flatpak platform plugin.

The portal interfaces are defined in [xdg-desktop-portal](https://github.com/flatpak/xdg-desktop-portal).

A Qt/KDE implementation can be found in [xdg-desktop-portal-kde](https://invent.kde.org/plasma/xdg-desktop-portal-kde/).

To produce a flatpak of xdg-portal-test-kde run the following:

```
    cd flatpak-build
    make build
```

run with:
```
    make run
```
The test expects the xdg-desktop-portal service (and a backend, such as xdg-desktop-portal-kde) to be available on the session bus.
