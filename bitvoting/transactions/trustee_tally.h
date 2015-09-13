/*=============================================================================

This class represents a trustee tally transaction. A election`s trustee
publishes this kind of transaction, if he received a tally transaction from
the election creator. It contains his part of the votes decryption and a ZKP.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_TX_TRUSTEE_TALLY_H
#define BITVOTING_TX_TRUSTEE_TALLY_H

#include "../transaction.h"
#include "../election.h"

#include <set>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/set.hpp>

class TxTrusteeTally : public Transaction
{
public:

    // Reference to the tally command by the initiator
    uint256 tally;

    // Trustee`s part of the partially decryption
    std::set<TalliedBallots> partialDecryption;

    // ----------------------------------------------------------------

    TxTrusteeTally():
        Transaction(TxType::TX_TRUSTEE_TALLY) {}

    // ----------------------------------------------------------------

    VerifyResult verify() /*const*/;

    std::string toString() const
    {
        return "TxTrusteeTally {}";
    }

private:
    friend class boost::serialization::access;

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Transaction>(*this);
        a & this->tally;
        a & this->partialDecryption;
    }
};

BOOST_CLASS_EXPORT_KEY(TxTrusteeTally)

#endif
