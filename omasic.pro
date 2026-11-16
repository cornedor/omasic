QT += core gui network qml quick quickcontrols2 dbus webenginecore webenginequick webchannel

CONFIG += c++17 release
TARGET = omasic
TEMPLATE = app

HEADERS += \
    src/applemusic.h \
    src/developertoken.h \
    src/mpris.h \
    src/outputs.h \
    src/page.h \
    src/player.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/applemusic.cpp \
    src/developertoken.cpp \
    src/mpris.cpp \
    src/outputs.cpp \
    src/page.cpp \
    src/player.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
