INCLUDEPATH  += $$PWD/

SOURCES += \
    $$PWD/connection.cpp \
    $$PWD/network.cpp

HEADERS += \
    $$PWD/connection.h \
    $$PWD/network.h \
    $$PWD/message.h \
    $$PWD/messages/ping.h \
    $$PWD/messages/text.h \
    $$PWD/messages/pong.h \
    $$PWD/protocols/pingpong.h \
    $$PWD/peers.h \
    $$PWD/messages/peerinfo.h \
    $$PWD/protocols/initialize.h \
    $$PWD/messages/heartbeat.h \
    $$PWD/protocols/heartbeat.h \
    $$PWD/messages/transaction.h \
    $$PWD/protocols/transactions.h \
    net/messages/block.h \
    net/protocols/duplicate.h \
    net/protocols/blocks.h \
    net/messages/block_request.h

