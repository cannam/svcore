TEMPLATE = lib

SV_UNIT_PACKAGES =
load(../sv.prf)

CONFIG += sv staticlib qt thread warn_on stl rtti exceptions

TARGET = svsystem

DEPENDPATH += .
INCLUDEPATH += .

# Input
HEADERS += Init.h System.h
SOURCES += Init.cpp System.cpp