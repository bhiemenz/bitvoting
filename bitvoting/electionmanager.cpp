#include "electionmanager.h"

#include "store.h"
#include "database/blockchaindb.h"

#include <boost/foreach.hpp>

// ================================================================

bool
ElectionManager::isVoterEligible(CPubKey key) const
{
    // check if the given key is listed as a voter
    CKeyID keyID = key.GetID();
    BOOST_FOREACH(CKeyID current, this->transaction->election->voters)
    {
        if (keyID == current)
            return true;
    }

    return false;
}

// ----------------------------------------------------------------

bool
ElectionManager::isTrusteeEligible(CPubKey key) const
{
    // check if the given key is listed as a trustee
    CKeyID keyID = key.GetID();
    BOOST_FOREACH(CKeyID current, this->transaction->election->trustees)
    {
        if (keyID == current)
            return true;
    }

    return false;
}

// ----------------------------------------------------------------

bool
ElectionManager::amICreator() const
{
    // get all my election keys
    std::vector<SignKeyPair> electionKeys;
    SignKeyStore::getAllKeysOfType(KEY_ELECTION, electionKeys);

    // get signature key of original transaction
    CKeyID creatorKeyID = transaction->getPublicKey().GetID();

    // check if one of my election keys matches the creation key
    BOOST_FOREACH(SignKeyPair current, electionKeys)
    {
        if(creatorKeyID == current.second.GetID())
            return true;
    }

    return false;
}

// ----------------------------------------------------------------

bool
ElectionManager::amIVoter() const
{
    // get all my vote keys
    std::vector<SignKeyPair> voteKeys;
    SignKeyStore::getAllKeysOfType(KEY_VOTE, voteKeys);

    // check if one of my keys is listed as a voter
    BOOST_FOREACH(SignKeyPair current, voteKeys)
    {
        if(isVoterEligible(current.second))
            return true;
    }

    return false;
}

// ----------------------------------------------------------------

bool
ElectionManager::amITrustee() const
{
    // get all my trustee keys
    std::vector<SignKeyPair> trusteeKeys;
    SignKeyStore::getAllKeysOfType(KEY_TRUSTEE, trusteeKeys);

    // check if one of my keys is listed as a trustee
    BOOST_FOREACH(SignKeyPair current, trusteeKeys)
    {
        if(isTrusteeEligible(current.second))
            return true;
    }

    return false;
}

// ----------------------------------------------------------------

bool
ElectionManager::amIInvolved() const
{
    return amICreator() || amIVoter() || amITrustee();
}

// ----------------------------------------------------------------

bool
ElectionManager::alreadyVoted() const
{
    return (this->myVotes.size() > 0);
}

// ----------------------------------------------------------------

bool
ElectionManager::resultsAvailable() const
{
    return (this->results.size() > 0);
}

// ----------------------------------------------------------------

bool
ElectionManager::getQuestion(uint160 id, Question& questionOut) const
{
    // find given question ID
    BOOST_FOREACH(Question question, this->transaction->election->questions)
    {
        if (question.id != id)
            continue;

        questionOut = question;
        return true;
    }

    return false;
}

// ----------------------------------------------------------------

VotingResult
ElectionManager::createVote(std::set<Ballot> votes, TxVote** voteOut)
{
    // first simple test to check if all questions are answered
    if (votes.size() != this->transaction->election->questions.size())
        return VotingResult::INVALID_COUNT;

    // check that all questions are answered at most once
    std::set<uint160> checked;
    BOOST_FOREACH(Ballot ballot, votes)
    {
        // check for duplicate question ID
        if (checked.find(ballot.questionID) != checked.end())
            return VotingResult::DUPLICATE_QUESTION;

        // check that question exists
        BOOST_FOREACH(Question question, this->transaction->election->questions)
        {
            if (ballot.questionID != question.id)
                continue;

            checked.insert(question.id);
        }
    }

    // check that all answered questions were checked
    if (votes.size() != checked.size())
        return VotingResult::UNKNOWN_QUESTION;

    // encrypt ballots
    std::set<EncryptedBallot> result;
    BOOST_FOREACH(Ballot ballot, votes)
    {
        // do not include vote if abstained
        if (ballot.answer == -1)
            continue;

        // encrypt vote
        paillier_pubkey_t* key = this->transaction->election->encPubKey;
        paillier_plaintext_t* plain = paillier_plaintext_from_ui(ballot.answer);

        // As long as the plaintext equals its index (i.e. plaintexts are 0 and 1 in this order)
        // we can just take the answer as choice.
        PLAINTEXT_SELECTION choice = static_cast<PLAINTEXT_SELECTION>(ballot.answer);
        paillier_ciphertext_proof_t* cipher = paillier_enc_proof(key, choice, paillier_get_rand_devurandom, NULL);

        // prepare ballot
        EncryptedBallot encrypt;
        encrypt.questionID = ballot.questionID;
        encrypt.answer = cipher;

        // free temporary variables
        paillier_freeplaintext(plain);

        result.insert(encrypt);
    }

    // prepare vote
    TxVote* vote = new TxVote();
    vote->election = this->transaction->getHash();
    vote->ballots = result;

    *voteOut = vote;

    return VotingResult::OK;
}

// ----------------------------------------------------------------

