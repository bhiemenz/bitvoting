#include "miner.h"
#include "helper.h"
#include "settings.h"
#include "bitcoin/hash.h"
#include "database/blockchaindb.h"
#include "store.h"
#include "utils/comparison.h"
#include "transactions/vote.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <random>

#include <boost/foreach.hpp>


// ---------------------  Miner  --------------------------------

Miner::Miner(MiningManager *mm, std::set<Transaction*, pt_cmp> &transactions) :
    miningManager(mm),
    transactions(transactions)
{
    numThreads = miningManager->numThreads;
    hashTarget = miningManager->hashTarget;

    // number of hardware threads available on this system
    if (numThreads == 0)
        numThreads = boost::thread::hardware_concurrency();

    if (numThreads == 0)
        return;

    // init nonces
    startNonce = Helper::GenerateRandomUInt();
    currentNonce = startNonce + 1;

    // init flags
    threadsDone = 0;
    running = true;
    newBlockFoundFlag = false;

    // start mining process on numThreads threads
    for (unsigned int i = 0; i < numThreads; i++)
        minerThreads.emplace(
                    mm->threadGroup->create_thread(boost::bind(&Miner::mineTransactions, this)));
}

Miner::~Miner()
{
    tearDown();
}

void Miner::onNewBlockFromNetwork(Block *b)
{
    // ignore this event if the miner itself created it
    if (newBlockFoundFlag)
        return;

    Log::i("(Miner) Received new block from network, block hash: %s", b->getHash().ToString().c_str());

    // someone published a new block => there is a new lastBlock
    // and therefore we have to use a new prevHash in mining process
    // -> remove all transactions from current transactions set,
    // which are contained in transactions set of the new block
    // temp = transactions - b->transactions
    std::set<Transaction*> temp;
    std::set_difference(transactions.begin(), transactions.end(),
                        b->transactions.begin(), b->transactions.end(),
                        std::inserter(temp, temp.end()));
    // these are the transactions the miner can be restarted with
    BOOST_FOREACH(Transaction *trans, temp)
    {
        miningManager->addTransaction(trans, false);
    }
    // restart
    mutex.lock();
    tearDown();
    mutex.unlock();
    miningManager->onMinerFinished();
}

void Miner::consumeNextNonces(unsigned int numNext,
                                  unsigned int &lowerBound, unsigned int &upperBound)
{
    boost::mutex::scoped_lock lock(mutex);
    lowerBound = currentNonce;
    // if currentNonce reached startNonce again
    // -> set lowerBound == upperBound to inform about end of nonces
    if (startNonce >= currentNonce && startNonce < currentNonce + numNext)
    {
        currentNonce = startNonce;
    } else {
        currentNonce += numNext;
    }
    upperBound = currentNonce;
}

void Miner::mineTransactions()
{
    Block *newBlock = new Block();

    // set time fixed
    newBlock->header.time = Helper::GetUNIXTimestamp();

    try
    {
        boost::this_thread::interruption_point();


        // --- precompute last block-hash ---
        Block *lastBlock;
        uint256 prevBlockHash;
        BlockChainStatus bcs = BlockChainDB::getLatestBlock(&lastBlock);
        switch( bcs )
        {
        case BC_OK:
            prevBlockHash = lastBlock->getHash();
            break;
        case BC_IS_EMPTY:
            prevBlockHash = BlockChainDB::getGenesisBlock();
            break;
        default:
            abort();
            return;
        }


        // --- create new block ---
        newBlock->header.hashPrevBlock = prevBlockHash;
        // copy transactions to newBlock.transactions
        std::copy(transactions.begin(), transactions.end(),
                  std::inserter( newBlock->transactions, newBlock->transactions.begin() ) );
        newBlock->setPublicKey(miningManager->skp.second);


        // --- search for proof of work ---
        Log::i("(Miner) Searching for proof of work...");
        unsigned int nonce;
        uint256 hashBuf;
        while (true)
        {
            // consume some nonces to process from the pool
            unsigned int lowerBound, upperBound;
            consumeNextNonces(Settings::MINING_NONCES_AT_ONCE, lowerBound, upperBound);
            // if lower == upper == startNonce it means that
            // no 'free' nonces are left, so abort!
            if (lowerBound == upperBound)
            {
                abort();
                return;
            }

            // process every nonce that was consumed
            for (nonce = lowerBound; nonce < upperBound; ++nonce)
            {
                // nonce is the only values which changes
                newBlock->header.nonce = nonce;

                // compute hash
                hashBuf = newBlock->getHash();
                // and check against the hashTarget (difficulty)
                if (hashBuf <= hashTarget)
                {
                    boost::this_thread::interruption_point();
                    // if found, inform mining manager and publish
                    if (onNewBlockFound(newBlock, miningManager->skp))
                        return;
                }
                boost::this_thread::interruption_point();
            }
        }
    }
    catch (boost::thread_interrupted)
    {
        // clear work and shut down
        delete newBlock;
        return;
    }
}

