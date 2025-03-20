QT = core

CONFIG += c++17 cmdline
CONFIG += link_pkgconfig

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        ../vban_common/pipewire_backend.cpp \
        ../vban_common/udp.cpp \
        ../vban_common/vban_client_list.cpp \
        ../vban_common/vban_functions.cpp \
        main.cpp

HEADERS += \
    ../vban_common/pipewire_backend.h \
    ../vban_common/popen2.h \
    ../vban_common/ringbuffer.h \
    ../vban_common/udp.h \
    ../vban_common/vban.h \
    ../vban_common/vban_client_list.h \
    ../vban_common/vban_functions.h

LIBS += \
        -lpthread


PKGCONFIG += libpipewire-0.3

INCLUDEPATH += \
        ../vban_common

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
