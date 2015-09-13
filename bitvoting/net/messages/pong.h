/*=============================================================================

Pong message for answering ping messages (currently not used)

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef PONG_H
#define PONG_H

#include "message.h"

#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>

// ==========================================================================

class PongMessage : public Message
{
public:
    PongMessage()
    {
        this->header.type = MessageType::PONG;
    }

    // ----------------------------------------------------------------

    std::string string()
    {
        return "PongMessage {" + this->string_header() + "}";
    }

private:
    friend class boost::serialization::access;

    template<typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Message>(*this);
    }
};

BOOST_CLASS_IMPLEMENTATION(PongMessage, boost::serialization::object_serializable)

#endif // PONG_H