bool Miner::onNewBlockFound(Block *newBlock, SignKeyPair &skp)
{
    Log::i("(Miner) Sucessfully mined a new block, block hash: %s", newBlock->getHash().ToString().c_str());
    mutex.lock();
    if (newBlockFoundFlag)
    {
        // don't let the mutex stay in locked status when returning
        mutex.unlock();
        return false;
    }

    // prevent extern interruption
    newBlockFoundFlag = true;

    // save to database
    /*
       save to database is done via 'publishBlock'
       and its loopback into handling of receiving blocks
    */

    // send to network
    miningManager->publishBlock(newBlock, skp);

    tearDown();
    mutex.unlock();

    Log::i("(Miner) miner found a new block and finishes...");
    miningManager->onMinerFinished();

    return true;
}

void Miner::abort()
{
    mutex.lock();
    threadsDone++;
    Log::i("(Miner) thread aborted. Total number of threads aborted: %d", threadsDone);
    if (threadsDone >= minerThreads.size())
    {
        Log::i("(Miner) All threads aborted -> terminate mining process to be ready for next session");
        // transactions weren't mined, so add them to queue again
        BOOST_FOREACH(Transaction *t, transactions)
        {
            miningManager->addTransaction(t, false);
        }
        tearDown();
        mutex.unlock();
        miningManager->onMinerFinished();
        return;
    }
    mutex.unlock();
}

void Miner::tearDown()
{
    // interrupt parallel threads
    BOOST_FOREACH(boost::thread *t, minerThreads)
    {
        t->interrupt();
        delete t;
    }
    minerThreads.clear();

    running = false;
}


// ---------------------  MiningManager  --------------------------------

MiningManager::MiningManager(boost::thread_group* threadGroup, BlocksProtocol& blocks) :
    threadGroup(threadGroup),
    blockProtocol(blocks)
{
    // get available mining keys
    std::vector<SignKeyPair> skpsMining;
    SignKeyStore::getAllKeysOfType(KEY_MINING, skpsMining);

    // generate a new key if none available
    if (skpsMining.size() == 0)
        SignKeyStore::genNewSignKeyPair(KEY_MINING, this->skp);
    else
        this->skp = skpsMining[0];

    // number of hardware threads available on this system
    this->numThreads = Settings::GetMiningThreads();
    if (this->numThreads <= 0 || this->numThreads > 4)
        this->numThreads = boost::thread::hardware_concurrency();

    // set hashTarget
    hashTarget = 0;
    hashTarget = (hashTarget - 1) >> Settings::MINING_LEADING_ZEROS;

    Log::i("(Miner) Current hash target for mining: %s", hashTarget.GetHex().c_str());
    Log::i("(Miner) Number of threads for mining: %i", numThreads);
}
MiningManager::~MiningManager()
{
}

