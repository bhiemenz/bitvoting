#include "transactions/vote.h"

#include "election.h"
#include "database/blockchaindb.h"
#include "database/electiondb.h"
#include "transactions/election.h"

VerifyResult
TxVote::verify() /*const*/
{
    if (!this->verifySignature())
            return VR_SIGN_ERROR;

    // find referenced election
    Transaction *tx = NULL;
    if (BlockChainDB::getTransaction(this->election, &tx) != BC_OK)
        return VR_TX_MISSING;

    TxElection *txElection = dynamic_cast<TxElection*>(tx);
    if (!txElection)
        return VR_TX_MISSING;

    // check that all questions at most once
    std::set<uint160> checked;
    BOOST_FOREACH(EncryptedBallot ballot, this->ballots)
    {
        if (!ballot.answer)
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
    if (this->ballots.size() != checked.size())
        return VR_BALLOT_ERROR;

    // check if verification key is indeed the public key referenced
    // in election => verification of signature with correct key
    ElectionManager *em = NULL;
    if (!ElectionDB::Get(txElection->getHash(), &em))
        return VR_USER_REJECTED;

    bool keyCheck = em->isVoterEligible(this->getPublicKey());
    return keyCheck ? VR_OK : VR_USER_REJECTED;
}
