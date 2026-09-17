QT += core gui widgets
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = jetson_pantilt_ui
TEMPLATE = app

SOURCES += \
    src/qt_main.cpp \
    src/MainWindow.cpp \
    src/SettingsDialog.cpp \
    src/V4L2Camera.cpp \
    src/JoystickWidget.cpp \
    src/TrtYolo.cpp \
    src/TrtClassifier.cpp \
    src/Packet.cpp \
    src/SerialLink.cpp \
    src/TargetTracker.cpp \
    src/HudRenderer.cpp \
    src/TargetSelector.cpp \
    src/GeoEstimator.cpp \
    src/CommandArbitrator.cpp

HEADERS += \
    include/MainWindow.h \
    include/SettingsDialog.h \
    include/V4L2Camera.h \
    include/JoystickWidget.h \
    include/TrtYolo.h \
    include/TrtClassifier.h \
    include/PIDController.h \
    include/Packet.h \
    include/SerialLink.h \
    include/Types.h \
    include/TargetTracker.h \
    include/HudRenderer.h \
    include/TargetSelector.h \
    include/GeoEstimator.h \
    include/CommandArbitrator.h

INCLUDEPATH += \
    include \
    /usr/local/cuda/include \
    /usr/include/aarch64-linux-gnu

LIBS += -lpthread
