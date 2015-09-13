#include "database/electiondb.h"
#include "utils/comparison.h"

// ================================================================

bool
ElectionDB::Get(const uint256& hash, ElectionManager** manager)
{
    // try to obtain ElectionManager from the database
    ElectionManager* data = NULL;
    bool result = ElectionDB::GetInstance().Read(hash, data);

    if (!result)
        return false;

    // recover the respective election transaction from the blockchain
    Transaction* t = NULL;
    BlockChainDB::getTransaction(hash, &t);
    *manager = data;
    (*manager)->transaction = (TxElection*) t;

    return true;
}

// ----------------------------------------------------------------

std::set<ElectionManager*, pt_cmp>
ElectionDB::GetAll()
{
    std::set<ElectionManager*, pt_cmp> result;

    // iterate over all my elections
    BOOST_FOREACH(uint256 hash, ElectionDB::GetInstance().myElections)
    {
        // obtain ElectionManager
        ElectionManager* current = NULL;
        if (!ElectionDB::Get(hash, &current))
            continue;

        // save to result
        result.insert(current);
    }

    return result;
}

// ----------------------------------------------------------------

bool
ElectionDB::Save(ElectionManager* manager)
{
    Log::i("(ElectionDB) Saving ElectionManager (%s)",
           manager->transaction->getHash().GetHex().c_str());

    ElectionDB& db = ElectionDB::GetInstance();

    // obtain hash of the original transaction
    uint256 hash = manager->transaction->getHash();

    // save to database
    bool result = db.Write(hash, manager);

    if (!result)
        return false;

    // register in shortcut hash-list
    db.myElections.insert(hash);
    db.Write(KEY_MY_ELECTIONS, db.myElections);

    return true;
}

// ----------------------------------------------------------------

bool
ElectionDB::Remove(const uint256 hash)
{
    ElectionDB& db = ElectionDB::GetInstance();

    // remove ElectionManager from database
    bool result = db.Erase(hash);

    if (!result)
        return false;

    // unregister from shortcut-list
    db.myElections.erase(db.myElections.find(hash));
    db.Write(KEY_MY_ELECTIONS, db.myElections);

    return true;
}
