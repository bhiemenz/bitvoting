/*=============================================================================

Encapsulating a transaction sent over the network

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef TRANSACTION_MESSAGE_H
#define TRANSACTION_MESSAGE_H

#include "message.h"
#include "../transaction.h"

#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>

// ==========================================================================

class TransactionMessage : public Message
{
public:
    // Storing the transaction
    Transaction* transaction = NULL;

    // ----------------------------------------------------------------

    TransactionMessage(Transaction* transaction = NULL):
        Message(),
        transaction(transaction)
    {
        this->header.type = MessageType::TRANSACTION;
        this->header.ttl = TTL_INFINITE; // purposed for infinite flooding
    }

    // ----------------------------------------------------------------

    std::string string()
    {
        std::string t = "NULL";
        if (this->transaction)
            t = std::to_string(this->transaction->getType());

        return "TransactionMessage { " + this->string_header() + "transaction: " + t + " }";
    }

private:
    friend class boost::serialization::access;

    template<typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Message>(*this);
        a & this->transaction;
    }
};

BOOST_CLASS_IMPLEMENTATION(TransactionMessage, boost::serialization::object_serializable)

#endif // TRANSACTION_MESSAGE_H
