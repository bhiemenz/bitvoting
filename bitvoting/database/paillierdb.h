/*=============================================================================

Providing access to the database for pallier keys, which are used for the
homomorphic encryption of answers for an election.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_PAILLIERDB_H
#define BITVOTING_PAILLIERDB_H

#include "paillier/paillier.h"
#include "paillier/comparison.h"

#include "bitcoin/uint256.h"
#include "bitcoin/key.h"
#include "database/leveldbwrapper.h"
#include "settings.h"

#include <vector>

#include <boost/foreach.hpp>
#include <boost/serialization/vector.hpp>

// ==========================================================================

typedef struct ElectionPrivateKey_t
{
    // Referencing the election this key is valid for
    uint256 election;

    // Referencing the trustee signature this key is linked to
    CKeyID signatureKey;

    // The actual private key
    paillier_partialkey_t* key;

    // ----------------------------------------------------------------

    bool operator==(const ElectionPrivateKey_t& other) const
    {
        return (election == other.election &&
                signatureKey == other.signatureKey &&
                *key == *(other.key));
    }

    // ----------------------------------------------------------------

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & election;
        a & signatureKey;
        a & key;
    }
} ElectionPrivateKey;

// ==========================================================================

#define KEY_PAILLIER_KEYS "paillier_keys"

class PaillierDB : public LevelDBWrapper
{
private:
    // ----------------------------------------------------------------
    // Singleton

    PaillierDB(const boost::filesystem::path pDatabaseDir):
          LevelDBWrapper(pDatabaseDir, Settings::DEFAULT_DB_CACHE)
    {
        // Read all keys from the database
        this->Read(KEY_PAILLIER_KEYS, this->paillierKeys);
    }

    PaillierDB(PaillierDB const&)        = delete;
    void operator=(PaillierDB const&)    = delete;

    static PaillierDB& GetInstance()
    {
        static PaillierDB instance(boost::filesystem::path(Settings::GetDirectory()) / "databases" / "paillier");
        return instance;
    }

public:
    // Stores all paillier keys
    std::vector<ElectionPrivateKey> paillierKeys;

    // ----------------------------------------------------------------

    // Get all paillier keys valid for the given election
    static std::vector<ElectionPrivateKey> Get(const uint256& election)
    {
        std::vector<ElectionPrivateKey> result;

        // iterate over all my paillier keys
        BOOST_FOREACH(ElectionPrivateKey sk, PaillierDB::GetInstance().paillierKeys)
        {
            if (sk.election == election)
                result.push_back(sk);
        }

        // return result
        return result;
    }

    // Get all my paillier keys
    static std::vector<ElectionPrivateKey> GetAll()
    {
        return PaillierDB::GetInstance().paillierKeys;
    }

    // Store the given paillier key in the database
    static bool Save(ElectionPrivateKey& sk)
    {
        PaillierDB& db = PaillierDB::GetInstance();
        db.paillierKeys.push_back(sk);

        // write to database
        db.Write(KEY_PAILLIER_KEYS, db.paillierKeys);

        return true;
    }
};

#endif // PAILLIERDB_H
