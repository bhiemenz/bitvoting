/*
    libpaillier - A library implementing the Paillier cryptosystem.

    Copyright (C) 2006 SRI International.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.
*/

#ifndef PAILLIER_H
#define PAILLIER_H

#include "bitcoin/uint256.h"

#include <gmp.h>
#include <utility>
#include <vector>
#include <boost/serialization/list.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/archive/text_oarchive.hpp>

/*
    IMPORTANT SECURITY NOTES:

    On paillier_keygen and strong primes:

  When selecting a modulus n = p q, we do not bother ensuring that p
    and q are strong primes. While this was at one point considered
    important (e.g., it is required by ANSI X9.31), modern factoring
    algorithms make any advantage of strong primes doubtful [1]. RSA
    Laboratories no longer recommends the practice [2].

  On memory handling:

  At no point is any special effort made to securely "shred" sensitive
  memory or prevent it from being paged out to disk. This means that
  it is important that functions dealing with private keys and
  plaintexts (e.g., paillier_keygen and paillier_enc) only be run on
  trusted machines. The resulting ciphertexts and public keys,
  however, may of course be handled in an untrusted manner.

  [1] Are "strong" primes needed for RSA? Ron Rivest and Robert
      Silverman. Cryptology ePrint Archive, Report 2001/007, 2001.

  [2] RSA Laboratories' Frequently Asked Questions About Today's
      Cryptography, Version 4.1, Section 3.1.4.
*/

/******
 TYPES
*******/

enum PLAINTEXT_SELECTION
{
    FIRST,
    SECOND
};

typedef struct
{
    mpz_t value;
} mpz_wrapper;

/*
    Verification key specific for decryption-server with id 'id'.
    Used for ZKP, that decryption-server indeed used it's secret key.
*/
typedef struct
{
    int id;
    mpz_t v;

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        char vChars[mpz_sizeinbase(v, 16)];
        mpz_get_str(vChars, 16, v);
        std::string vStr(vChars);

        if(version == 0)
        {
            ar & id;
            ar & vStr;
        }

    }

    template<class Archive>
    void load(Archive & ar, const unsigned int version)
    {
        std::string vStr;

        if(version == 0)
        {
            ar & id;
            ar & vStr;
        }

        const char* vChars = vStr.c_str();
        mpz_init_set_str(v, vChars, 16);
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
} paillier_verificationkey_t;

/*
    This represents a Paillier public key, which is basically just global properties,
    the modulus n and the verification value/keys.
    The other values are just stored to avoid recomputation.
*/
typedef struct
{
    int bits;  /* e.g., 1024 */
    int decryptServers; /* number of authorities */
    int threshold; /* number of authorities necessary for decryption */
    mpz_t n;   /* public modulus n = p q */
    mpz_t n_squared; /* cached to avoid recomputing */
    mpz_t n_plusone; /* cached to avoid recomputing */
    mpz_t delta; /* cached to avoid recomputing */
    mpz_t combineSharesConstant; /* cached to avoid recomputing */
    mpz_t v; /* generator of cyclic group of squares for ZKP */
    paillier_verificationkey_t** verificationKeys; /* one key per decryption-server for ZKP */

    // compute the 'other' values (the basic values must be set already)
    void complete()
    {
        // n^2
        mpz_mul(n_squared, n, n);
        // n+1
        mpz_add_ui(n_plusone, n, 1);
        //delta = l!
        mpz_fac_ui(delta, decryptServers);
        // combineSharesConstant = (4*delta^2)^(-1) mod n^s
        mpz_mul( combineSharesConstant, delta, delta);
        mpz_mul_ui( combineSharesConstant, combineSharesConstant, 4);
        int errorCode = mpz_invert( combineSharesConstant, combineSharesConstant, n);
        if (errorCode == 0)
        {
            throw std::runtime_error("Inverse of 4*delta^2 mod n^s not found!");
        }
    }

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        // see GMP docs for the +2
        char nChars[mpz_sizeinbase(n, 16)+2];
        mpz_get_str(nChars, 16, n);
        std::string nStr(nChars);

        char vChars[mpz_sizeinbase(v, 16)+2];
        mpz_get_str(vChars, 16, v);
        std::string vStr(vChars);

        if(version == 0)
        {
            ar & bits;
            ar & decryptServers;
            ar & threshold;
            ar & nStr;
            ar & vStr;
            for (int i = 0; i < decryptServers; ++i) {
                paillier_verificationkey_t temp = (*verificationKeys[i]);
                ar & temp;
            }
        }

    }

    template<class Archive>
    void load(Archive & ar, const unsigned int version)
    {
        std::string nStr;
        std::string vStr;
        mpz_init(n_squared);
        mpz_init(n_plusone);
        mpz_init(delta);
        mpz_init(combineSharesConstant);

        if(version == 0)
        {
            ar & bits;
            ar & decryptServers;
            ar & threshold;
            ar & nStr;
            ar & vStr;
            verificationKeys = (paillier_verificationkey_t**) malloc(sizeof(paillier_verificationkey_t) * decryptServers);
            for (int i = 0; i < decryptServers; ++i) {
                paillier_verificationkey_t temp;
                ar & temp;
                verificationKeys[i] = (paillier_verificationkey_t*) malloc(sizeof(paillier_verificationkey_t));
                verificationKeys[i]->id = temp.id;
                mpz_init_set(verificationKeys[i]->v, temp.v);
            }
        }

        const char* nChars = nStr.c_str();
        mpz_init_set_str(n, nChars, 16);

        const char* vChars = vStr.c_str();
        mpz_init_set_str(v, vChars, 16);

        complete();
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()

} paillier_pubkey_t;

