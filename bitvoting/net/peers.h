/*=============================================================================

Stores and manages all connected peers.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef PEERS_H
#define PEERS_H

#include "connection.h"

#include <vector>

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/foreach.hpp>

// ==========================================================================

class Peers
{
private:
    // ----------------------------------------------------------------
    // Singleton

    Peers() {};
    Peers(Peers const&)             = delete;
    void operator=(Peers const&)    = delete;

    static Peers& GetInstance()
    {
        static Peers instance;
        return instance;
    }

    // ----------------------------------------------------------------

    // Stores all active connections
    std::vector<boost::shared_ptr<Connection>> connections;

    boost::mutex connectionsMutex;

public:
    // Get all connections
    static std::vector<boost::shared_ptr<Connection>>& GetConnections()
    {
        return Peers::GetInstance().connections;
    }

    // Obtain mutex over connections
    static boost::mutex& GetMutex()
    {
        return Peers::GetInstance().connectionsMutex;
    }

    // Remove a specific connection
    static void RemoveConnection(boost::shared_ptr<Connection> connection)
    {
        boost::mutex::scoped_lock lock(Peers::GetMutex());
        std::vector<boost::shared_ptr<Connection>>& connections = Peers::GetConnections();
        std::vector<boost::shared_ptr<Connection>>::iterator it;
        for (it = connections.begin(); it < connections.end();)
        {
            if (*it == connection)
                it = connections.erase(it);
            else
                 ++it;
        }
    }

    // Check if a specific endpoint is already connected
    static bool HasConnection(boost::asio::ip::tcp::endpoint& endpoint)
    {
        BOOST_FOREACH(boost::shared_ptr<Connection> current, Peers::GetConnections())
        {
            if (current->GetSocket().remote_endpoint() == endpoint)
                return true;
        }

        return false;
    }

    // Check if a specific peer is already connected
    static bool HasConnection(boost::uuids::uuid uuid)
    {
        BOOST_FOREACH(boost::shared_ptr<Connection> current, Peers::GetConnections())
        {
            if (!current->IsInitialized())
                continue;

            if (current->guid == uuid)
                return true;
        }

        return false;
    }
};

#endif // PEERS_H
