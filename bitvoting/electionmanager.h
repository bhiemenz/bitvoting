/*=============================================================================

This class is for maintaining additional information about every election the
current client is involved in (creator, voter, trustee). This serves better and
more efficient access to detailed information without going through the
blockchain every time.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_ELECTIONMANAGER_H
#define BITVOTING_ELECTIONMANAGER_H

#include "election.h"
#include "transactions/election.h"
#include "transactions/vote.h"
#include "transactions/trustee_tally.h"
#include "transactions/tally.h"
#include "bitcoin/uint256.h"
#include "bitcoin/key.h"
#include "paillier/paillier.h"
#include "paillier/serialization.h"

#include <set>
#include <map>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/map.hpp>

// ==========================================================================

enum VotingResult
{
    OK,                 // ok
    INVALID_COUNT,      // invalid count of answers given
    DUPLICATE_QUESTION, // answered one question more than once
    UNKNOWN_QUESTION    // question is unknown to this election
};

// ==========================================================================

class TxVote;
class TxTally;
class TxElection;
class TxTrusteeTally;

class ElectionManager
{
public:
    // Holds the original election transaction (restored from Blockchain)
    TxElection* transaction = NULL;

    // These should be filled on reception of a new TxElection/TxVote/TxTally (en block)
    bool ended = false;

    // Register all voters that have already voted
    std::set<CKeyID> votesRegistered;

    // Register my votes (key + hash of vote transaction)
    std::map<CKeyID, uint256> myVotes;

    // Register all tallies (hash of tally transactions + hash of trustee tally transactions)
    std::map<uint256, std::set<uint256>> tallies;

    // Register all results (hash of tally transaction + computed results)
    std::map<uint256, std::set<Ballot>> results;

    // ----------------------------------------------------------------

    ElectionManager(TxElection* transaction = NULL):
        transaction(transaction) {}

    // ----------------------------------------------------------------

    // Check if the given key is eligible as a voter
    bool isVoterEligible(CPubKey) const;

    // Check if the given key is eligible as a trustee
    bool isTrusteeEligible(CPubKey) const;

    // Check if I have created the original election
    bool amICreator() const;

    // Check if I have an eligible voting key
    bool amIVoter() const;

    // Check if I have an eligible trustee key
    bool amITrustee() const;

    // Check if I am involved in the election
    bool amIInvolved() const;

    // Check if I have already voted
    bool alreadyVoted() const;

    // Check if there are results available
    bool resultsAvailable() const;

    // Obtain the original question to a given question ID
    bool getQuestion(uint160, Question&) const;

    // Create a vote given the given answers
    VotingResult createVote(std::set<Ballot>, TxVote**);

    // Perform the tallying for the given transaction
    bool tally(const uint256 &tallyHash);

    // Create a partial tally given the original tally transaction + corresponding key
    bool createTrusteeTally(TxTally*, paillier_partialkey_t*, TxTrusteeTally**);

    // ----------------------------------------------------------------

    inline bool operator<(/*const*/ ElectionManager& other) /*const*/
    {
        return this->transaction->getHash() < other.transaction->getHash();
    }

private:
    // Gather all votes until a given block
    std::set<EncryptedBallot> getAllVotes(uint256);

    // ----------------------------------------------------------------

    friend class boost::serialization::access;

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        // DO NOT SERIALIZE ELECTION, AS IT WILL BE RECOVERED BY THE DB
        a & this->ended;
        a & this->votesRegistered;
        a & this->myVotes;
        a & this->tallies;
        a & this->results;
    }
};

#endif // ELECTIONMANAGER_H