/*
  Partial key for decryption-server with id 'id'.
*/
typedef struct
{
    int id;
    mpz_t s;

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        char sChars[mpz_sizeinbase(s, 16)];
        mpz_get_str(sChars, 16, s);
        std::string sStr(sChars);

        if(version == 0)
        {
            ar & id;
            ar & sStr;
        }

    }

    template<class Archive>
    void load(Archive & ar, const unsigned int version)
    {
        std::string sStr;

        if(version == 0)
        {
            ar & id;
            ar & sStr;
        }

        const char* sChars = sStr.c_str();
        mpz_init_set_str(s, sChars, 16);
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
} paillier_partialkey_t;

typedef std::pair<paillier_partialkey_t*, paillier_pubkey_t*> EncKeyPair;

/*
  This is a (semantic rather than structural) type for plaintexts.
  These can be converted to and from ASCII strings and byte arrays.
*/
typedef struct
{
    mpz_t m;
} paillier_plaintext_t;

/*
  This is a type for ciphertexts.
*/
typedef struct
{
    mpz_t c;

} paillier_ciphertext_pure_t;

/*
  This is a type for ciphertexts with
  Zero-Knowledge-Proof that the encrypted message
  is element of a specified set.
*/
typedef struct : paillier_ciphertext_pure_t
{
    mpz_t e;
    mpz_t e1;
    mpz_t v1;
    mpz_t e2;
    mpz_t v2;
} paillier_ciphertext_proof_t;

/*
  Partial Decryption of ciphertext related to decryption-server with
  id 'id' for threshold variant with ZKP.
*/
typedef struct
{
    int id;
    mpz_t decryption;
    mpz_t c4;
    mpz_t ci2;
    mpz_t e;
    mpz_t z;
} paillier_partialdecryption_proof_t;

/*
  Point of polynomial function (= evaluation of polynomial at X).
*/
typedef struct
{
    mpz_t p;
} paillier_polynomial_point_t;

/*
  This is the type of the callback functions used to obtain the
  randomness needed by the probabilistic algorithms. The functions
  paillier_get_rand_devrandom and paillier_get_rand_devurandom
  (documented later) may be passed to any library function requiring a
  paillier_get_rand_t, or you may implement your own. If you implement
  your own such function, it should fill in "len" random bytes in the
  array "buf".
*/
typedef void (*paillier_get_rand_t) ( void* buf, int len );

/*****************
 BASIC OPERATIONS
*****************/

