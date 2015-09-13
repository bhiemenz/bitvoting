#include "controller.h"

#include "helper.h"
#include "store.h"
#include "database/blockchaindb.h"
#include "database/electiondb.h"
#include "transactions/election.h"
#include "transactions/vote.h"
#include "transactions/tally.h"
#include "transactions/trustee_tally.h"

Controller::Controller(MainWindow& gui, MiningManager& mining,
                       TransactionsProtocol& transactionProtocol,
                       BlocksProtocol& blockProtocol):
    gui(gui),
    miningManager(mining),
    transactionProtocol(transactionProtocol),
    blockProtocol(blockProtocol)
{
    gui.setController(this);

    // register network transaction handler
    transactionProtocol.SetCallback(TxType::TX_ELECTION, boost::bind(&Controller::receiveTransaction, this, _1));
    transactionProtocol.SetCallback(TxType::TX_VOTE, boost::bind(&Controller::receiveTransaction, this, _1));
    transactionProtocol.SetCallback(TxType::TX_TALLY, boost::bind(&Controller::receiveTransaction, this, _1));
    transactionProtocol.SetCallback(TxType::TX_TRUSTEE_TALLY, boost::bind(&Controller::receiveTransaction, this, _1));

    blockProtocol.SetCallback(boost::bind(&Controller::receiveBlock, this, _1));
    blockProtocol.SetRequestCallback(boost::bind(&Controller::receiveBlockRequest, this, _1));

    // callbacks for transaction processing
    this->callbacks[TxType::TX_ELECTION] = boost::bind(&Controller::processTxElection, this, _1);
    this->callbacks[TxType::TX_VOTE] = boost::bind(&Controller::processTxVote, this, _1);
    this->callbacks[TxType::TX_TALLY] = boost::bind(&Controller::processTxTally, this, _1);
    this->callbacks[TxType::TX_TRUSTEE_TALLY] = boost::bind(&Controller::processTxTrusteeTally, this, _1);
}

bool Controller::onElectionCreated(Election* election, SignKeyPair& signKey, std::string& directory, paillier_partialkey_t** privateKeys)
{
    Log::i("(Controller) onElectionCreated called");

    TxElection* txElection = new TxElection(election);

    // already set the verification key for hashing
    txElection->setPublicKey(signKey.second);

    // It is assumed that there are as much pailler_partialkey_t*
    // as trustees registered in the election!
    if (election->trustees.size() != election->encPubKey->decryptServers)
        return false;

    Log::i("(Controller) Exporting paillier private keys");

    // Export private paillier keys
    uint256 hash = txElection->getHash();
    int i = 0;
    BOOST_FOREACH(CKeyID trusteeSignKey, election->trustees)
    {
        ElectionPrivateKey epk;
        epk.election = hash;
        epk.key = privateKeys[i++];
        epk.signatureKey = trusteeSignKey;

        try
        {
            Helper::SaveToFile(epk, directory + "/trustee_" + trusteeSignKey.GetHex().substr(0, 8), true);
        }
        catch(...)
        {
            Log::e("(Controller) Could not export paillier private key. Please try again");
            return false;
        }
    }

    // own network callback is automatically called
    if (!this->transactionProtocol.Publish(txElection, signKey))
        return false;

    return true;
}

bool Controller::onVote(ElectionManager *em, std::set<Ballot> &votes, SignKeyPair &skp)
{
    Log::i("(Controller) onVote called");

    TxVote *txVote;
    VotingResult result = em->createVote(votes, &txVote);
    if (result != VotingResult::OK)
    {
        Log::e("(Controller) Unable to create vote (ElectionManager returned %d)", result);
        return false;
    }

    // send vote transaction into network
    if (!this->transactionProtocol.Publish(txVote, skp))
        return false;

    return true;
}

bool Controller::onTally(ElectionManager* manager, bool ending, uint256& lastBlock)
{
    Log::i("(Controller) onTally called (last block: %s)", lastBlock.GetHex().c_str());

    TxTally* txTally = new TxTally();
    txTally->election = manager->transaction->getHash();
    txTally->endElection = ending;
    txTally->lastBlock = lastBlock;

    // get creation key
    SignKeyPair key;
    if (!SignKeyStore::getSignKeyPair(manager->transaction->getPublicKey().GetID(), key))
        return false;

    if (!this->transactionProtocol.Publish(txTally, key))
        return false;

    return true;
}

