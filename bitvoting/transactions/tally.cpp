#include "transactions/tally.h"

#include "database/blockchaindb.h"
#include "database/electiondb.h"
#include "transactions/election.h"
#include "transactions/vote.h"

#include <boost/foreach.hpp>

VerifyResult
TxTally::verify() /*const*/
{
    if (!this->verifySignature())
        return VR_SIGN_ERROR;

    // find referenced transaction
    Transaction *tx = NULL;
    if (BlockChainDB::getTransaction(this->election, &tx) != BC_OK)
        return VR_TX_MISSING;

    const TxElection *txElection = dynamic_cast<const TxElection*>(tx);
    if (!txElection)
        return VR_TX_MISSING;

    // find referenced last block
    Block* lastVotesBlock = NULL;
    if (BlockChainDB::getBlock(this->lastBlock, &lastVotesBlock) != BC_OK)
        return VR_LAST_VOTES;

    // check if there is at least one TxVote in the lastBlock,
    // which corresponds to the same election as this tally
    bool valid = false;
    BOOST_FOREACH(Transaction *t, lastVotesBlock->transactions)
    {
        if (t->getType() != TxType::TX_VOTE)
            continue;

        // only consider TxVote transactions in this block
        const TxVote *p = dynamic_cast<const TxVote*>(t);
        if (!p)
            continue;

        // check if vote and tally correspond to same election
        if (p->election != election)
            continue;

        valid = true;
    }

    if (!valid)
        return VR_LAST_VOTES;

    // TODO: check that there is no earlier tally showing to a block further down the block chain

    // check if verification key is indeed the public key of the
    // election creator => verification of signature with correct key
    if (txElection->getPublicKey() != this->getPublicKey())
        return VR_PK_MISMATCH;

    return VR_OK;
}
