/*=============================================================================

Protocol for transmitting and exchanging blocks over the network.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BLOCKS_PROTOCOL_H
#define BLOCKS_PROTOCOL_H

#include "network.h"
#include "messages/block.h"
#include "messages/block_request.h"
#include "bitcoin/key.h"
#include "protocols/duplicate.h"

#include <vector>

#include <boost/bind.hpp>
#include <boost/function.hpp>

// ==========================================================================

class BlocksProtocol : private DuplicateProtocol
{
public:
    BlocksProtocol(Network& network) : network(network)
    {
        // register appropriate callbacks
        network.SetCallback(MessageType::BLOCK, boost::bind(&BlocksProtocol::ReceivedBlock, this, _1, _2));
        network.SetCallback(MessageType::BLOCK_REQ, boost::bind(&BlocksProtocol::ReceivedBlockRequest, this, _1, _2));
    }

    // ----------------------------------------------------------------

    // Register callback for block messages
    void SetCallback(boost::function<void (Block*)> callback)
    {
        this->callback = callback;
    }

    // Register callback for block request messages
    void SetRequestCallback(boost::function<std::vector<Block*> (BlockRequestMessage*)> callback)
    {
        this->callbackRequest = callback;
    }

    // Publish (flood) a new block in the network
    bool Publish(Block* block, SignKeyPair& keys)
    {
        if (keys.first.getRole() != Role::KEY_MINING)
            return false;

        // sign block
        block->sign(keys);

        // flood block message
        BlockMessage* message = new BlockMessage(block);
        message->header.ttl = TTL_INFINITE; // let everyone know!
        this->seenMessage(message);
        this->network.Flood(message);
        delete message;

        // publish to myself
        this->Distribute(block);
    }

private:
    // Callback when receiving new block messages
    void ReceivedBlock(boost::shared_ptr<Connection> connection, Message* message)
    {
        BlockMessage* tMessage = (BlockMessage*) message;

        if (this->checkDuplicate(tMessage))
            return;

        this->Distribute(tMessage->block);

        // pass on message
        this->network.Flood(tMessage, connection);
    }

    // Callback when receiving new block request messages
    void ReceivedBlockRequest(boost::shared_ptr<Connection> connection, Message* message)
    {
        BlockRequestMessage* tMessage = (BlockRequestMessage*) message;

        if (!this->callbackRequest)
            return;

        // check if client has the requested blocks
        std::vector<Block*> requestedBlocks = this->callbackRequest(tMessage);
        if (requestedBlocks.size() == 0)
            return;

        // respond with requested blocks
        for (int i = 1; i < requestedBlocks.size(); i++)
        {
            BlockMessage* answerMessage = new BlockMessage(requestedBlocks[i]);
            connection->Write(answerMessage);
            delete answerMessage;
        }
    }

    // Delegate Block to the registered callback
    void Distribute(Block* block)
    {
        if (!this->callback)
            return;

        this->callback(block);
    }

    // ----------------------------------------------------------------

    // Reference to network
    Network& network;

    // Registered callbacks
    boost::function<void (Block*)> callback;
    boost::function<std::vector<Block*> (BlockRequestMessage*)> callbackRequest;
};

#endif // BLOCKS_PROTOCOL_H
