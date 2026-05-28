TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CFLAGS_RELEASE += -O3 -march=core2 -static

win32: QMAKE_TARGET_PRODUCT = AVRTapeEEPROM
win32: QMAKE_TARGET_DESCRIPTION = EEPROM image creator for AVRTapeControl

SOURCES += \
        main.c
