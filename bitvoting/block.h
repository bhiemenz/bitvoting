/*=============================================================================

This class represents a block with all its attributes.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_BLOCK_H
#define BITVOTING_BLOCK_H

#include "settings.h"
#include "transaction.h"
#include "bitcoin/uint256.h"
#include "utils/comparison.h"

#include <set>
#include <string>

#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/set.hpp>

// ----------------------------------------------------------------
typedef struct BlockHeader_
{

    // Protocol version
    int version = Settings::CLIENT_VERSION;

    // Hash of previous block
    uint256 hashPrevBlock = 0;

    // Nonce that was used for proof of work
    unsigned int nonce = 0;

    // Time, this block was solved
    long long time = 0;

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & version;
        a & hashPrevBlock;
        a & nonce;
        a & time;
    }
} BlockHeader;

// ----------------------------------------------------------------
class Block : public Signable
{
public:

    // Header for this block
    BlockHeader header;

    // List of transactions, which are part of this block
    std::set<Transaction*, pt_cmp> transactions;

    std::string toString() const
    {
        return "Block {}";
    }

private:

    friend class boost::serialization::access;

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Signable>(*this);
        a & this->header;
        a & this->transactions;
    }
};

BOOST_CLASS_EXPORT_KEY(Block)

#endif
