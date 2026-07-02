QT += testlib
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    ../utils.cpp \
    test_utils.cpp

HEADERS += \
    ../utils.h
