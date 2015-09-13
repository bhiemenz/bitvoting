/*=============================================================================

Represents a single message that is transmitted over the network.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef MESSAGE_H
#define MESSAGE_H

#include "settings.h"
#include "helper.h"

#include <vector>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/string.hpp>
#include <boost/uuid/uuid_serialize.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

// ==========================================================================

enum MessageType
{
    NONE        = 0x00,

    // Network specific messages
    TEXT        = 0x01,
    PING        = 0x02,
    PONG        = 0x03,
    PEER_INFO   = 0x04,
    HEARTBEAT   = 0x05,

    // Voting specific messages
    TRANSACTION = 0x10,
    BLOCK       = 0x20,
    BLOCK_REQ   = 0x21
};

// ==========================================================================

#define PRINT_HEADER true   // should the header be printed in the log
#define TTL_NOT_SET -32     // constant, indicating that a TTL has not been set yet
#define TTL_INFINITE -64    // constant, indicating that the TTL should not be decreased

typedef struct
{
    // TTL and hop info
    int ttl = TTL_NOT_SET;
    int hop = 0;

    // Size (in bytes) of the message
    long size = 0;

    // Type of the message
    MessageType type = MessageType::NONE;
} MessageHeader;

// ==========================================================================

class Message
{
public:
    Message()
    {
        // generate a random id for this message
        this->id = Helper::GenerateUUID();
    }

    virtual ~Message() {}

    // ----------------------------------------------------------------

    virtual std::string string() = 0;

    // Converts the header into a readable form
    std::string string_header()
    {
        if (!PRINT_HEADER)
            return "";

        std::string id = boost::lexical_cast<std::string>(this->id);
        return "ttl: (" + std::to_string(this->header.ttl) + "," + std::to_string(this->header.hop) + "); id: " + id + "; ";
    }

    // ----------------------------------------------------------------

    // Stores the header information for this message
    MessageHeader header;

    // Unique identifier for this message
    boost::uuids::uuid id;

private:
    friend class boost::serialization::access;

    template<typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        // This method is intended to be implemented in the subclasses
        a & this->id;
    }
};

BOOST_CLASS_IMPLEMENTATION(Message, boost::serialization::object_serializable)

#endif // MESSAGE_H
