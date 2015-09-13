TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
CONFIG += c++11

DESTDIR = build
OBJECTS_DIR = build

LIBS += -lboost_system
LIBS += -lboost_filesystem
LIBS += -lboost_thread
LIBS += -lboost_program_options
LIBS += -lboost_iostreams
LIBS += -lboost_serialization
LIBS += -pthread

SOURCES += \
    helper.cpp \
    settings.cpp \
    connection.cpp \
    network.cpp \
    main_network.cpp

HEADERS += \
    helper.h \
    settings.h \
    connection.h \
    network.h \
    message.h \
    messages/ping.h \
    messages/text.h \
    messages/pong.h \
    protocols/pingpong.h \
    peers.h \
    messages/peerinfo.h \
    protocols/initialize.h \
    messages/heartbeat.h \
    protocols/heartbeat.h

OTHER_FILES += \
    config.cfg

