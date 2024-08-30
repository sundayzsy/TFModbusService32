QT += serialbus serialport widgets
requires(qtConfig(combobox))

TARGET = modbusslave
TEMPLATE = app
CONFIG += c++11

DESTDIR = $$PWD/../

SOURCES += main.cpp\
        mainwindow.cpp \
        settingsdialog.cpp \
    protocoljson.cpp

HEADERS  += mainwindow.h settingsdialog.h \
    protocoljson.h \
    commondefine.h

FORMS    += mainwindow.ui settingsdialog.ui

RESOURCES += slave.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/serialbus/modbus/slave
INSTALLS += target
