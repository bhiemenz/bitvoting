/*=============================================================================

This class represents a vote transaction. A voter publishes this kind of
transaction if he has chosen an answer for all questions of the election,
he wants (and is allowed) to participate in. His anwsers are encrypted.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_TX_VOTE_H
#define BITVOTING_TX_VOTE_H

#include "../transaction.h"
#include "../election.h"

#include <set>
#include <string>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/set.hpp>

class TxVote : public Transaction
{
public:

    // Hash to election transaction, this vote is about
    uint256 election;

    // Encrypted answers to election`s questions
    std::set<EncryptedBallot> ballots;

    // ----------------------------------------------------------------

    TxVote():
        Transaction(TxType::TX_VOTE) {}

    // ----------------------------------------------------------------

    VerifyResult verify() /*const*/;

    std::string toString() const
    {
        return "TxVote {}";
    }

private:
    friend class boost::serialization::access;

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Transaction>(*this);
        a & this->election;
        a & this->ballots;
    }
};

BOOST_CLASS_EXPORT_KEY(TxVote)

#endif
