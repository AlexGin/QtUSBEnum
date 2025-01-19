TEMPLATE = app
TARGET = QtUSBEnum

QT += core gui widgets network

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
INCLUDEPATH += \
    .
    ./USBEnum

SOURCES += \
    main.cpp \
    QtUSBEnum.cpp \
    FileUtils.cpp \
    HttpDaemon.cpp \
    LogFile.cpp \
    UdpNotifyItem.cpp \
    UdpNotifyServer.cpp \
    USBEnumJson.cpp \
    USBEnum/DevNode.cpp \
    USBEnum/DriveVolume.cpp \
    USBEnum/Enumerator.cpp \
    USBEnum/USBDevice.cpp \
    USBEnum/USBHub.cpp \
    USBEnum/UsbVolume.cpp \
    USBEnum/UVCViewer.cpp

HEADERS += \
    QtUSBEnum.h \
    FileUtils.h \
    HttpDaemon.h \
    LogFile.h \
    UdpNotifyItem.h \
    UdpNotifyServer.h \
    USBEnumEventFilter.h \
    USBEnumJson.h \
    USBEnum/DataTypes.h \
    USBEnum/DevNode.h \
    USBEnum/DriveVolume.h \
    USBEnum/Enumerator.h \
    USBEnum/IUSBDevice.h \
    USBEnum/USBDesc.h \
    USBEnum/USBDevice.h \
    USBEnum/USBHub.h \
    USBEnum/UsbVolume.h \
    USBEnum/UVCViewer.h \
    USBEnum/vndrlist.h

FORMS += \
    QtUSBEnum.ui

RESOURCES += \
    QtUSBEnum.qrc

LIBS += \
  -lshell32 -lkernel32 -luser32 -lsetupapi -lstrsafe -lshlwapi -lWinmm