MINING_ERROR MiningManager::addTransaction(Transaction *t, bool run)
{
    VerifyResult error = t->verify();
    if (error)
    {
        Log::i("(Miner) Reject received transaction (Type: %i | Hash: %s)", t->getType(), t->getHash().ToString().c_str());
        Log::i("(Miner) Reason for rejection: %s", printVerifyResult(error).c_str());
        return MINING_INVALID_TX;
    }

    Log::i("(Miner) Accept received transaction (Type: %i | Hash: %s)", t->getType(), t->getHash().ToString().c_str());

    // if called from failed attempts to mine, insert at front
    if (!run)
        transQueue.insert(transQueue.begin(), t);
    else
        transQueue.push_back(t);

    // try to run
    if (run)
        return runIfPossible();

    return MINING_OK;
}

void MiningManager::onMinerFinished()
{
    // try to run in case there were new transaction in the queue, ready to be mined
    runIfPossible();
}

MINING_ERROR MiningManager::runIfPossible()
{
    boost::mutex::scoped_lock lock(mutex);

    if (numThreads == 0)
        return MINING_FAIL;

    // check if mining process is currently running
    if (m != NULL && m->isRunning())
        return MINING_IN_PROGRESS;

    // check if there are enough transactions to be mined simultaneously
    // i.e. if there are enough after filtering out the transactions
    // which can't be mined into 1 block
    std::set<Transaction*, pt_cmp> toBeProcessed;
    if(!getTransactionsForBlock(toBeProcessed))
        return MINING_NOT_ENOUGH_TX;

    Log::i("(Miner) Starting new mining process with %i transactions", toBeProcessed.size());

    // clear the old mining process
    if (m != NULL)
    {
        delete m;
        m = NULL;
    }

    // create a new mining process (which starts automatically)
    m = new Miner(this, toBeProcessed);

    return MINING_OK;
}

bool MiningManager::getTransactionsForBlock(std::set<Transaction*,pt_cmp> &outTransactions)
{
    // If duplicate vote appears:
    // Take the FIRST vote of all votes, which were signed with the same key
    // and add it to the transactions for the new block.
    // All others wait in queue in the same order they were, so that the last
    // incoming vote will be the last to be put into a block.

    // for each transaction...
    BOOST_FOREACH(Transaction *t1, transQueue)
    {
        // ...check for each already seen transaction...
        bool foundDuplicate = false;
        BOOST_FOREACH(Transaction *t2, outTransactions)
        {
            // ...if we already saw this vote on this (other) transaction...
            if (checkForDuplicateVoteTransaction(t1, t2))
            {
                // ...if "yes": skip
                foundDuplicate = true;
                break;
            }
        }

        // ...if "no" for each: add to already seen transactions
        if (!foundDuplicate)
            outTransactions.emplace(t1);
    }

    // check if enough transactions could be retrieved from queue
    if(outTransactions.size() < Settings::MINING_MIN_TRANSACTIONS)
        return false;

    // remove the outTransactions from transQueue
    // (transQueue := transQueue - outTransactions)
    BOOST_FOREACH(Transaction *t, outTransactions)
    {
        transQueue.erase(std::remove(transQueue.begin(), transQueue.end(), t), transQueue.end());
    }

    return true;
}

bool MiningManager::checkForDuplicateVoteTransaction(Transaction *t1, Transaction *t2)
{
    if(t1->getType() != TxType::TX_VOTE)
        return false;

    if(t1->getType() != t2->getType())
        return false;

    TxVote *txVote1 = dynamic_cast<TxVote*>(t1);
    TxVote *txVote2 = dynamic_cast<TxVote*>(t2);

    if(txVote1->election != txVote2->election)
        return false;

    if(txVote1->getPublicKey() != txVote2->getPublicKey())
        return false;

    return true;
}

void MiningManager::onNewBlockFromNetwork(Block *b)
{
    if (m != NULL)
        m->onNewBlockFromNetwork(b);
}
