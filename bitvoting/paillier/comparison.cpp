#include "comparison.h"

// ================================================================

bool mpz_equal(const mpz_t first, const mpz_t second)
{
    return (mpz_cmp(first, second) == 0);
}

// ----------------------------------------------------------------

bool mpz_less(const mpz_t first, const mpz_t second)
{
    return (mpz_cmp(first, second) < 0);
}

// ================================================================

bool operator==(const paillier_partialkey_t& first,
                const paillier_partialkey_t& second)
{
    return (first.id == second.id &&
            mpz_equal(first.s, second.s));
}

// ================================================================

bool operator==(const paillier_partialdecryption_proof_t& first,
                const paillier_partialdecryption_proof_t& second)
{
    return (first.id == second.id &&
            mpz_equal(first.decryption, second.decryption) &&
            mpz_equal(first.c4, second.c4) &&
            mpz_equal(first.ci2, second.ci2) &&
            mpz_equal(first.e, second.e) &&
            mpz_equal(first.z, second.z));
}

// ----------------------------------------------------------------

bool operator<(const paillier_partialdecryption_proof_t& first,
               const paillier_partialdecryption_proof_t& second)
{
    if (first.id < second.id) return true;
    if (second.id < first.id) return false;
    if (mpz_less(first.decryption, second.decryption)) return true;
    if (mpz_less(second.decryption, first.decryption)) return false;
    if (mpz_less(first.c4, second.c4)) return true;
    if (mpz_less(second.c4, first.c4)) return false;
    if (mpz_less(first.ci2, second.ci2)) return true;
    if (mpz_less(second.ci2, first.ci2)) return false;
    if (mpz_less(first.e, second.e)) return true;
    if (mpz_less(second.e, first.e)) return false;
    if (mpz_less(first.z, second.z)) return true;
    return false;
}

// ================================================================

bool operator==(const paillier_ciphertext_proof_t& first,
                const paillier_ciphertext_proof_t& second)
{
    return mpz_equal(first.c, second.c) &&
            mpz_equal(first.e, second.e) &&
            mpz_equal(first.e1, second.e1) &&
            mpz_equal(first.e2, second.e2) &&
            mpz_equal(first.v1, second.v1) &&
            mpz_equal(first.v2, second.v2);
}

// ----------------------------------------------------------------

bool operator<(const paillier_ciphertext_proof_t& first,
               const paillier_ciphertext_proof_t& second)
{
    if (mpz_less(first.c, second.c)) return true;
    if (mpz_less(second.c, first.c)) return false;
    if (mpz_less(first.e, second.e)) return true;
    if (mpz_less(second.e, first.e)) return false;
    if (mpz_less(first.e1, second.e1)) return true;
    if (mpz_less(second.e1, first.e1)) return false;
    if (mpz_less(first.e2, second.e2)) return true;
    if (mpz_less(second.e2, first.e2)) return false;
    if (mpz_less(first.v1, second.v1)) return true;
    if (mpz_less(second.v1, first.v1)) return false;
    if (mpz_less(first.v2, second.v2)) return true;
    return false;
}

// ================================================================

bool operator==(const paillier_verificationkey_t& first,
                const paillier_verificationkey_t& second)
{
    return (first.id == second.id &&
            mpz_equal(first.v, second.v));
}

// ================================================================

bool operator==(const paillier_pubkey_t& first,
                const paillier_pubkey_t& second)
{
    bool result = (first.bits == second.bits &&
            mpz_equal(first.combineSharesConstant, second.combineSharesConstant) &&
            first.decryptServers == second.decryptServers &&
            mpz_equal(first.delta, second.delta) &&
            mpz_equal(first.n, second.n) &&
            mpz_equal(first.n_plusone, second.n_plusone) &&
            mpz_equal(first.n_squared, second.n_squared) &&
            first.threshold == second.threshold &&
            mpz_equal(first.v, second.v));

    if (!result)
        return false;

    for (int i = 0; i < first.decryptServers; i++)
    {
        if (first.verificationKeys[i] == second.verificationKeys[i])
            continue;

        if (!(*(first.verificationKeys[i]) == *(second.verificationKeys[i])))
            return false;
    }

    return true;
}
