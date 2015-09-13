/*=============================================================================

This protocol handles the initialization phase of the connection of two peers.
It includes exchanging peer information (like GUID, version etc.)

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef INITIALIZE_H
#define INITIALIZE_H

#include "network.h"
#include "message.h"
#include "messages/peerinfo.h"
#include "messages/block_request.h"
#include "helper.h"
#include "peers.h"
#include "../database/blockchaindb.h"

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

// ==========================================================================

class InitializeProtocol
{
public:
    InitializeProtocol(Network& network) : network(network)
    {
        // register appropriate callbacks
        network.SetCallback(MessageType::PEER_INFO, boost::bind(&InitializeProtocol::ReceivedPeerInfo, this, _1, _2));
    }

    // ----------------------------------------------------------------

    // Sends peer info of this client to the given connection
    static void Initialize(boost::shared_ptr<Connection> connection)
    {
        // send my peer info to the given connection
        PeerInfoMessage* info = new PeerInfoMessage();
        info->guid = boost::lexical_cast<std::string>(Helper::GetGUID());
        connection->Write(info);
        delete info;
    }

private:
    // Callback for receiving a PeerInfo Message
    void ReceivedPeerInfo(boost::shared_ptr<Connection> connection, Message* message)
    {
        PeerInfoMessage* info = (PeerInfoMessage*) message;

        // obtain & parse GUID of sender
        boost::uuids::string_generator generator;
        boost::uuids::uuid uuid = generator(info->guid);

        // check if there is already a connection with this GUID (or my own)
        if (uuid == Helper::GetGUID() || Peers::HasConnection(uuid))
        {
            connection->Close();
            return;
        }

        // apply information to the connection (is now initialized)
        connection->guid = uuid;
        connection->version = info->version;

        // check if newly connected peer has better chain knowledge
        BlockRequestMessage* blockRequest = new BlockRequestMessage(true);
        blockRequest->block = BlockChainDB::getLatestBlockHash();
        connection->Write(blockRequest);
        delete blockRequest;
    }

    // ----------------------------------------------------------------

    // Reference to the network
    Network& network;
};

#endif // INITIALIZE_H
