/*=============================================================================

Provides operators for comparison of all paillier-dependent data structures.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef PAILLIER_COMPARISON_H
#define PAILLIER_COMPARISON_H

#include "paillier.h"

// ==========================================================================

bool mpz_equal(const mpz_t, const mpz_t);
bool mpz_less(const mpz_t, const mpz_t);

// --------------------------------------------------------------------------

bool operator==(const paillier_partialkey_t&,
                const paillier_partialkey_t&);

// --------------------------------------------------------------------------

bool operator==(const paillier_partialdecryption_proof_t&,
                const paillier_partialdecryption_proof_t&);
bool operator<(const paillier_partialdecryption_proof_t&,
               const paillier_partialdecryption_proof_t&);

// --------------------------------------------------------------------------

bool operator==(const paillier_ciphertext_proof_t&,
                const paillier_ciphertext_proof_t&);
bool operator<(const paillier_ciphertext_proof_t&,
               const paillier_ciphertext_proof_t&);

// --------------------------------------------------------------------------

bool operator==(const paillier_verificationkey_t&,
                const paillier_verificationkey_t&);

// --------------------------------------------------------------------------

bool operator==(const paillier_pubkey_t&,
                const paillier_pubkey_t&);

#endif
