/*=============================================================================

This class represents a election with all its attributes.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_ELECTION_H
#define BITVOTING_ELECTION_H

#include "helper.h"
#include "bitcoin/key.h"
#include "bitcoin/uint256.h"
#include "paillier/comparison.h"
#include "paillier/paillier.h"
#include "paillier/serialization.h"

#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

#include <boost/serialization/access.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

// ----------------------------------------------------------------
// Answers for an election
#define DEFAULT_ANSWERS {"NO", "YES"}
typedef std::vector<std::string> Answers;

// ----------------------------------------------------------------
class Question
{
public:

    // Identifier for this question
    uint160 id;

    // Question represent as string
    std::string question;

    // Possible answers (only 2 allowed!)
    Answers answers;

    // ----------------------------------------------------------------

    Question():
        Question("") {}

    Question(std::string question, Answers answers = DEFAULT_ANSWERS):
        question(question)
    {
        if (answers.size() != 2)
            throw std::invalid_argument("Exactly two answers must be provided!");

        this->id = Helper::GenerateRandom160();
        this->answers = answers;
    }

    // ----------------------------------------------------------------

    inline bool operator<(const Question& other) const
    {
        return this->id < other.id;
    }

    inline bool operator==(const Question& other) const
    {
        return this->id == other.id;
    }

private:

    friend class boost::serialization::access;

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & this->id;
        a & this->question;
        a & this->answers;
    }
};

// ----------------------------------------------------------------
typedef struct Ballot_t
{
    // Reference to the question answered
    uint160 questionID = 0;

    // Holds the index of the answer (-1 means abstained)
    int answer = -1;

    // ----------------------------------------------------------------

    inline bool operator<(const Ballot_t& other) const
    {
        return std::tie(questionID, answer) <
                std::tie(other.questionID, other.answer);
    }

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & this->questionID;
        a & this->answer;
    }
} Ballot;

// ----------------------------------------------------------------
typedef struct TalliedBallots_t
{
    // Ballots regarding question ID
    uint160 questionID = 0;

    // Holds the partial decryption of all ballots
    paillier_partialdecryption_proof_t* answers = NULL;

    // ----------------------------------------------------------------

    inline bool operator==(const TalliedBallots_t& other) const
    {
        return (questionID == other.questionID &&
                *answers == *(other.answers));
    }

    inline bool operator<(const TalliedBallots_t& other) const
    {
        return std::tie(questionID, *answers) <
                std::tie(other.questionID, *(other.answers));
    }

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & this->questionID;
        a & this->answers;
    }
} TalliedBallots;

// ----------------------------------------------------------------
typedef struct EncryptedBallot_t
{
    // Question ID
    uint160 questionID = 0;

    // Encrypted answers
    paillier_ciphertext_proof_t* answer = NULL;

    // ----------------------------------------------------------------

    inline bool operator==(const EncryptedBallot_t& other) const
    {
        return (questionID == other.questionID &&
                *answer == *(other.answer));
    }

    inline bool operator<(const EncryptedBallot_t& other) const
    {
        return std::tie(questionID, *answer) <
                std::tie(other.questionID, *(other.answer));
    }

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & this->questionID;
        a & this->answer;
    }
} EncryptedBallot;

// ----------------------------------------------------------------
class Election
{
public:

    // Name of election
    std::string name;

    // Description of election
    std::string description;

    // Questions, the election is about
    std::vector<Question> questions;

    // Time when voting is allowed
    long int probableEndingTime = 0;

    // Encryption key for voting
    paillier_pubkey_t* encPubKey = NULL;

    // List of voters (identified by their keys), who can vote in this election
    std::set<CKeyID> voters;

    // List of trustees (identified by their keys), who are responsible for this election
    std::set<CKeyID> trustees;

    // ----------------------------------------------------------------

    Election() {}

    Election(std::vector<Question> questions, std::set<CKeyID> voters, std::set<CKeyID> trustees):
        questions(questions),
        voters(voters),
        trustees(trustees) {}

    // ----------------------------------------------------------------

    bool operator==(const Election& other) const
    {
        return (this->name == other.name &&
                    this->description == other.description &&
                    this->questions == other.questions &&
                    *(this->encPubKey) == *(other.encPubKey) &&
                    this->probableEndingTime == probableEndingTime &&
                    this->voters == other.voters &&
                    this->trustees == other.trustees);
    }

private:

    friend class boost::serialization::access;

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & this->name;
        a & this->description;
        a & this->questions;
        a & this->probableEndingTime;
        a & this->encPubKey;
        a & this->voters;
        a & this->trustees;
    }
};

#endif
