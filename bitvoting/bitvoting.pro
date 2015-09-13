TEMPLATE = app
TARGET = Bitvoting

QT      += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Config
CONFIG += console
CONFIG -= app_bundle
CONFIG += c++11

include(./paillier/paillier.pri)
include(./net/network.pri)
include(./tests/test.pri)
include(./gui/gui.pri)

PKGCONFIG += openssl
INCLUDEPATH += $$OPENSSL_INCLUDE_PATH

# Libraries - Main
LIBS += -lgmp
LIBS += -lssl
LIBS += -lcrypto
LIBS += -lleveldb
LIBS += -lmemenv

# Libraries - Both
LIBS += -lboost_system
LIBS += -lboost_filesystem
LIBS += -lboost_thread
LIBS += -lboost_serialization

# Libraries - Network
LIBS += -lboost_program_options
LIBS += -lboost_iostreams
LIBS += -pthread

# packages required: libgmp3-dev, libssl-dev, libboost-all-dev, libleveldb-dev

SOURCES += \
    controller.cpp \
    main.cpp \
    miner.cpp \
    settings.cpp \
    helper.cpp \
    transaction.cpp \
    electionmanager.cpp \
    bitcoin/allocators.cpp \
    bitcoin/key.cpp \
    transactions/trustee_tally.cpp \
    transactions/tally.cpp \
    transactions/vote.cpp \
    transactions/election.cpp \
    database/electiondb.cpp \
    database/blockchaindb.cpp \
    database/leveldbwrapper.cpp

HEADERS += \
    block.h \
    controller.h \
    election.h \
    miner.h \
    transaction.h \
    store.h \
    settings.h \
    helper.h \
    export.h \
    utils/comparison.h \
    electionmanager.h \
    bitcoin/allocators.h \
    bitcoin/hash.h \
    bitcoin/key.h \
    bitcoin/uint256.h \
    database/electiondb.h \
    database/leveldbwrapper.h \
    database/paillierdb.h \
    database/signkeydb.h \
    database/blockchaindb.h \
    transactions/election.h \
    transactions/vote.h \
    transactions/trustee_tally.h \
    transactions/tally.h

OTHER_FILES += \
    config.cfg
