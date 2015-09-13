/*=============================================================================

This class represents an election transaction. If a network user wants to
hold an election, he creates this kind of transaction and publishes it.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_TX_ELECTION_H
#define BITVOTING_TX_ELECTION_H

#include "../transaction.h"
#include "../election.h"

#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/serialization.hpp>

class TxElection : public Transaction
{
public:

    // Election, this transaction is about
    Election* election = NULL;

    // ----------------------------------------------------------------

    TxElection(Election* election = NULL):
        Transaction(TxType::TX_ELECTION),
        election(election) {}

    // ----------------------------------------------------------------

    VerifyResult verify() /*const*/;

    std::string toString() const
    {
        return "TxElection {}";
    }

private:
    friend class boost::serialization::access;

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Transaction>(*this);
        a & this->election;
    }
};

BOOST_CLASS_EXPORT_KEY(TxElection)

#endif
