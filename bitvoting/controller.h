/*=============================================================================

This class acts as controller, handling and deligating all incomming
events. Events can be triggered by the UI or the network.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_CONTROLLER_H
#define BITVOTING_CONTROLLER_H

#include "electionmanager.h"
#include "miner.h"
#include "database/paillierdb.h"
#include "gui/mainwindow.h"
#include "net/protocols/transactions.h"
#include "net/protocols/blocks.h"
#include "paillier/paillier.h"

// Forward
class MainWindow;

// ----------------------------------------------------------------
class Controller
{
public:

    Controller(MainWindow&, MiningManager& mining,
               TransactionsProtocol&, BlocksProtocol&);

    // ----- GUI Events -----

    // Transform a given election to an election transaction, export
    // paillier private keys and send transaction into the network
    bool onElectionCreated(Election*, SignKeyPair&, std::string&, paillier_partialkey_t**);

    // Transform a given ballot to an vote transaction
    // and send transaction into the network
    bool onVote(ElectionManager*, std::set<Ballot>&, SignKeyPair&);

    // Creates a tally transaction based on all given
    // information and send transaction into the network
    bool onTally(ElectionManager*, bool, uint256&);

    // Is called if the user wants to
    // import a paillier private key
    bool onNewPaillierKey(ElectionPrivateKey&);

private:

    MainWindow& gui;
    MiningManager& miningManager;
    TransactionsProtocol& transactionProtocol;
    BlocksProtocol& blockProtocol;

    // ----- Network Events -----

    // Is called, of a new transaction was received
    void receiveTransaction(Transaction*);

    // Is called, if a new block was received
    void receiveBlock(Block*);

    // Is called, if a network user requests a block
    std::vector<Block*> receiveBlockRequest(BlockRequestMessage*);

    // ----- Block Events -----
    // Called, after a new valid block was received

    // Process a received election transaction
    void processTxElection(Transaction*);

    // Process a received vote transaction
    void processTxVote(Transaction*);

    // Process a received tally transaction
    void processTxTally(Transaction*);

    // Process a received trustee tally transaction
    void processTxTrusteeTally(Transaction*);

    // ----------------------------------------------------------------

    // Map for callback function for each transaction type
    std::map<TxType, boost::function<void (Transaction*)>> callbacks;
};

#endif
