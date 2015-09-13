#include "test_paillier.h"

#include "helper.h"
#include "paillier/paillier.h"

#include <boost/foreach.hpp>

void test_pailler()
{
    Log::i("(Test) # Test: Paillier");

    // --- Parameters ---
    int bits = 256;
    int numOfTrustees = 4;
    int threshold = 3;


    // --- Key Generation ---

    paillier_pubkey_t* pub;
    paillier_partialkey_t** prv;
    paillier_keygen(bits, numOfTrustees, threshold, &pub, &prv, paillier_get_rand_devurandom);


    // --- Setup possible Plaintexts ---

    int originalPlaintext1 = 0;
    int originalPlaintext2 = 1;
    paillier_plaintext_t *pt1 = paillier_plaintext_from_ui(originalPlaintext1);
    paillier_plaintext_t *pt2 = paillier_plaintext_from_ui(originalPlaintext2);


    // --- Simulate Votes ---

    std::vector<PLAINTEXT_SELECTION> toBeEncrypted;
    toBeEncrypted.push_back(PLAINTEXT_SELECTION::FIRST);
    toBeEncrypted.push_back(PLAINTEXT_SELECTION::FIRST);
    toBeEncrypted.push_back(PLAINTEXT_SELECTION::SECOND);
    toBeEncrypted.push_back(PLAINTEXT_SELECTION::FIRST);
    toBeEncrypted.push_back(PLAINTEXT_SELECTION::SECOND);
    toBeEncrypted.push_back(PLAINTEXT_SELECTION::SECOND);
    toBeEncrypted.push_back(PLAINTEXT_SELECTION::SECOND);
    toBeEncrypted.push_back(PLAINTEXT_SELECTION::FIRST);


    // --- Encrypt Votes + ZKP ---

    // Create and store all ciphertexts
    std::vector<paillier_ciphertext_proof_t*> cipherTexts;
    BOOST_FOREACH(PLAINTEXT_SELECTION choice, toBeEncrypted)
    {
        // If ciphertexts are only 0 and 1, consider using the abbreviation method of paillier_enc_proof
        // Here we show the full set of parameters
        paillier_ciphertext_proof_t *c = paillier_enc_proof(pub, pt1, pt2, choice, paillier_get_rand_devurandom, NULL);
        cipherTexts.push_back(c);
    }


    // --- Use Homomrphic Property To Add Up ---

    paillier_ciphertext_pure_t *sum = paillier_create_enc_zero();
    int index = 0;
    // Only consider valid votes (able to verify proof)
    BOOST_FOREACH(paillier_ciphertext_proof_t *c, cipherTexts)
    {
        // If ciphertexts are only 0 and 1, consider using the abbreviation method of paillier_verfiy_enc
        // Here we show the full set of parameters
        if (paillier_verify_enc(pub, c, pt1, pt2))
        {
            // add up
            paillier_mul(pub, sum, sum, c);
        }
        else
        {
            Log::w("(Test) Verification failed at index: %d", index);
        }
        index++;
    }


    // --- Decrypt Partially ---

    // Create and store all partial decryptions
    std::vector<paillier_partialdecryption_proof_t*> partialDecryptions;
    for (int i = 0; i < pub->decryptServers; ++i)
    {
        paillier_partialdecryption_proof_t *partDec = paillier_dec_proof(pub, prv[i], sum, paillier_get_rand_devurandom, NULL);
        partialDecryptions.push_back(partDec);
    }


    // --- Combine Partial Decryptions To Extract Original Plaintext ---

    paillier_plaintext_t *result = paillier_combining(NULL, pub, &partialDecryptions[0]);


    // --- Assert ---

    // Calculate expected outcome
    int targetSum = 0;
    BOOST_FOREACH(PLAINTEXT_SELECTION choice, toBeEncrypted)
    {
        switch (choice) {
        case FIRST:
            targetSum += originalPlaintext1;
            break;
        case SECOND:
            targetSum += originalPlaintext2;
            break;
        default:
            Log::e("(Test) Unknown enum type: %d", choice);
        }
    }

    // Assert outcome
    mpz_t targetResult;
    mpz_init_set_ui(targetResult, targetSum);
    assert(mpz_cmp(result->m, targetResult) == 0);


    // --- Clean up ---

    mpz_clear(targetResult);
    paillier_freeplaintext(result);
    BOOST_FOREACH(paillier_partialdecryption_proof_t *decProof, partialDecryptions)
    {
        paillier_freepartdecryptionproof(decProof);
    }
    paillier_freeciphertext(sum);
    BOOST_FOREACH(paillier_ciphertext_proof_t *c, cipherTexts)
    {
        paillier_freeciphertextproof(c);
    }
    paillier_freeplaintext(pt1);
    paillier_freeplaintext(pt2);
    paillier_freepartkeysarray(prv, pub->decryptServers);
    paillier_freepubkey(pub);
}
