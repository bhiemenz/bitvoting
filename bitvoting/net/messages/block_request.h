/*=============================================================================

Sending a request for (a) specific block(s).

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BLOCK_REQUEST_H
#define BLOCK_REQUEST_H

#include "message.h"
#include "bitcoin/uint256.h"

#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>

// ==========================================================================

class BlockRequestMessage : public Message
{
public:
    // Indicates which block is requested (following that)
    uint256 block = 0;

    // Indicates if only one block, or the complete chain is requested
    bool following = false;

    // ----------------------------------------------------------------

    BlockRequestMessage(bool following = false):
        Message(),
        following(following)
    {
        this->header.type = MessageType::BLOCK_REQ;
    }

    // ----------------------------------------------------------------

    std::string string()
    {
        return "BlockRequestMessage { " + this->string_header() + "block: " + this->block.GetHex() + "}";
    }

private:
    friend class boost::serialization::access;

    template<typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Message>(*this);
        a & this->block;
        a & this->following;
    }
};

BOOST_CLASS_IMPLEMENTATION(BlockRequestMessage, boost::serialization::object_serializable)


#endif // BLOCK_REQUEST_H
