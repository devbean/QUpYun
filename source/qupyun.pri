QT          *= network

INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

DEFINES += QUPYUN_LIBRARY

HEADERS += \
    $$PWD/qupyun.h \
    $$PWD/qupyun_global.h

SOURCES += \
    $$PWD/qupyun.cpp
