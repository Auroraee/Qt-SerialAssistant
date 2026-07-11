QT += core gui widgets serialport

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    asyncwritequeue.cpp \
    frameassembler.cpp \
    framelogmodel.cpp \
    main.cpp \
    mainwindow.cpp \
    modbusrtudecoder.cpp \
    serialportmanager.cpp \
    serialsessioncontroller.cpp \
    utils.cpp

HEADERS += \
    asyncwritequeue.h \
    frameassembler.h \
    framelogmodel.h \
    frametypes.h \
    mainwindow.h \
    modbusrtudecoder.h \
    serialportmanager.h \
    serialsessioncontroller.h \
    utils.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    SerialAssistant_en_US.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