bool Controller::onNewPaillierKey(ElectionPrivateKey& epk)
{
    Log::i("(Controller) onNewPaillierKey called");

    // check if i am involved in the election
    ElectionManager* manager = NULL;
    if (!ElectionDB::Get(epk.election, &manager))
        return false;

    // check if i have the corresponding signing key
    SignKeyPair signKey;
    if (!SignKeyStore::getSignKeyPair(epk.signatureKey, signKey))
        return false;

    // check that the corresponding key is eligible as a trustee
    if (!manager->isTrusteeEligible(signKey.second))
        return false;

    // iterate over all available tallies
    std::map<uint256, std::set<uint256>>::iterator iter;
    for (iter = manager->tallies.begin(); iter != manager->tallies.end(); iter++)
    {
        bool alreadyVoted = false;

        // check if there is already a trustee tally signed w/ the corresponding key
        BOOST_FOREACH(uint256 ttHash, iter->second)
        {
            Transaction* transaction = NULL;
            if (BlockChainDB::getTransaction(ttHash, &transaction) != BlockChainStatus::BC_OK)
                continue;

            // check if keys match
            TxTrusteeTally* txTrusteeTally = (TxTrusteeTally*) transaction;
            if (txTrusteeTally->getPublicKey() != signKey.second)
                continue;

            alreadyVoted = true;
            break;
        }

        if (alreadyVoted)
            continue;

        // load original tally message
        Transaction* transaction = NULL;
        if (BlockChainDB::getTransaction(iter->first, &transaction) != BlockChainStatus::BC_OK)
            continue;

        TxTally* txTally = (TxTally*) transaction;

        // create tx trustee tally
        TxTrusteeTally* txTrusteeTally = NULL;
        manager->createTrusteeTally(txTally, epk.key, &txTrusteeTally);
        this->transactionProtocol.Publish(txTrusteeTally, signKey);
    }

    return true;
}

void Controller::processTxElection(Transaction* in)
{
    TxElection *txElection = dynamic_cast<TxElection*>(in);

    // create new election manager and save to db
    ElectionManager *em = new ElectionManager(txElection);

    if(em->amIInvolved())
        ElectionDB::Save(em);
}

void Controller::processTxVote(Transaction* in)
{
    TxVote *txVote = dynamic_cast<TxVote*>(in);
    if(!txVote)
        return;

    // check if I am involved in the referenced election
    ElectionManager *myElection = NULL;
    if (!ElectionDB::Get(txVote->election, &myElection))
        return;

    Log::i("(Controller) Register vote for election I am involved in");

    CKeyID voter = txVote->getPublicKey().GetID();

    // check if voter's vote was already registered, if not do
    myElection->votesRegistered.insert(voter);

    // check if this is my vote
    if (SignKeyStore::containsSignKeyPair(voter))
        myElection->myVotes[voter] = txVote->getHash();

    // save changes
    ElectionDB::Save(myElection);
}

void Controller::processTxTally(Transaction* in)
{
    TxTally *txTally = dynamic_cast<TxTally*>(in);

    // check if i am involved in the election
    ElectionManager *myElection = NULL;
    if (!ElectionDB::Get(txTally->election, &myElection))
        return;

    // if election was already ended, reject/ignore
    if(myElection->ended)
        return;

    Log::i("(Controller) Register tally!");

    myElection->ended = txTally->endElection;

    // create new tally entry in em tallies
    myElection->tallies[txTally->getHash()];

    ElectionDB::Save(myElection);

    if (!myElection->amITrustee())
        return;

    // get all homomorphic keys I have for this election
    std::vector<ElectionPrivateKey> keys = PaillierDB::Get(txTally->election);

    Log::i("(Controller) Creating proof w/ respective paillier keys (Count: %d)", keys.size());

    // compute tallies
    BOOST_FOREACH(ElectionPrivateKey privKey, keys)
    {
        // check that i have the corresponding signing key &
        // check that the corresponding key is eligible
        SignKeyPair signKey;
        if (!SignKeyStore::getSignKeyPair(privKey.signatureKey, signKey) ||
                !myElection->isTrusteeEligible(signKey.second))
        {
            Log::e("(Controller) Found illegal paillier key!");
            continue;
        }

        // create tx trustee tally
        TxTrusteeTally *txTrusteeTally = NULL;
        myElection->createTrusteeTally(txTally, privKey.key, &txTrusteeTally);
        this->transactionProtocol.Publish(txTrusteeTally, signKey);
    }
}

void Controller::processTxTrusteeTally(Transaction* in)
{
    TxTrusteeTally *txTrusteeTally = dynamic_cast<TxTrusteeTally*>(in);

    // load tally transaction
    Transaction *tx = NULL;
    if(BlockChainDB::getTransaction(txTrusteeTally->tally, &tx) != BC_OK)
        return;

    TxTally *txTally = dynamic_cast<TxTally*>(tx);

    uint256 tallyHash = txTally->getHash();

    // load election transaction
    tx = NULL;
    if(BlockChainDB::getTransaction(txTally->election, &tx) != BC_OK)
        return;

    TxElection *txElection = dynamic_cast<TxElection*>(tx);

    // check if i am involved in this election
    ElectionManager *myElection = NULL;
    if (!ElectionDB::Get(txElection->getHash(), &myElection))
        return;

    Log::i("(Controller) Register Trustee Tally");

    // save/update tally and its corresponding trustee tally
    myElection->tallies[tallyHash].insert(txTrusteeTally->getHash());

    // if election was already tallied, nothing more has to be done
    if (!myElection->results.count(tallyHash))
    {
        paillier_pubkey_t* encPubKey = myElection->transaction->election->encPubKey;

        // tally election, if enough trustee tallies exist now
        if (myElection->tallies[tallyHash].size() >= encPubKey->threshold)
        {
            Log::i("(Controller) Trustee Tallies reached threshold, performing tally...");

            if (!myElection->tally(tallyHash))
                Log::e("(Controller) Error during tallying!");
        }
    }

    // save updated election manager to db
    ElectionDB::Save(myElection);
}

