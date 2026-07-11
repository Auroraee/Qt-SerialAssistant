QT += testlib serialport widgets

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    ../asyncwritequeue.cpp \
    ../frameassembler.cpp \
    ../framelogmodel.cpp \
    ../mainwindow.cpp \
    ../modbusrtudecoder.cpp \
    ../serialportmanager.cpp \
    ../serialsessioncontroller.cpp \
    ../utils.cpp \
    test_utils.cpp

HEADERS += \
    ../asyncwritequeue.h \
    ../frameassembler.h \
    ../framelogmodel.h \
    ../frametypes.h \
    ../mainwindow.h \
    ../modbusrtudecoder.h \
    ../serialportmanager.h \
    ../serialsessioncontroller.h \
    ../utils.h

FORMS += \
    ../mainwindow.ui
