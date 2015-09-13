/*=============================================================================

Ping message for neighbor discovery and establishing new connections.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef PING_H
#define PING_H

#include "message.h"
#include "settings.h"
#include "helper.h"

#include <string>

#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>

// ==========================================================================

class PingMessage : public Message
{
public:
    // IP address of the sending peer
    std::string address;

    // Port the sending peer is listening on
    short port = Settings::GetPort();

    // ----------------------------------------------------------------

    PingMessage()
    {
        this->header.type = MessageType::PING;
    }

    // ----------------------------------------------------------------

    std::string string()
    {
        return "PingMessage { " + this->string_header() + "address: " + this->address + "; port: " + std::to_string(this->port) + " }";
    }

private:
    friend class boost::serialization::access;

    template<typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Message>(*this);
        a & this->address;
        a & this->port;
    }
};

BOOST_CLASS_IMPLEMENTATION(PingMessage, boost::serialization::object_serializable)

#endif // PING_H
