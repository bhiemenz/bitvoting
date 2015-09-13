#include "test_serialization.h"

#include <cassert>

// ----------------------------------------------------------------------------

#include "bitcoin/uint256.h"

void test_serialization_uints()
{
    Log::i("(Test) - uints");

    // Test uint160
    uint160 u1 = Helper::GenerateRandom160();
    serialize(u1);
    uint160 u1r;
    deserialize( u1r);

    assert(u1 == u1r);

    // Test uint160
    uint256 u2 = Helper::GenerateRandom256();
    serialize(u2);
    uint256 u2r;
    deserialize( u2r);

    assert(u2 == u2r);
}

// ----------------------------------------------------------------------------

#include "store.h"

void test_serialization_keys()
{
    Log::i("(Test) - Keys");

    SignKeyPair skp1;
    assert(SignKeyStore::genNewSignKeyPair(Role::KEY_VOTE, skp1));
    SignKeyStore::removeSignKeyPair(skp1.second.GetID()); // revert auto add to db

    serialize(&skp1);
    SignKeyPair skp2;
    deserialize(skp2);

    assert(skp1 == skp2);
}

// ----------------------------------------------------------------------------

#include "paillier/paillier.h"
#include "paillier/comparison.h"
#include "paillier/serialization.h"

void test_serialization_paillier()
{
    Log::i("(Test) - Paillier");

    // create random keys
    paillier_pubkey_t* publicKey1 = NULL;
    paillier_partialkey_t** privateKeys1 = NULL;
    const int n = 4;
    paillier_keygen(256, n, n, &publicKey1, &privateKeys1, paillier_get_rand_devurandom);

    // check publick key
    serialize(publicKey1);
    paillier_pubkey_t* publicKey2 = NULL;
    deserialize(&publicKey2);

    assert(*publicKey1 == *publicKey2);

    // check private keys
    for (int i = 0; i < n; i++)
    {
        serialize(privateKeys1[i]);
        paillier_partialkey_t* privateKey2 = NULL;
        deserialize(&privateKey2);

        assert(*(privateKeys1[i]) == *(privateKey2));

        paillier_freepartkey(privateKey2);
    }

    PLAINTEXT_SELECTION choice = PLAINTEXT_SELECTION::SECOND;
    paillier_ciphertext_proof_t* cipher1 = paillier_enc_proof(publicKey1, choice, paillier_get_rand_devurandom, NULL);

    serialize(*cipher1);
    paillier_ciphertext_proof_t* cipher2 = NULL;
    deserialize(&cipher2);

    assert(*cipher1 == *cipher2);

    // check partial decryption
    paillier_partialdecryption_proof_t* proof1 = paillier_dec_proof(publicKey1, privateKeys1[0], cipher1, paillier_get_rand_devurandom, NULL);

    serialize(proof1);
    paillier_partialdecryption_proof_t* proof2 = NULL;
    deserialize(&proof2);

    assert(*proof1 == *proof2);

    // free everything
    paillier_freepubkey(publicKey1);
    paillier_freepubkey(publicKey2);
    paillier_freepartkeysarray(privateKeys1, n);
    paillier_freeciphertext(cipher1);
    paillier_freeciphertext(cipher2);
    paillier_freepartdecryptionproof(proof1);
    paillier_freepartdecryptionproof(proof2);
}

// ============================================================================

void test_serialization()
{
    Log::i("(Test) # Test: Serialization");

    test_serialization_uints();
    test_serialization_keys();
    test_serialization_paillier();
}
