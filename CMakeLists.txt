cmake_minimum_required(VERSION 3.16)

project(xdg-portal-test-kde)

set(QT_MIN_VERSION "6.6")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

################# set KDE specific information #################

find_package(ECM 5.102.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)
include(FindPkgConfig)

find_package(GLIB2 REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    DBus
    Widgets
)

find_package(KF${QT_MAJOR_VERSION} REQUIRED
    I18n
    KIO
    Notifications
    WindowSystem
)

find_package(KWayland REQUIRED)
find_package(Wayland REQUIRED Client)

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0 -DQT_NO_KEYWORDS)
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_STRICT_ITERATORS -DQT_NO_CAST_FROM_BYTEARRAY)

add_subdirectory(src)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
