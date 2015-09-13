#include "test_comparison.h"

#include <set>
#include <vector>

#include "helper.h"
#include "paillier/paillier.h"
#include "election.h"
#include "store.h"

#include <boost/foreach.hpp>

template<typename T>
void test_generic(std::set<T>& data)
{
    std::vector<T> vdata;
    for (auto iter = data.begin(); iter != data.end(); iter++)
    {
        if (Helper::GenerateRandom() > 0.5)
            vdata.insert(vdata.begin(), *iter);
        else
            vdata.push_back(*iter);
    }

    std::set<T> sdata(vdata.begin(), vdata.end());
    assert(data == sdata);
}

void test_comparison()
{
    Log::i("(Test) # Test: Comparison");

    // questions
    Log::i("(Test) - Questions");
    std::set<Question> questions;
    for (int i = 0; i < Helper::GenerateRandom(10) + 2; i++)
    {
        Question q("Question #" + std::to_string(i));
        questions.insert(q);
    }

    test_generic(questions);

    // keys
    Log::i("(Test) - Keys");
    std::set<CKeyID> keys;
    for (int i = 0; i < Helper::GenerateRandom(10) + 2; i++)
    {
        SignKeyPair skp;
        SignKeyStore::genNewSignKeyPair(static_cast<Role>(Helper::GenerateRandom(4)), skp);
        keys.insert(skp.second.GetID());
        SignKeyStore::removeSignKeyPair(skp.second.GetID());
    }

    test_generic(keys);

    // encrypted ballots
    Log::i("(Test) - Encrypted Ballots");
    std::set<EncryptedBallot> eballots;
    std::set<TalliedBallots> tballots;
    int nTrustees = 4;
    paillier_pubkey_t* publicKey = NULL;
    paillier_partialkey_t** privateKeys = NULL;
    paillier_keygen(256, nTrustees, nTrustees, &publicKey, &privateKeys, paillier_get_rand_devurandom);

    for (int i = 0; i < Helper::GenerateRandom(2) + 2; i++)
    {
        PLAINTEXT_SELECTION choice = PLAINTEXT_SELECTION::FIRST;
        if (Helper::GenerateRandom() > 0.5)
            choice = PLAINTEXT_SELECTION::SECOND;
        paillier_ciphertext_proof_t* cipher = paillier_enc_proof(publicKey, choice, paillier_get_rand_devurandom, NULL);

        EncryptedBallot ballot;
        ballot.questionID = Helper::GenerateRandom160();
        ballot.answer = cipher;

        eballots.insert(ballot);

        int j = Helper::GenerateRandom(nTrustees - 1);
        paillier_partialdecryption_proof_t* proof = paillier_dec_proof(publicKey, privateKeys[j], cipher, paillier_get_rand_devurandom, NULL);

        TalliedBallots ballots;
        ballots.questionID = Helper::GenerateRandom160();
        ballots.answers = proof;

        tballots.insert(ballots);
    }

    test_generic(eballots);

    Log::i("(Test) - Tallied Ballots");

    test_generic(tballots);

    paillier_freepubkey(publicKey);
    paillier_freepartkeysarray(privateKeys, nTrustees);
    BOOST_FOREACH(EncryptedBallot b, eballots)
            paillier_freeciphertextproof(b.answer);
    BOOST_FOREACH(TalliedBallots b, tballots)
            paillier_freepartdecryptionproof(b.answers);
}
