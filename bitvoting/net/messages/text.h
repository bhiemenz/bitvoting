/*=============================================================================

Sending a simple text message (for testing purposes)

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef TEXT_H
#define TEXT_H

#include "message.h"

#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>

// ==========================================================================

class TextMessage : public Message
{
public:
    // Store text
    std::string text;

    // ----------------------------------------------------------------

    TextMessage(std::string text = "") : text(text)
    {
        this->header.type = MessageType::TEXT;
        this->header.ttl = 1; // only meant for direct neighbors
    }

    // ----------------------------------------------------------------

    std::string string()
    {
        return "TextMessage { " + this->string_header() + "text: " + this->text + " }";
    }

private:
    friend class boost::serialization::access;

    template<typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Message>(*this);
        a & this->text;
    }
};

BOOST_CLASS_IMPLEMENTATION(TextMessage, boost::serialization::object_serializable)

#endif // TEXT_H
