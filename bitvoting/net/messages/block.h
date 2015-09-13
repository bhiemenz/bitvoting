/*=============================================================================

Message, transmitting a specific single block over the network.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BLOCK_MESSAGE_H
#define BLOCK_MESSAGE_H

#include "message.h"
#include "../block.h"
#include "bitcoin/uint256.h"

#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>

// ==========================================================================

class BlockMessage : public Message
{
public:
    // Block that should be transmitted
    Block* block = NULL;

    // ----------------------------------------------------------------

    BlockMessage(Block* block = NULL):
        Message(),
        block(block)
    {
        this->header.type = MessageType::BLOCK;
    }

    // ----------------------------------------------------------------

    std::string string()
    {
        return "BlockMessage { " + this->string_header() + "block: " + this->block->toString() + " }";
    }

private:
    friend class boost::serialization::access;

    template<typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Message>(*this);
        a & this->block;
    }
};

BOOST_CLASS_IMPLEMENTATION(BlockMessage, boost::serialization::object_serializable)

#endif // BLOCK_MESSAGE_H
