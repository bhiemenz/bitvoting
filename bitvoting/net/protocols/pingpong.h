/*=============================================================================

Protocol for neighbor discovery and establishing new connections.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef PINGPONG_H
#define PINGPONG_H

#include "network.h"
#include "peers.h"
#include "messages/ping.h"
#include "messages/pong.h"
#include "protocols/duplicate.h"

#include <boost/bind.hpp>

// ==========================================================================

class PingPongProtocol : private DuplicateProtocol
{
public:
    PingPongProtocol(Network& network, boost::thread_group& threadGroup) : network(network)
    {
        // register appropriate callbacks
        network.SetCallback(MessageType::PING, boost::bind(&PingPongProtocol::ReceivedPing, this, _1, _2));
        network.SetCallback(MessageType::PONG, boost::bind(&PingPongProtocol::ReceivedPong, this, _1, _2));

        // create a new thread periodically sending pings
        threadGroup.create_thread(boost::bind(&PingPongProtocol::SendPings, this));
    }

    // ----------------------------------------------------------------

    // Start flooding Ping messages to all connected peers.
    void Ping()
    {
        PingMessage* ping = new PingMessage();
        this->seenMessage(ping);
        this->network.Flood(ping);
        delete ping;
    }

private:
    // Periodically send ping messages (called by thread)
    void SendPings()
    {
        while (true)
        {
            boost::this_thread::interruption_point();

            // only ping if max. connections have not been reached
            if (Peers::GetConnections().size() < Settings::GetMaxConnections())
                this->Ping();

            // stall thread for a specific time
            Helper::Sleep(Settings::GetPingInterval());
        }
    }

    // Callback for receiving new ping messages
    void ReceivedPing(boost::shared_ptr<Connection> connection, Message* message)
    {
        PingMessage* ping = (PingMessage*) message;

        if (this->checkDuplicate(ping))
            return;

        // If no address is given in the PingMessage body, add it after the first hop (direct connection).
        // This takes away the look-up effort from the sending instance to determine its own IP address.
        if (ping->address.empty())
        {
            ping->address = connection->GetSocket().remote_endpoint().address().to_string();
        }
        else
        {
            // check if already connected with the given peer
            boost::asio::ip::address_v4 address  =
                    boost::asio::ip::address_v4::from_string(ping->address);
            boost::asio::ip::tcp::endpoint endpoint(address, ping->port);

            if (!Peers::HasConnection(endpoint))
            {
                try
                {
                    // if not, establish a connection to the advertised address
                    this->network.Connect(endpoint);
                }
                catch(std::exception& e)
                {
                    Log::e("(Network) %s", e.what());
                }
            }
        }

        // pass on ping message
        this->network.Flood(ping, connection);
    }

    // Callback for receiving new pong messages (not used)
    void ReceivedPong(boost::shared_ptr<Connection>, Message*)
    {
        // currently not necessary
    }

    // ----------------------------------------------------------------

    // Reference to network
    Network& network;
};

#endif // PINGPONG_H