bool
ElectionManager::tally(const uint256 &tallyHash)
{
    // check if trustee tallies exist
    if (!this->tallies.count(tallyHash))
        return false;

    std::set<uint256> trusteeTallies = this->tallies[tallyHash];

    // collect all ballots
    std::set<TalliedBallots> ballots;
    BOOST_FOREACH(uint256 ttHash, trusteeTallies)
    {
        // get trustee tally transaction
        Transaction* transaction = NULL;
        BlockChainDB::getTransaction(ttHash, &transaction);

        TxTrusteeTally* trusteeTally = (TxTrusteeTally*) transaction;

        // gather
        ballots.insert(trusteeTally->partialDecryption.begin(),
                       trusteeTally->partialDecryption.end());
    }

    paillier_pubkey_t* key = this->transaction->election->encPubKey;

    /*
       reason for use of vector instead of set:
       easy conversion from vector to array (pointer), which doesn't
       apply for sets, since former are memory contiguous.
    */
    // sort all ballots reg. question IDs
    std::map<uint160, std::vector<paillier_partialdecryption_proof_t*>> decryptionSets;
    BOOST_FOREACH(TalliedBallots ballot, ballots)
    {
        // check if question was seen before, otherwise initialize
        if (!decryptionSets.count(ballot.questionID))
            decryptionSets[ballot.questionID];

        // check if for this question enough ballots were collected
        if (decryptionSets[ballot.questionID].size() >= key->threshold)
            continue;

        // check if the proof is valid
        if (!paillier_verify_decryption(key, ballot.answers))
            continue;

        // add proof to question's set
        decryptionSets[ballot.questionID].push_back(ballot.answers);
    }

    // verify that for each question enough proofs are available
    std::map<uint160, std::vector<paillier_partialdecryption_proof_t*>>::iterator iter;
    for (iter = decryptionSets.begin(); iter != decryptionSets.end(); iter++)
    {
        // check if enough ballots to tally
        if (iter->second.size() < key->threshold)
            return false;
    }

    // perform tallying
    for (iter = decryptionSets.begin(); iter != decryptionSets.end(); iter++)
    {
        // decrypt votes
        paillier_partialdecryption_proof_t** decryptions = &iter->second[0];
        paillier_plaintext_t* plain = paillier_combining(NULL, key, decryptions);

        Ballot b;
        b.questionID = iter->first;
        b.answer = mpz_get_ui(plain->m);

        paillier_freeplaintext(plain);

        // update results
        this->results[tallyHash].insert(b);
    }

    return true;
}

// ----------------------------------------------------------------

bool
ElectionManager::createTrusteeTally(TxTally* tally, paillier_partialkey_t* privateKey, TxTrusteeTally** tallyOut)
{
    // collect all ballots until last block
    std::set<EncryptedBallot> ballots = this->getAllVotes(tally->lastBlock);

    if (ballots.size() == 0)
        return false;

    paillier_pubkey_t* key = this->transaction->election->encPubKey;

    // compute combinations of respective questions
    std::map<uint160, paillier_ciphertext_pure_t*> combinations;
    BOOST_FOREACH(EncryptedBallot ballot, ballots)
    {
        // only consider valid votes, i.e. the encrypted plaintext
        // is element of a specified set of allowed plaintexts
        if (!paillier_verify_enc(key, ballot.answer))
            continue;

        // initialize if not set yet
        if (!combinations.count(ballot.questionID))
            combinations[ballot.questionID] = paillier_create_enc_zero();

        // combine two encrypted answers
        paillier_mul(key, combinations[ballot.questionID], combinations[ballot.questionID], ballot.answer);
    }

    // compute proof for each question
    std::set<TalliedBallots> tallies;
    std::map<uint160, paillier_ciphertext_pure_t*>::iterator iter;
    for(iter = combinations.begin(); iter != combinations.end(); iter++)
    {
        // create proof
        paillier_partialdecryption_proof_t* proof = paillier_dec_proof(key, privateKey, iter->second, paillier_get_rand_devurandom, NULL);

        // prepare tallied ballot
        TalliedBallots ballot;
        ballot.questionID = iter->first;
        ballot.answers = proof;
        tallies.insert(ballot);

        paillier_freeciphertext(iter->second);
    }

    // create transaction
    TxTrusteeTally* result = new TxTrusteeTally();
    result->tally = tally->getHash();
    result->partialDecryption = tallies;

    *tallyOut = result;

    return true;
}

// ----------------------------------------------------------------

std::set<EncryptedBallot>
ElectionManager::getAllVotes(uint256 lastBlock)
{
    std::set<EncryptedBallot> result;

    uint256 hash = this->transaction->getHash();

    // get block in which election was started
    Block *startBlock = NULL;
    if (BlockChainDB::getBlockByTransaction(hash, &startBlock) != BlockChainStatus::BC_OK)
        return result;

    // get all blocks until lastBlock
    std::vector<Block*> blocksFromChain;
    if (BlockChainDB::getAllBlocks(startBlock->getHash(), lastBlock, blocksFromChain) != BlockChainStatus::BC_OK)
        return result;

    // search for all relevant votes
    std::set<CKeyID> voters;
    for (int i = blocksFromChain.size() - 1; i >= 0; i--)
    {
        Block* currentBlock = blocksFromChain[i];

        BOOST_FOREACH(Transaction* txCurrent, currentBlock->transactions)
        {
            if (txCurrent->getType() != TxType::TX_VOTE)
                continue;

            TxVote *txVote = (TxVote*) txCurrent;

            // check if vote corresponds to election
            if (hash != txVote->election)
                continue;

            // skip old votes
            if (!voters.insert(txVote->getPublicKey().GetID()).second)
                continue;

            // insert all all ballots from this vote
            result.insert(txVote->ballots.begin(), txVote->ballots.end());
        }
    }

    return result;
}
