QT += core gui network qml quick quickcontrols2 testlib dbus
CONFIG += c++17 testcase
TARGET = omasic_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/applemusic.h \
    ../src/developertoken.h \
    ../src/mpris.h \
    ../src/outputs.h \
    ../src/page.h \
    ../src/player.h \
    ../src/theme.h \
    ../src/translator.h

SOURCES += \
    omasic_tests.cpp \
    ../src/applemusic.cpp \
    ../src/developertoken.cpp \
    ../src/mpris.cpp \
    ../src/outputs.cpp \
    ../src/page.cpp \
    ../src/player.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp
