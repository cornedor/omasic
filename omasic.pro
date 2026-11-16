QT += core gui network qml quick quickcontrols2 dbus webenginecore webenginequick webchannel

CONFIG += c++17 release
TARGET = omasic
TEMPLATE = app

HEADERS += \
    src/applemusic.h \
    src/artworkpalette.h \
    src/audiolevels.h \
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
    src/artworkpalette.cpp \
    src/audiolevels.cpp \
    src/developertoken.cpp \
    src/mpris.cpp \
    src/outputs.cpp \
    src/page.cpp \
    src/player.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc

# Qt Quick loads shaders in its own .qsb format; qsb (qt6-shadertools) bakes
# the visualizer's into the build directory, and from there into the binary.
SHADERS = src/field.frag src/glyphs.frag
qsb.input = SHADERS
qsb.output = ${QMAKE_FILE_BASE}.frag.qsb
qsb.commands = $$[QT_HOST_BINS]/qsb --qt6 -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
qsb.CONFIG = no_link target_predeps
QMAKE_EXTRA_COMPILERS += qsb

shaders.files = $$OUT_PWD/field.frag.qsb $$OUT_PWD/glyphs.frag.qsb
shaders.base = $$OUT_PWD
shaders.prefix = /
RESOURCES += shaders
