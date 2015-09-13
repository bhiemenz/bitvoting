/*=============================================================================

Base class for transactions. Each transaction is signed (more precise its hash)
before it is published and contains a verification key, that has be to used
to verify this signature.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_TRANSACTION_H
#define BITVOTING_TRANSACTION_H

#include "bitcoin/key.h"
#include "bitcoin/uint256.h"

#include <string>
#include <vector>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/vector.hpp>

// ----------------------------------------------------------------
enum TxType
{
    TX_NONE,
    TX_VOTE,            // commit vote (pushed by voter)
    TX_ELECTION,        // initialize an election (pushed by election creator)
    TX_TALLY,           // initialize a tally
    TX_TRUSTEE_TALLY    // commit partial decryption of election (pushed by trustee)
};

// ----------------------------------------------------------------
enum VerifyResult
{
    VR_OK,              // everthing is fine
    VR_SIGN_ERROR,      // could not verify signature
    VR_TX_MISSING,      // required transaction is not in block chain
    VR_USER_REJECTED,   // user has no authority for this election
    VR_PK_MISMATCH,     // [Tally] PK does not match with election creator
    VR_LAST_VOTES,      // [Tally] could not find votes for election
    VR_BALLOT_ERROR,    // [Vote] ballot is not correct
    VR_ELEC_ERROR       // [Election] election attributes are missing/wrong
};

// ----------------------------------------------------------------
// Return string showing result of transaction verifcation
std::string printVerifyResult(const VerifyResult result);

// ----------------------------------------------------------------
class Signable
{
public:

    Signable() {}
    virtual ~Signable() {}

    // ----------------------------------------------------------------

    // Sign this with given sign key
    bool sign(SignKeyPair);

    // Use verification key to check signature
    bool verifySignature() /*const*/;

    // Generate hash
    const uint256 getHash() /*const*/;

    // Get public key
    inline CPubKey getPublicKey() const
    {
        return this->verificationKey;
    }

    // Set public key
    inline void setPublicKey(CPubKey verificationKey)
    {
        this->verificationKey = verificationKey;
    }

    // ----------------------------------------------------------------

    inline bool operator<(/*const*/ Signable& other) /*const*/
    {
        return this->getHash() < other.getHash();
    }

    inline bool operator==(/*const*/ Signable& other) /*const*/
    {
        return this->getHash() == other.getHash();
    }

    // ----------------------------------------------------------------

    virtual std::string toString() const = 0;

private:

    // Verification key, every user can verify this signature with
    CPubKey verificationKey;

    // Created signature for this
    std::vector<unsigned char> signature;

    // Flag, necessary because signature should not be part of hash
    bool hashing = false;

    friend class boost::serialization::access;

    template<typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & this->verificationKey;

        if (!this->hashing)
            a & this->signature;
    }
};

BOOST_CLASS_EXPORT_KEY(Signable)

// ----------------------------------------------------------------
class Transaction : public Signable
{
public:
    Transaction(TxType type = TxType::TX_NONE):
        type(type) {}
    virtual ~Transaction() {}

    // ----------------------------------------------------------------

    inline TxType getType() const
    {
        return this->type;
    }

    // ----------------------------------------------------------------

    virtual std::string toString() const = 0;

    virtual VerifyResult verify() /*const*/ = 0;

private:

    // Type of this transaction
    TxType type;

    // ----------------------------------------------------------------

    friend class boost::serialization::access;

    template<typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & boost::serialization::base_object<Signable>(*this);
        a & this->type;
    }
};

BOOST_CLASS_EXPORT_KEY(Transaction)

#endif
