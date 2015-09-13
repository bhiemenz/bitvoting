/*=============================================================================

Sending the initial peer information.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef PEERINFO_H
#define PEERINFO

#include "message.h"
#include "settings.h"

#include <string>

#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>

// ==========================================================================

class PeerInfoMessage : public Message
{
public:
    // Identifier of the sending peer
    std::string guid;

    // Client version of the sending peer
    int version = Settings::CLIENT_VERSION;

    // ----------------------------------------------------------------

    PeerInfoMessage()
    {
        this->header.type = MessageType::PEER_INFO;
    }

    // ----------------------------------------------------------------

    std::string string()
    {
        return "PeerInfoMessage { " + this->string_header() + "guid: " + this->guid + "; version: " + std::to_string(this->version) + " }";
    }

private:
    friend class boost::serialization::access;

    template<typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Message>(*this);
        a & this->guid;
        a & this->version;
    }
};

BOOST_CLASS_IMPLEMENTATION(PeerInfoMessage, boost::serialization::object_serializable)

#endif // PEERINFO_H
