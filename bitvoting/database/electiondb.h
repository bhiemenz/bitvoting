/*=============================================================================

Persistently stores the additional election information provided by all
ElectionManagers for this client.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef ELECTIONDB_H
#define ELECTIONDB_H

#include "database/leveldbwrapper.h"
#include "database/blockchaindb.h"
#include "settings.h"
#include "bitcoin/uint256.h"
#include "electionmanager.h"
#include "transactions/election.h"
#include "paillier/serialization.h"

#include <map>

#include <boost/filesystem/path.hpp>
#include <boost/foreach.hpp>

// ==========================================================================

#define KEY_MY_ELECTIONS "election_list"

class ElectionDB : public LevelDBWrapper
{
private:
    // ----------------------------------------------------------------
    // Singleton

    ElectionDB(const boost::filesystem::path pDatabaseDir):
          LevelDBWrapper(pDatabaseDir, Settings::DEFAULT_DB_CACHE)
    {
        // load all hashes from the database
        this->Read(KEY_MY_ELECTIONS, this->myElections);
    }

    ElectionDB(ElectionDB const&)        = delete;
    void operator=(ElectionDB const&)    = delete;

    static ElectionDB& GetInstance()
    {
        static ElectionDB instance(boost::filesystem::path(Settings::GetDirectory())
                                   / "databases" / "elections");
        return instance;
    }

public:
    // Stores the hashes for election transactions
    std::set<uint256> myElections;

    // ----------------------------------------------------------------

    // Recover an election manager from the database
    static bool Get(const uint256&, ElectionManager**);

    // Retrieve all ElectionManagers which I am eligible to
    static std::set<ElectionManager*, pt_cmp> GetAll();

    // Store an election manager to database
    static bool Save(ElectionManager*);

    // Erase an election manager from database
    static bool Remove(const uint256);
};

#endif // ELECTIONDB_H
