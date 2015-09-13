/*=============================================================================

Handles all network connections, establishing new ones and providing callbacks
for network events.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef NETWORK_H
#define NETWORK_H

#include "connection.h"
#include "message.h"

#include <map>

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

// ==========================================================================

class Network
{
public:
    Network(boost::thread_group&);

    // ----------------------------------------------------------------

    // Connect to a given endpoint
    void Connect(const boost::asio::ip::tcp::endpoint&);

    // Flood a given message to all directly connected peers (w/
    // possible exclusion of one)
    void Flood(Message*, boost::shared_ptr<Connection> = NULL);

    // Tears down all connections, shuts down sockets
    void Shutdown();

    // Registers a callback for a given MessageType
    void SetCallback(MessageType,
                     boost::function<void (boost::shared_ptr<Connection>, Message*)>);

private:
    // Start network
    void Start();

    // Start listening on a specific port for new connections
    void Listen();

    // Callback for receiving a new connection
    void Accept(boost::shared_ptr<Connection>,
                const boost::system::error_code&);

    // Callback for successfully establishing a new connection
    void Connected(boost::shared_ptr<Connection>,
                   boost::asio::ip::tcp::endpoint&,
                   const boost::system::error_code&);

    // Callback for receiving a new Message
    void MessageReceived(boost::shared_ptr<Connection>, Message*);

    // ----------------------------------------------------------------

    boost::thread_group& threadGroup;
    boost::asio::io_service ioService;
    boost::asio::ip::tcp::acceptor acceptor;

    // Stores all registered callbacks
    std::map<MessageType, boost::function<void (boost::shared_ptr<Connection>, Message*)>> callbacks;
};

#endif // NETWORK_H
