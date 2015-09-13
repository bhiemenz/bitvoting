/*=============================================================================

Sends a keep-alive heartbeat over the network.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include "message.h"

#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>

// ==========================================================================

class HeartbeatMessage : public Message
{
public:
    HeartbeatMessage()
    {
        this->header.type = MessageType::HEARTBEAT;
        this->header.ttl = 1; // heartbeats are only purposed for direct connections
    }

    // ----------------------------------------------------------------

    std::string string()
    {
        return "HeartbeatMessage {" + this->string_header() + "}";
    }

private:
    friend class boost::serialization::access;

    template<typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Message>(*this);
    }
};

BOOST_CLASS_IMPLEMENTATION(HeartbeatMessage, boost::serialization::object_serializable)

#endif // HEARTBEAT_H
