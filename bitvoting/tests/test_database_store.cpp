#include "tests/test_database_store.h"

#include "electionmanager.h"
#include "store.h"
#include "bitcoin/key.h"
#include "database/electiondb.h"
#include "database/signkeydb.h"
#include "tests/test_blockchain.h"

// Generate and store a new sign key to the store
void genAndStoreTestSignKey(unsigned int num, Role role, std::vector<uint160> &idsOut)
{
    for(unsigned int i = 0; i < num; i++)
    {
        SignKeyPair keyPair;
        SignKeyStore::genNewSignKeyPair(role, keyPair);
        idsOut.push_back(keyPair.second.GetID());
    }
}

// Test for SignKeyStore and, by implication, SignKeyDB, too
void testSignKeyStore()
{
    // save ids to enable cleaning up the database after tests
    std::vector<uint160> keyIDs;

    // get current size of database/store
    std::vector<SignKeyPair> allKeys;
    SignKeyStore::getAllKeys(allKeys);
    unsigned int dbSize = allKeys.size();

    // ----- Generate some sign keys -----
    genAndStoreTestSignKey(23, KEY_VOTE, keyIDs);
    genAndStoreTestSignKey(7, KEY_TRUSTEE, keyIDs);
    genAndStoreTestSignKey(9, KEY_ELECTION, keyIDs);

    // ----- Get all keys -----
    allKeys.clear();
    SignKeyStore::getAllKeys(allKeys);
    assert(allKeys.size() == 39 + dbSize);

    // ----- Get keys of certain role -----
    // greater than, because other keys are maybe already stored
    std::vector<SignKeyPair> keysOfType;
    SignKeyStore::getAllKeysOfType(KEY_VOTE, keysOfType);
    assert(keysOfType.size() >= 23);
    SignKeyStore::getAllKeysOfType(KEY_TRUSTEE, keysOfType);
    assert(keysOfType.size() >= 7);
    SignKeyStore::getAllKeysOfType(KEY_ELECTION, keysOfType);
    assert(keysOfType.size() >= 9);

    // ----- Add and remove key pair -----
    SignKeyPair keyPair;
    SignKeyStore::genNewSignKeyPair(KEY_VOTE, keyPair);

    bool isStored = false;
    isStored = SignKeyStore::containsSignKeyPair(keyPair.second.GetID());
    assert(isStored);
    isStored = false;
    SignKeyPair temp;
    isStored = SignKeyStore::getSignKeyPair(keyPair.second.GetID(), temp);
    assert(isStored);
    assert(temp.second.GetHash() == keyPair.second.GetHash());

    SignKeyStore::removeSignKeyPair(keyPair.second.GetID());

    isStored = true;
    isStored = SignKeyStore::getSignKeyPair(keyPair.second.GetID(), temp);
    assert(!isStored);
    isStored = true;
    isStored = SignKeyStore::containsSignKeyPair(keyPair.second.GetID());
    assert(!isStored);

    // ------ Cleaning up -----
    for(unsigned int i = 0; i < keyIDs.size(); i++)
    {
        uint160 id = keyIDs.at(i);
        assert(SignKeyStore::containsSignKeyPair(id));
        SignKeyStore::removeSignKeyPair(id);
        assert(!SignKeyStore::containsSignKeyPair(id));
    }

    allKeys.clear();
    SignKeyStore::getAllKeys(allKeys);
    assert(allKeys.size() == dbSize);
}

// Test for ElectionDB
void testElectionDB()
{
    // ----- Create an txElection -----
    Transaction *tester = NULL;
    random_transaction_election(&tester);
    TxElection *tx = dynamic_cast<TxElection*>(tester);
    CKey sk;
    sk.MakeNewKey();
    CPubKey pk = sk.GetPubKey();
    tx->setPublicKey(pk);

    // ----- Create election manager -----
    ElectionManager *em = new ElectionManager(tx);

    // ----- Save to database -----
    ElectionDB::Save(em);

    // ----- Get from database -----
    ElectionManager *em2 = NULL;
    bool isStored = false;
    isStored = ElectionDB::Get(tx->getHash(), &em2);
    assert(isStored);
    // do not check for transaction, since it is stored in block chain
    assert(em->tallies == em2->tallies && em->ended == em2->ended &&
           em->myVotes == em2->myVotes && em->votesRegistered == em2->votesRegistered);

    // ----- Remove from database -----
    ElectionDB::Remove(tx->getHash());
    em2 = NULL;
    isStored = true;
    isStored = ElectionDB::Get(tx->getHash(), &em2);
    assert(!isStored);

    // ----- Cleaning up -----
    delete em2;
    em2 = NULL;
    delete em;
    em = NULL;
    delete tx;
    tx = NULL;
}

// Start all tests in this file
void test_database_store()
{
    Log::i("(Test) # Test: Database and Store");
    testSignKeyStore();
    testElectionDB();
}