/*
  Generate a keypair of length modulusbits using randomness from the
  provided get_rand function. Space will be allocated for each of the
  keys, and the given pointers will be set to point to the new
  paillier_pubkey_t and paillier_prvkey_t structures. The functions
  paillier_get_rand_devrandom and paillier_get_rand_devurandom may be
  passed as the final argument.
*/
void paillier_keygen( int modulusbits, int decryptServers, int thresholdServers,
                      paillier_pubkey_t** pub,
                      paillier_partialkey_t*** partKeys,
                      paillier_get_rand_t get_rand );

 /*
     Encrypt the given plaintext with the given public key using
     randomness from get_rand for blinding. If res is not null, its
     contents will be overwritten with the result. Otherwise, a new
     ciphertext will be allocated and returned.
 */
 paillier_ciphertext_pure_t *paillier_enc(paillier_ciphertext_pure_t *res,
                                      mpz_t r,
                                      paillier_pubkey_t* pub,
                                      paillier_plaintext_t* pt,
                                      gmp_randstate_t &rand , const char *r_hex);

 /*
     Encrypt 0 or 1 based on the given choice with the given public key using
     randomness from get_rand for blinding and return the result
     together with a zero-knowledge-proof, that the encrypted
     plaintext was indeed 0 or 1.
 */
 paillier_ciphertext_proof_t *paillier_enc_proof(paillier_pubkey_t *pub,
                                           PLAINTEXT_SELECTION choice,
                                           paillier_get_rand_t get_rand,
                                           const char *r_hex);

 /*
     Encrypt the given plaintext with the given public key using
     randomness from get_rand for blinding and return the result
     together with a zero-knowledge-proof, that the encrypted
     plaintext was indeed one of the possibleMessages.
 */
 paillier_ciphertext_proof_t *paillier_enc_proof(paillier_pubkey_t *pub,
                                           paillier_plaintext_t *pt,
                                           paillier_plaintext_t *pt2,
                                           PLAINTEXT_SELECTION index,
                                           paillier_get_rand_t get_rand,
                                           const char *r_hex);



 /*
     Verifies the ZKP that the encrypter encrpyted 0 or 1.
 */
 bool paillier_verify_enc(paillier_pubkey_t *pub,
                          paillier_ciphertext_proof_t *encrProof);

 /*
     Verifies the ZKP that the encrypter encrpyted one of the possibleMessages.
 */
 bool paillier_verify_enc(paillier_pubkey_t *pub,
                          paillier_ciphertext_proof_t *encrProof,
                          paillier_plaintext_t *pt1,
                          paillier_plaintext_t *pt2);

 /*
     Decrypt the given ciphertext with the given key pair. If res is not
     null, its contents will be overwritten with the result. Otherwise, a
     new paillier_plaintext_t will be allocated and returned.
 */
 paillier_partialdecryption_proof_t* paillier_dec(paillier_partialdecryption_proof_t *res,
                                             paillier_pubkey_t* pub,
                                             paillier_partialkey_t* prv,
                                             paillier_ciphertext_pure_t *ct );

 /*
     Decrypt the given ciphertext using paillier_dec with the given key pair and
     creates a ZKP that the decryption-server used it's private key to create the
     partial decryption.
 */
 paillier_partialdecryption_proof_t* paillier_dec_proof( paillier_pubkey_t* pub,
                     paillier_partialkey_t* prv,
                     paillier_ciphertext_pure_t* ct,
                     paillier_get_rand_t get_rand,
                     const char* r_hex );

 /*
     Verifies the ZKP that the decryption-server used it's private key to create
     the partial decryption.
 */
 bool paillier_verify_decryption( paillier_pubkey_t* pub,
                             paillier_partialdecryption_proof_t* dec_proof );

 /*
     Combines (at least) threshold partial decryptions. The result is the
     original plaintext.
 */
 paillier_plaintext_t*
 paillier_combining( paillier_plaintext_t* res,
                             paillier_pubkey_t* pub,
                             paillier_partialdecryption_proof_t** partDecr);

/*****************************
 USE OF ADDITIVE HOMOMORPHISM
*****************************/

/*
    Multiply the two ciphertexts assuming the modulus in the given
    public key and store the result in the contents of res, which is
    assumed to have already been allocated.
*/
void paillier_mul(paillier_pubkey_t* pub,
                                     paillier_ciphertext_pure_t *res,
                                     paillier_ciphertext_pure_t *ct0,
                                     paillier_ciphertext_pure_t *ct1 );

