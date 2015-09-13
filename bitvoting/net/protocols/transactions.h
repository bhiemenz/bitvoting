/*=============================================================================

Protocol for handling the exchange and proper delegation of transaction
messages.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef PROTOCOL_TRANSACTIONS_H
#define PROTOCOL_TRANSACTIONS_H

#include "network.h"
#include "messages/transaction.h"
#include "bitcoin/key.h"
#include "transaction.h"
#include "protocols/duplicate.h"

#include <boost/bind.hpp>
#include <boost/function.hpp>

// ==========================================================================

class TransactionsProtocol : private DuplicateProtocol
{
public:
    TransactionsProtocol(Network& network) : network(network)
    {
        // register appropriate callbacks
        network.SetCallback(MessageType::TRANSACTION, boost::bind(&TransactionsProtocol::ReceivedTransaction, this, _1, _2));
    }

    // ----------------------------------------------------------------

    // Register a callback for a given Transaction type
    void SetCallback(TxType type, boost::function<void (Transaction*)> callback)
    {
        this->callbacks[type] = callback;
    }

    // Publish a given transaction into the network
    bool Publish(Transaction* transaction, SignKeyPair keys)
    {
        Log::i("(Network) Signing & Publishing Transaction (Type: %d)", transaction->getType());

        // check if signing key matches transaction
        TxType type = transaction->getType();
        Role role = keys.first.getRole();

        if(type == TxType::TX_VOTE && role != Role::KEY_VOTE)
            return false;
        else if(type == TxType::TX_ELECTION && role != Role::KEY_ELECTION)
            return false;
        else if(type == TxType::TX_TALLY && role != Role::KEY_ELECTION)
            return false;
        else if(type == TxType::TX_TRUSTEE_TALLY && role != Role::KEY_TRUSTEE)
            return false;

        // sign transaction
        transaction->sign(keys);

        // send transaction
        TransactionMessage* message = new TransactionMessage(transaction);
        this->seenMessage(message);
        this->network.Flood(message);
        delete message;

        // publish to myself
        this->Distribute(transaction);

        return true;
    }

private:
    // Callback for receiving a new transaction message
    void ReceivedTransaction(boost::shared_ptr<Connection> connection, Message* message)
    {
        TransactionMessage* tMessage = (TransactionMessage*) message;

        if (this->checkDuplicate(tMessage))
            return;

        // delegate to callbacks
        this->Distribute(tMessage->transaction);

        // pass on message in network (flood)
        this->network.Flood(tMessage, connection);
    }

    // Delegates the transaction to the appropriate callback
    void Distribute(Transaction* transaction)
    {
        if (!this->callbacks[transaction->getType()])
            return;

        this->callbacks[transaction->getType()](transaction);
    }

    // ----------------------------------------------------------------

    // Reference to network
    Network& network;

    // Stores the callbacks for transactions
    std::map<TxType, boost::function<void (Transaction*)>> callbacks;
};

#endif // PROTOCOL_TRANSACTIONS_H
