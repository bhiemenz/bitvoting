/*=============================================================================

These classes are responsible for mining of blocks of transactions.
This is necessary so that every action based on some transactions can be sure
about the validity of the transactions needed. Therefore only transactions
that were mined into a block are considered.
To mine a set of transactions into one block, the miner creates a proof of work
based on the hashes of the transactions to be mined, the hash of the previous
block in the chain and a nonce. If this hash is lower than the hash-target
provided by the network, the proof of work is accepted.
The hash-target controls the difficulty of creating such a proof of work.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_MINER_H
#define BITVOTING_MINER_H

#include "block.h"
#include "transaction.h"
#include "bitcoin/uint256.h"
#include "net/protocols/blocks.h"

#include <boost/thread.hpp>
#include <boost/signals2.hpp>
#include <set>
#include <utility>
#include <set>
#include <thread>

/*
Note:
    1) All incoming transactions are stored in set transQueue
    2) If set reaches a certain size (not only check with function!)
        2.1) Another mining process is already started, wait
        2.2) If no mining process is already started, start mining
    3) Run proof of work for subset of transQueue
    4) Create new block based on proof of work
*/

// Status code for mining results
enum MINING_ERROR
{
    MINING_OK,

    MINING_NOT_ENOUGH_TX,

    MINING_IN_PROGRESS,

    MINING_INVALID_TX,

    MINING_FAIL
};

class MiningManager;

/*
 * Mining strategy:
 * Starting at a random starting nonce, we count up until we reach the
 * starting nonce again, using maximum(unsigned int) == -1.
 * Every thread consumes several nonces at once for processing to
 * minimize mutex-protected accesses.
 */
class Miner
{
public:
    Miner(MiningManager *mm, std::set<Transaction*, pt_cmp> &transactions);
    ~Miner();

    // handles new blocks from the network (others), i.e. searchs
    // for transactions the miner is currently mining but were already
    // included in a block.
    // Stops the mining process due to the need of prevHash (which changed)
    void onNewBlockFromNetwork(Block *b);

    // returns if the mining process is currently running
    bool isRunning()
    {
        return running;
    }

    // concatenates the transactions and builds the hash
    static uint256 hashTransactions(std::set<Transaction *, pt_cmp> &transactions);

private:

    // reserves next 'numNext' nonces and therefore increments
    // the 'processedNonces' by 'numNonces'.
    // Sets lowerBound to the lowest unprocessed nonce and
    // upperBound to the maximum of the reserved interval.
    void consumeNextNonces(unsigned int numNext,
                           unsigned int &lowerBound, unsigned int &upperBound);

    // handles successful mining, i.e. finding a proof of work for given transactions
    bool onNewBlockFound(Block *newBlock, SignKeyPair &skp);

    // Run mining of given transaction
    void mineTransactions();

    // Called if a thread wants to abort because of failures
    void abort();

    // Interrupts mining threads
    void tearDown();

    // new transactions, which can be used for new proof of work
    std::set<Transaction*, pt_cmp> transactions;

    boost::mutex mutex;
    bool newBlockFoundFlag;

    // nonces from 'startNonce' to 'currentNonce' were already processed
    unsigned int currentNonce;
    unsigned int startNonce;

    bool running = false;

    // the number of threads that finished (e.g. by aborting)
    unsigned int threadsDone;

    std::set<boost::thread*> minerThreads;

    MiningManager *miningManager = NULL;

    // the target/difficulty for mining process
    // a proof of work is found, if hash < hashTarget
    uint256 hashTarget;

    // the number of threads to be used in the mining process
    unsigned int numThreads;
};


// MiningManager provides the possibility for future implementing
// different mining-strategies and mining multiple blocks at once
class MiningManager
{
friend class Miner;

public:
    MiningManager(boost::thread_group*, BlocksProtocol&);
    ~MiningManager();

    // publish block to network
    void publishBlock(Block* b, SignKeyPair& skp)
    {
        this->blockProtocol.Publish(b, skp);
    }

    uint256 getHashTarget()
    {
        return hashTarget;
    }

    // verifies the transaction and adds it to the queue if verify=true
    // also tries to run the mining process on the current queue
    // (see runIfPossible)
    // if parameter run is set to false, there will be no attempt to
    // run the mining process after inserting the transaction to the front of queue
    MINING_ERROR addTransaction(Transaction *t, bool run = true);

    // handles new blocks from the network (others), i.e. informs
    // the currently running mining process
    void onNewBlockFromNetwork(Block *b);

private:
    BlocksProtocol& blockProtocol;

    // the number of threads to be used in the mining process
    unsigned int numThreads;

    // the sign-key-pair to be used to sign the new block
    SignKeyPair skp;

    // the target/difficulty for mining process
    // a proof of work is found, if hash < hashTarget
    uint256 hashTarget;

    // queue of transactions to be mined
    std::vector<Transaction*> transQueue;

    boost::mutex mutex;
    boost::thread_group *threadGroup = NULL;
    Miner *m = NULL;

    // callback signaling that the current mining process finished
    // e.g. by aborting or finding a new block
    void onMinerFinished();

    // runs mining process if threads are free
    // and there are enough transactions
    MINING_ERROR runIfPossible();

    // gets and removes all transactions from transQueue,
    // which should be included in the next block
    bool getTransactionsForBlock(std::set<Transaction *, pt_cmp> &outTransactions);

    // check if two transactions are duplicate votes
    bool checkForDuplicateVoteTransaction(Transaction *t1, Transaction *t2);
};

#endif
