#include "transactions/trustee_tally.h"

#include "database/blockchaindb.h"
#include "database/electiondb.h"
#include "transactions/election.h"
#include "transactions/tally.h"

VerifyResult
TxTrusteeTally::verify() /*const*/
{
    if (!this->verifySignature())
            return VR_SIGN_ERROR;

    // find referenced tally transaction
    Transaction *tx;
    if (BlockChainDB::getTransaction(this->tally, &tx) != BC_OK)
        return VR_TX_MISSING;

    TxTally *txTally = dynamic_cast<TxTally*>(tx);
    if (!txTally)
        return VR_TX_MISSING;

    // find referenced election by tally (2nd hop
    tx = NULL;
    if (BlockChainDB::getTransaction(txTally->election, &tx) != BC_OK)
        return VR_TX_MISSING;

    TxElection *txElection = dynamic_cast<TxElection*>(tx);
    if (!txElection)
        return VR_TX_MISSING;

    // check if verification key is indeed the public key referenced
    // in election => verification of signature with correct key
    ElectionManager *em = NULL;
    if (!ElectionDB::Get(txElection->getHash(), &em))
        return VR_TX_MISSING;

    if (!em->isTrusteeEligible(this->getPublicKey()))
        return VR_USER_REJECTED;

    // check that number of answers match number of questions
    if (this->partialDecryption.size() != txElection->election->questions.size())
        return VR_BALLOT_ERROR;

    // check that all questions at most once
    std::set<uint160> checked;
    BOOST_FOREACH(TalliedBallots ballot, this->partialDecryption)
    {
        if (!ballot.answers)
            return VR_BALLOT_ERROR;

        // check for duplicate question ID
        if (checked.find(ballot.questionID) != checked.end())
            return VR_BALLOT_ERROR;

        BOOST_FOREACH(Question question, txElection->election->questions)
        {
            if (ballot.questionID != question.id)
                continue;

            checked.insert(question.id);
        }
    }

    // check that no unknown questions were answered
    bool countCheck = this->partialDecryption.size() == checked.size();
    return countCheck ? VR_OK : VR_BALLOT_ERROR;
}
