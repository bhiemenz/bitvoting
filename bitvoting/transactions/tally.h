/*=============================================================================

This class represents a tally transaction. The election creator publishes this
kind of transaction to announce a vote count for his election. He determines
the last block, which should be part of the vote count and if he wants to end
the election finally.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_TX_TALLY_H
#define BITVOTING_TX_TALLY_H

#include "../transaction.h"
#include "../bitcoin/uint256.h"

#include <string>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/set.hpp>

class TxTally : public Transaction
{
public:

    // Reference to the election, which should be tallied
    uint256 election;

    // Reference to the block, which marks the last block to be included in the process
    uint256 lastBlock;

    // Flag, indicating if the election is closed (final tally)
    bool endElection = false;

    // ----------------------------------------------------------------

    TxTally():
        Transaction(TxType::TX_TALLY) {}

    // ----------------------------------------------------------------

    VerifyResult verify() /*const*/;

    std::string toString() const
    {
        return "TxTally {}";
    }

private:
    friend class boost::serialization::access;

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Transaction>(*this);
        a & this->election;
        a & this->lastBlock;
        a & this->endElection;
    }
};

BOOST_CLASS_EXPORT_KEY(TxTally)

#endif
