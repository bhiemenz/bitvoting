/*=============================================================================

Protocol for sending periodic heartbeat messages (keep-alive)

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef HEARTBEAT_PROTOCOL_H
#define HEARTBEAT_PROTOCOL_H

#include "network.h"
#include "message.h"
#include "messages/heartbeat.h"
#include "peers.h"
#include "helper.h"
#include "connection.h"

#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/foreach.hpp>

// ==========================================================================

class HeartbeatProtocol
{
public:
    HeartbeatProtocol(Network& network, boost::thread_group& threadGroup) : network(network)
    {
        // create a new thread periodically sending heartbeats
        threadGroup.create_thread(boost::bind(&HeartbeatProtocol::SendHeartbeats, this));
    }

private:
    // Sends periodic heartbeat messages (called by thread)
    void SendHeartbeats()
    {
        while (true)
        {
            boost::this_thread::interruption_point();

            // send heartbeat to every initialized connection
            HeartbeatMessage* heartbeat = new HeartbeatMessage();
            this->network.Flood(heartbeat);
            delete heartbeat;

            // stall thread for a specific time
            Helper::Sleep(Settings::GetHeartbeatInterval());
        }
    }

    // ----------------------------------------------------------------

    // Reference to the network
    Network& network;
};

#endif // HEARTBEAT_PROTOCOL_H
