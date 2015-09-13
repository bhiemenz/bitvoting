#include "network.h"
#include "peers.h"
#include "settings.h"
#include "messages/ping.h"
#include "protocols/initialize.h"

#include <iostream>

#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>

// ================================================================

Network::Network(boost::thread_group& threadGroup) : threadGroup(threadGroup), ioService(), acceptor(ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), Settings::GetPort()))
{
    // start a new thread for handling the network
    this->threadGroup.create_thread(boost::bind(&Network::Start, this));
}

// ----------------------------------------------------------------

void
Network::Start()
{
    // start services
    this->Listen();
    this->acceptor.get_io_service().run();
}

// ----------------------------------------------------------------

void
Network::Shutdown()
{
    // close all open connections
    while( Peers::GetConnections().size() )
    {
        Peers::GetConnections().front()->Close();
    }

    this->acceptor.get_io_service().stop();
}

// ----------------------------------------------------------------

void
Network::Listen()
{
    // initialize connection and set callback
    boost::shared_ptr<Connection> connection(new Connection(acceptor.get_io_service()));
    connection->SetCallback(boost::bind(&Network::MessageReceived, this, connection, _1));

    // async. call to wait for clients to connect
    acceptor.async_accept(connection->GetSocket(),
                          boost::bind(&Network::Accept, this, connection,
                                      boost::asio::placeholders::error));
}

// ----------------------------------------------------------------

void
Network::Accept(boost::shared_ptr<Connection> connection,
                const boost::system::error_code& error)
{
    Log::i("(Network) New connection from %s",
           Helper::GetAddressString(connection->GetSocket().remote_endpoint()).c_str());

    if (Peers::GetConnections().size() >= Settings::GetMaxConnections())
    {
        Log::e("(Network) Already reached maximum of possible connections, dropping!");
        connection->Close();
        Listen();
        return;
    }

    if (error)
    {
        Log::e("(Network) %s", error.message().c_str());
    }
    else
    {
        // add connection to connected peers
        Peers::GetMutex().lock();
        Peers::GetConnections().push_back(connection);
        Peers::GetMutex().unlock();

        connection->Read();

        // send my peer info
        InitializeProtocol::Initialize(connection);
    }

    // listen for a new connection
    Listen();
}

// ----------------------------------------------------------------

void
Network::Connect(const boost::asio::ip::tcp::endpoint& endpoint)
{
    Log::i("(Network) Connecting to %s", Helper::GetAddressString(endpoint).c_str());

    if (Peers::GetConnections().size() >= Settings::GetMaxConnections())
        throw std::out_of_range("Already reached maximum of possible connections!");

    // initialize connection and set callback
    boost::shared_ptr<Connection> connection(new Connection(this->acceptor.get_io_service(),
                                                            ConnectionType::OUTBOUND));
    connection->SetCallback(boost::bind(&Network::MessageReceived, this, connection, _1));

    // start an asynchronous connect operation
    connection->GetSocket().async_connect(endpoint,
                                          boost::bind(&Network::Connected, this,
                                                      connection, endpoint,
                                                      boost::asio::placeholders::error));
}

// ----------------------------------------------------------------

void
Network::Connected(boost::shared_ptr<Connection> connection,
                   boost::asio::ip::tcp::endpoint&,
                   const boost::system::error_code& error)
{
    if (error)
    {
        Log::e("(Network) Could not connect to remote server: %s", error.message().c_str());
        return;
    }

    Log::i("(Network) Successfully connected!");

    // add connection to connected peers
    Peers::GetMutex().lock();
    Peers::GetConnections().push_back(connection);
    Peers::GetMutex().unlock();

    connection->Read();

    // send my peer info
    InitializeProtocol::Initialize(connection);
}

// ----------------------------------------------------------------

void
Network::Flood(Message* message, boost::shared_ptr<Connection> exclude)
{
    // set TTL header if not been set before
    if (message->header.ttl == TTL_NOT_SET)
        message->header.ttl = Settings::GetFloodingTTL();

    // send given message to all connected peers (which are initialized)
    BOOST_FOREACH(boost::shared_ptr<Connection> connection, Peers::GetConnections())
    {
        // check if connection is excluded
        if (exclude && connection == exclude)
            continue;

        if (!connection->IsInitialized())
            continue;

        connection->Write(message);
    }
}

// ----------------------------------------------------------------

void
Network::SetCallback(MessageType type, boost::function<void (boost::shared_ptr<Connection>, Message*)> callback)
{
    // register callback
    this->callbacks[type] = callback;
}

// ----------------------------------------------------------------

void
Network::MessageReceived(boost::shared_ptr<Connection> connection, Message* message)
{
    MessageType type = message->header.type;

    if (!this->callbacks[type])
        return;

    // delegate callback to responsible handler
    this->callbacks[type](connection, message);
}