/*
  Raise the given ciphertext to power pt and store the result in res,
  which is assumed to be already allocated. If ct is an encryption of
  x, then res will become an encryption of x * pt mod n, where n is
  the modulus in pub.
*/
void paillier_exp(paillier_pubkey_t* pub,
                                     paillier_ciphertext_pure_t *res,
                                     paillier_ciphertext_pure_t *ct,
                                     paillier_plaintext_t* pt );

/****************************
 PLAINTEXT IMPORT AND EXPORT
****************************/

/*
  Allocate and initialize a paillier_plaintext_t from an unsigned long
  integer, an array of bytes, or a null terminated string.
*/
paillier_plaintext_t* paillier_plaintext_from_ui( unsigned long int x );
paillier_plaintext_t* paillier_plaintext_from_bytes( void* m, int len );
paillier_plaintext_t* paillier_plaintext_from_str( char* str );

/*
    Export a paillier_plaintext_t as a null terminated string or an
    array of bytes. In either case the result is allocated for the
    caller and the original paillier_plaintext_t is unchanged.
*/
char* paillier_plaintext_to_str( paillier_plaintext_t* pt );
void* paillier_plaintext_to_bytes( int len, paillier_plaintext_t* pt );

/*****************************
 CIPHERTEXT IMPORT AND EXPORT
*****************************/

/*
    Import or export a paillier_ciphertext_t from or to an array of
    bytes. These behave like the corresponding functions for
    paillier_plaintext_t's.
*/
paillier_ciphertext_pure_t* paillier_ciphertext_from_bytes( void* c, int len );
void* paillier_ciphertext_to_bytes(int len, paillier_ciphertext_pure_t *ct );

/********
 CLEANUP
********/

/*
  These free the structures allocated and returned by various
  functions within library and should be used when the structures are
  no longer needed.
*/
void paillier_freepubkey( paillier_pubkey_t* pub );
void paillier_freepartkey( paillier_partialkey_t* prv );
void paillier_freepartkeysarray( paillier_partialkey_t** prv, int length );
void paillier_freeplaintext( paillier_plaintext_t* pt );
void paillier_freeciphertext(paillier_ciphertext_pure_t *ct );
void paillier_freeciphertextproof(paillier_ciphertext_proof_t *ct );
void paillier_freeverificationkey( paillier_verificationkey_t* v );
void paillier_freepartdecryptionproof( paillier_partialdecryption_proof_t* pdp );
void paillier_freepolynomialpoint( paillier_polynomial_point_t* p );

/***********
 MISC STUFF
***********/

/*
    These functions may be passed to the paillier_keygen and
    paillier_enc functions to provide a source of random numbers. The
    first reads bytes from /dev/random. On Linux, this device
    exclusively returns entropy gathered from environmental noise and
    therefore frequently blocks when not enough is available. The second
    returns bytes from /dev/urandom. On Linux, this device also returns
    environmental noise, but augments it with a pseudo-random number
    generator when not enough is available. The latter is probably the
    better choice unless you have a specific reason to believe it is
    insufficient.
*/
void paillier_get_rand_devrandom(  void* buf, int len );
void paillier_get_rand_devurandom( void* buf, int len );

uint256 hashMultiple(std::vector<__mpz_struct> &in);


/*
    This function just allocates and returns a paillier_ciphertext_t
    which is a valid, _unblinded_ encryption of zero (which may actually
    be done without knowledge of a public key). Note that this
    encryption is UNBLINDED, so don't use it unless you want anyone who
    sees it to know it is an encryption of zero. This function is
    sometimes handy to get some homomorphic computations started or
    quickly allocate a paillier_ciphertext_t in which to place some
    later result.
*/
paillier_ciphertext_pure_t *paillier_create_enc_zero();

/*
    Just a utility used internally when we need round a number of bits
    up the number of bytes necessary to hold them.
*/
#define PAILLIER_BITS_TO_BYTES(n) ((n) % 8 ? (n) / 8 + 1 : (n) / 8)

#endif