void Controller::receiveTransaction(Transaction* in)
{
    // check if transaction is already known
    if (BlockChainDB::containsTransaction(in->getHash()))
        return;

    Log::i("(Controller) Received unknown transaction (Type: %i | Hash: %s), forward to Miner", in->getType(), in->getHash().ToString().c_str());

    // forward to miner
    miningManager.addTransaction(in);
}

void Controller::receiveBlock(Block* b)
{
    Log::i("(Controller) Received a new block (Hash: %s)", b->getHash().ToString().c_str());

    BlockChainStatus bcs;

    // ----- Verify block -----

    // --- verify header ---

    //  check last block
    Block *lastBlock;
    bcs = BlockChainDB::getLatestBlock(&lastBlock);
    uint256 lastBlockHash;
    long long lastBlockTime = 0;
    switch( bcs )
    {
    case BC_OK:
        lastBlockHash = lastBlock->getHash();
        lastBlockTime = lastBlock->header.time;
        break;
    case BC_IS_EMPTY:
        lastBlockHash = BlockChainDB::getGenesisBlock();
        break;
    default:
        return;
    }

    if ( b->header.hashPrevBlock != lastBlockHash )
    {
        Log::i("(Controller) Received a new block, but its previous hash does not match last block in block chain -> reject block");
        return;
    }

    //  check time is reasonable (not future, not before gensis_block)
    if (b->header.time > Helper::GetUNIXTimestamp()
            || b->header.time < lastBlockTime)
    {
        Log::i("(Controller) Received block has implausible creation time -> reject block");
        return;
    }

    //  check hash (and therefore implicitly the nonce)
    uint256 hashTarget = 0;
    hashTarget = (hashTarget - 1) >> Settings::MINING_LEADING_ZEROS;
    uint256 hash = b->getHash();
    if ( !(hash <= hashTarget) )
    {
        Log::i("(Controller) Received block`s hash is not lower than target -> reject block");
        return;
    }

    // check existance of block
    if ( BlockChainDB::containsBlock(hash) )
    {
        Log::i("(Controller) Received block already exists in block chain -> reject block");
        return;
    }

    // --- verify transactions ---

    //  check correctness of each transaction
    BOOST_FOREACH(Transaction *t, b->transactions)
    {
        // check existance of transaction
        if(BlockChainDB::containsTransaction(t->getHash()))
        {
            Log::i("(Controller) Received block contains transactions, that are already part of block chain -> reject block");
            return;
        }

        // check transaction
        VerifyResult error = t->verify();
        if(error)
        {
            Log::i("(Controller) Reject transaction in block (Block hash: %s | Tx Type: %i | Tx Hash: %s)",
                   b->getHash().ToString().c_str(), t->getType(), t->getHash().ToString().c_str());
            Log::i("(Controller) Reason for rejection: %s", printVerifyResult(error).c_str());
            return;
        }

    }

    Log::i("(Controller) Received block passes every check -> accept block");

    // ----- Accept block -----

    // Save persistently
    BlockChainStatus result = BlockChainDB::addBlock(b);
    if (result != BlockChainStatus::BC_OK)
    {
        Log::i("(Controller) Could not save new block (Reason: %d)", result);
        return;
    }

    // Let the miner know about new block in case he is currently
    // mining one of the included transactions
    miningManager.onNewBlockFromNetwork(b);

    // process each transaction
    BOOST_FOREACH(Transaction *tx, b->transactions)
    {
        Log::i("(Controller) Processing Transaction (Type: %d | Hash: %s)",
               tx->getType(), tx->getHash().GetHex().c_str());
        this->callbacks[tx->getType()](tx);
    }

    // update UI
    this->gui.updateElectionList();
}

std::vector<Block*> Controller::receiveBlockRequest(BlockRequestMessage* message)
{
    std::vector<Block*> result;

    if (!BlockChainDB::containsBlock(message->block))
        return result;

    if (message->following)
    {
        // get all blocks
        std::vector<Block*> blocks;
        if (BlockChainDB::getAllBlocks(message->block, blocks) != BlockChainStatus::BC_OK)
            return result;

        return blocks;
    }

    Block *block = NULL;
    if (BlockChainDB::getBlock(message->block, &block) == BC_OK)
        result.push_back(block);

    return result;
}
