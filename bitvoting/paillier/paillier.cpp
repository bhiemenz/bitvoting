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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sstream>
#include "paillier.h"
#include "bitcoin/allocators.h"
#include "bitcoin/hash.h"

#include <boost/foreach.hpp>

void
init_rand( gmp_randstate_t rand, paillier_get_rand_t get_rand, int bytes )
{
    void* buf;
    mpz_t s;

    buf = malloc(bytes);
    get_rand(buf, bytes);

    gmp_randinit_default(rand);
    mpz_init(s);
    mpz_import(s, bytes, 1, 1, 0, 0, buf);
    gmp_randseed(rand, s);
    mpz_clear(s);

    free(buf);
}

void
genSafePrimes( mpz_t* p1, mpz_t* p, int modulusbits, gmp_randstate_t rand )
{
    mpz_init(*p1);
    mpz_init(*p);
    do
    {
        do
            mpz_urandomb(*p1, rand, (modulusbits / 2) - 1);
        while( !mpz_probab_prime_p(*p1, 10) );
        mpz_mul_ui(*p,*p1,2);
        mpz_add_ui(*p,*p,1);
    }
    while( !mpz_probab_prime_p(*p, 10) );
}



paillier_polynomial_point_t*
evaluatePolynomial(mpz_t* a, int aLength, int X, mpz_t nm)
{
    paillier_polynomial_point_t* sum = (paillier_polynomial_point_t*) malloc(sizeof(paillier_polynomial_point_t));
    mpz_t subterm;
    mpz_init(sum->p);
    mpz_init(subterm);

    mpz_set_ui(sum->p, 0);
    // sum += a_i * X^i
    for (int i = 0; i < aLength; ++i) {
        mpz_mul_ui(subterm, a[i], pow(X,i));
        mpz_add(sum->p, sum->p, subterm);
    }
    mpz_mod(sum->p, sum->p, nm);

    mpz_clear(subterm);

    return sum;
}

void
paillier_keygen( int modulusbits, int decryptServers, int thresholdServers,
                                 paillier_pubkey_t** pub,
                                 paillier_partialkey_t*** partKeys,
                                 paillier_get_rand_t get_rand )
{
    mpz_t p1;
    mpz_t q1;
    mpz_t p;
    mpz_t q;
    mpz_t m;
    mpz_t nm;
    mpz_t nSquare;
    mpz_t mInversToN;
    mpz_t d;
    mpz_t r;
    mpz_t gcdRN;
    mpz_t vExp;
    mpz_t * a;
    mpz_t * shares;
    mpz_t * viarray;
    gmp_randstate_t rand;

    /* allocate the new key structures */

    *pub = (paillier_pubkey_t*) malloc(sizeof(paillier_pubkey_t));
    *partKeys = (paillier_partialkey_t**) malloc(sizeof(paillier_partialkey_t*) * decryptServers);

    /* initialize our integers */

    mpz_init((*pub)->n);
    mpz_init((*pub)->n_squared);
    mpz_init((*pub)->n_plusone);
    mpz_init((*pub)->delta);
    mpz_init((*pub)->combineSharesConstant);
    mpz_init((*pub)->v);
    mpz_init(m);
    mpz_init(nm);
    mpz_init(nSquare);
    mpz_init(mInversToN);
    mpz_init(d);
    mpz_init(r);
    mpz_init(gcdRN);
    mpz_init(vExp);


    /* pick random (modulusbits/2)-bit primes p and q */

    init_rand(rand, get_rand, modulusbits / 8 + 1);
    do
    {
        genSafePrimes(&p1,&p,modulusbits,rand);

        do
            genSafePrimes(&q1,&q,modulusbits,rand);
        while( mpz_cmp(p,q)==0 || mpz_cmp(p,q1)==0 || mpz_cmp(q,p1)==0 || mpz_cmp(q1,p1)==0 );

        /* compute the public modulus n = p q */

        mpz_mul((*pub)->n, p, q);
        mpz_mul(m, p1, q1);
    } while( !mpz_tstbit((*pub)->n, modulusbits - 1) );
    (*pub)->bits = modulusbits;
    (*pub)->decryptServers = decryptServers;
    (*pub)->threshold = thresholdServers;
    (*pub)->complete();


    // We decide on some s>0, thus the plaintext space will be Zn^s
    // for now we set s=1
    mpz_mul(nm, (*pub)->n, m);
    mpz_mul(nSquare, (*pub)->n, (*pub)->n);

    // next d need to be chosen such that
    // d=0 mod m and d=1 mod n, using Chinese remainder thm
    // we can find d using Chinese remainder thm
    // note that $d=(m. (m^-1 mod n))$
    int errorCode = mpz_invert(mInversToN,m,(*pub)->n);
    if (errorCode == 0)
    {
        throw std::runtime_error("Inverse of m mod n not found!");
    }
    mpz_mul(d,m,mInversToN);


    //a[0] is equal to d
    //a[i] is the random number used for generating the polynomial
    //between 0... n^s*m-1, for 0 < i < thresholdServers
    a = (mpz_t*) malloc(sizeof(mpz_t) * thresholdServers);
    mpz_init(a[0]);
    mpz_set(a[0], d);
    for (int i = 1; i < thresholdServers; ++i) {
        mpz_init(a[i]);
        mpz_urandomm(a[i], rand, nm);
    }


    //We need to generate v
    //Although v needs to be the generator of the squares in Z^*_{n^2}
    //I will use a heuristic which gives a generator with high prob.
    //get a random element r such that gcd(r,nSquare) is one
    //set v=r*r mod nSquare. This heuristic is used in the Victor Shoup
    //threshold signature paper.
    do
    {
        mpz_urandomb(r, rand, 4*modulusbits);
        mpz_gcd(gcdRN, r, (*pub)->n);
    } while( !mpz_cmp_ui(gcdRN, 1)==0 );
    // we can now set v to r*r mod nSquare
    mpz_powm_ui((*pub)->v, r, 2, nSquare);

    //This array holds the resulting keys
    shares = (mpz_t*) malloc(sizeof(mpz_t) * decryptServers);
    viarray = (mpz_t*) malloc(sizeof(mpz_t) * decryptServers);


    for (int i = 0; i < decryptServers; ++i) {
        mpz_init(shares[i]);
        mpz_init(viarray[i]);

        // The secret share of the i'th authority will be s_i=f(i) for 1<=i<=l
        paillier_polynomial_point_t *polynPoint = evaluatePolynomial(a, thresholdServers, i+1, nm);
        mpz_set(shares[i], polynPoint->p);
        paillier_freepolynomialpoint(polynPoint);
        //for each decryption server a verication key v_i=v^(delta*s_i) mod n^(s+1)
        mpz_mul(vExp, (*pub)->delta, shares[i]);
        mpz_powm(viarray[i], (*pub)->v, vExp, nSquare);
    }


    /* Set key structures */

    (*pub)->verificationKeys = (paillier_verificationkey_t**) malloc(sizeof(paillier_verificationkey_t) * decryptServers);
    for (int i = 0; i < decryptServers; ++i) {
        int id = i+1;

        paillier_verificationkey_t* verKey = (paillier_verificationkey_t*) malloc(sizeof(paillier_verificationkey_t));
        mpz_init(verKey->v);
        verKey->id=id;
        mpz_set(verKey->v, viarray[i]);
        (*pub)->verificationKeys[i]=verKey;

        paillier_partialkey_t* share = (paillier_partialkey_t*) malloc(sizeof(paillier_partialkey_t));
        mpz_init(share->s);
        share->id=id;
        mpz_set(share->s, shares[i]);
        (*partKeys)[i]=share;
    }


    /* clear temporary integers and randstate */

    mpz_clear(p);
    mpz_clear(q);
    mpz_clear(p1);
    mpz_clear(q1);
    mpz_clear(m);
    mpz_clear(nm);
    mpz_clear(nSquare);
    mpz_clear(mInversToN);
    mpz_clear(d);
    mpz_clear(r);
    mpz_clear(gcdRN);
    mpz_clear(vExp);
    gmp_randclear(rand);
    for (int i = 0; i < thresholdServers; ++i) {
        mpz_clear(a[i]);
    }
    free(a);
    for (int i = 0; i < decryptServers; ++i) {
        mpz_clear(shares[i]);
        mpz_clear(viarray[i]);
    }
    free(shares);
    free(viarray);
}

// hashMultiple hashes multiple mpz_t values by
// concatenating their hex representation
// remember: mpz_t[0] == __mpz_struct
uint256
hashMultiple(std::vector<__mpz_struct> &in)
{
    uint256 out;

    /* get strings */
    std::vector<char*> chars;
    int newSize = 0;
    BOOST_FOREACH(__mpz_struct val, in)
    {
        mpz_t value = { val };
        char *str = mpz_get_str(NULL, 16, value);
        chars.push_back(str);
        newSize += strlen(str);
    }

    /* concat */
    // Determine new size (+1 because of null-terminator)
    newSize += 1;
    // Allocate new buffer
    char* concat = (char *)malloc(newSize);
    // do the copy and concat
    strcpy(concat, "");
    BOOST_FOREACH(char* c, chars)
    {
        strncat(concat, c, strlen(c));
    }

    /* build hash */
    out = Hash(concat, concat + newSize);

    /* free temporary variables */
    BOOST_FOREACH(char* c, chars)
    {
        free(c);
    }

    return out;
}

// hash4 calculates the hash of 4 given mpz_t values
uint256
hash4(mpz_t a, mpz_t b, mpz_t c, mpz_t d)
{
    std::vector<__mpz_struct> vec;
    vec.push_back(a[0]);
    vec.push_back(b[0]);
    vec.push_back(c[0]);
    vec.push_back(d[0]);
    return hashMultiple(vec);
}

paillier_ciphertext_pure_t*
paillier_enc( paillier_ciphertext_pure_t* res,
              mpz_t r,
              paillier_pubkey_t* pub,
              paillier_plaintext_t* pt,
              gmp_randstate_t &rand,
              const char* r_hex)
{
    mpz_t x;

    /* pick random blinding factor */
    if( r_hex )
    {
        mpz_set_str(r, r_hex, 16);
        assert(mpz_cmp(r, pub->n) < 0);
    } else {
        do
            mpz_urandomb(r, rand, pub->bits);
        while( mpz_cmp(r, pub->n) >= 0 );
    }

    /* compute ciphertext */

    if( !res )
    {
        res = (paillier_ciphertext_pure_t*) malloc(sizeof(paillier_ciphertext_pure_t));
        mpz_init(res->c);
    }
    mpz_init(x);
    mpz_powm(res->c, pub->n_plusone, pt->m, pub->n_squared);
    mpz_powm(x, r, pub->n, pub->n_squared);

    mpz_mul(res->c, res->c, x);
    mpz_mod(res->c, res->c, pub->n_squared);

    mpz_clear(x);

    return res;
}

paillier_ciphertext_proof_t *paillier_enc_proof(paillier_pubkey_t *pub,
                                          PLAINTEXT_SELECTION choice,
                                          paillier_get_rand_t get_rand,
                                          const char *r_hex)
{
    paillier_plaintext_t *pt1 = paillier_plaintext_from_ui(0);
    paillier_plaintext_t *pt2 = paillier_plaintext_from_ui(1);
    paillier_ciphertext_proof_t *out = paillier_enc_proof(pub,
                                                         pt1,
                                                         pt2,
                                                         choice,
                                                         get_rand,
                                                         r_hex);
    paillier_freeplaintext(pt1);
    paillier_freeplaintext(pt2);
    return out;
}

paillier_ciphertext_proof_t *paillier_enc_proof(paillier_pubkey_t *pub,
                                          paillier_plaintext_t *pt,
                                          paillier_plaintext_t *pt2,
                                          PLAINTEXT_SELECTION index,
                                          paillier_get_rand_t get_rand,
                                          const char *r_hex)
{
    // --- Init ---
    mpz_t m1;
    mpz_t m2;
    mpz_t r;
    mpz_t rho;
    mpz_t u1;
    mpz_t u2;
    mpz_t gPower;
    mpz_t cPower;
    mpz_t e1NoMod;
    mpz_t rPower;
    gmp_randstate_t rand;

    mpz_init_set(m1, pt->m);
    mpz_init_set(m2, pt2->m);
    mpz_init(r);
    mpz_init(rho);
    mpz_init(u1);
    mpz_init(u2);
    mpz_init(gPower);
    mpz_init(cPower);
    mpz_init(e1NoMod);
    mpz_init(rPower);
    init_rand(rand, get_rand, pub->bits / 8 + 1);

    paillier_plaintext_t *chosenPlaintext = pt;
    // This method assumes that everything with index 1 corresponds to the plaintext
    // to be encrpyted.
    // If index==second the pt2 is the chosen plaintext and pt the "other" plaintext,
    // so we have to swap the indices!
    if (index == PLAINTEXT_SELECTION::SECOND)
    {
        chosenPlaintext = pt2;
        mpz_swap(m1, m2);
    }

    // Create result
    paillier_ciphertext_proof_t *encrProof;
    encrProof = (paillier_ciphertext_proof_t*) malloc(sizeof(paillier_ciphertext_proof_t));

    mpz_init(encrProof->c);
    mpz_init(encrProof->e);
    mpz_init(encrProof->e1);
    mpz_init(encrProof->v1);
    mpz_init(encrProof->e2);
    mpz_init(encrProof->v2);

    // Get encryption
    paillier_enc(encrProof, r, pub, chosenPlaintext, rand, r_hex);

    // pick random rho in Z*_n
    do
        mpz_urandomb(rho, rand, pub->bits);
    while( mpz_cmp(rho, pub->n) >= 0 );

    // pick random e2 in Z_n
    do
        mpz_urandomb(encrProof->e2, rand, pub->bits);
    while( mpz_cmp(encrProof->e2, pub->n) >= 0 );

    // pick random v2 in Z*_n
    do
        mpz_urandomb(encrProof->v2, rand, pub->bits);
    while( mpz_cmp(encrProof->v2, pub->n) >= 0 );

    // compute u2 = v2^n * (g^m2 / c)^e2 mod n^2 = v2^n * g^(m2*e2) * c^(-e2) mod n^2
    mpz_powm(u2, encrProof->v2, pub->n, pub->n_squared);
    mpz_mul(gPower, m2, encrProof->e2);
    mpz_powm(gPower, pub->n_plusone, gPower, pub->n_squared);
    mpz_ui_sub(cPower, 0, encrProof->e2);
    mpz_powm(cPower, encrProof->c, cPower, pub->n_squared);
    mpz_mul(u2, u2, gPower);
    mpz_mul(u2, u2, cPower);
    mpz_mod(u2, u2, pub->n_squared);

    // compute u1 = rho^n mod n^2
    mpz_powm(u1, rho, pub->n, pub->n_squared);

    // Commit to u1,u2 by hash: s = H(u1,u2,c,m1,m2)
    std::vector<__mpz_struct> vec;
    if (index == PLAINTEXT_SELECTION::SECOND)
    {
        vec.push_back(u2[0]);
        vec.push_back(u1[0]);
    } else {
        vec.push_back(u1[0]);
        vec.push_back(u2[0]);
    }
    vec.push_back(encrProof->c[0]);
    vec.push_back(pt->m[0]);
    vec.push_back(pt2->m[0]);
    uint256 hash = hashMultiple(vec);
    mpz_set_str(encrProof->e, hash.GetHex().c_str(), 16);

    // e1NoMod = e - e2
    mpz_sub(e1NoMod, encrProof->e, encrProof->e2);
    // e1 = e1NoMod mod n
    mpz_mod(encrProof->e1, e1NoMod, pub->n);

    // v1 = rho * r^e1 * g^(e1NoMod / n) mod n
    mpz_tdiv_q(encrProof->v1, e1NoMod, pub->n);
    mpz_powm(encrProof->v1, pub->n_plusone, encrProof->v1, pub->n);
    mpz_powm(rPower, r, encrProof->e1, pub->n);
    mpz_mul(encrProof->v1, encrProof->v1, rPower);
    mpz_mul(encrProof->v1, encrProof->v1, rho);
    mpz_mod(encrProof->v1, encrProof->v1, pub->n);


    // --- Finish ---

    // Clear temporaray variables
    mpz_clear(r);
    mpz_clear(rho);
    mpz_clear(u1);
    mpz_clear(u2);
    mpz_clear(gPower);
    mpz_clear(cPower);
    mpz_clear(e1NoMod);
    mpz_clear(rPower);

    // Publish (c, s, e1, v1, e2, v2)
    if (index == PLAINTEXT_SELECTION::SECOND)
    {
        mpz_swap(encrProof->e1, encrProof->e2);
        mpz_swap(encrProof->v1, encrProof->v2);
    }
    return encrProof;
}

bool paillier_verify_enc(paillier_pubkey_t *pub,
                         paillier_ciphertext_proof_t *encrProof)
{
    paillier_plaintext_t *pt1 = paillier_plaintext_from_ui(0);
    paillier_plaintext_t *pt2 = paillier_plaintext_from_ui(1);
    bool out = paillier_verify_enc(pub,
                               encrProof,
                               pt1,
                               pt2);
    paillier_freeplaintext(pt1);
    paillier_freeplaintext(pt2);
    return out;
}

bool paillier_verify_enc(paillier_pubkey_t *pub,
                         paillier_ciphertext_proof_t *encrProof,
                         paillier_plaintext_t* pt1,
                         paillier_plaintext_t* pt2)
{
    // --- Init ---

    mpz_t u1;
    mpz_t gPower1;
    mpz_t cPower1;
    mpz_t u2;
    mpz_t gPower2;
    mpz_t cPower2;
    mpz_t e;
    mpz_t temp;

    mpz_init(u1);
    mpz_init(gPower1);
    mpz_init(cPower1);
    mpz_init(u2);
    mpz_init(gPower2);
    mpz_init(cPower2);
    mpz_init(e);
    mpz_init(temp);

    // --- Pre-compute ---

    // compute u1 = v1^n * (g^pt1 / c)^e1 mod n^2 = v1^n * g^(pt1*e1) * c^(-e1) mod n^2
    mpz_powm(u1, encrProof->v1, pub->n, pub->n_squared);
    mpz_mul(gPower1, pt1->m, encrProof->e1);
    mpz_powm(gPower1, pub->n_plusone, gPower1, pub->n_squared);
    mpz_ui_sub(cPower1, 0, encrProof->e1);
    mpz_powm(cPower1, encrProof->c, cPower1, pub->n_squared);
    mpz_mul(u1, u1, gPower1);
    mpz_mul(u1, u1, cPower1);
    mpz_mod(u1, u1, pub->n_squared);

    // compute u2 = v2^n * (g^pt2 / c)^e2 mod n^2 = v2^n * g^(pt2*e2) * c^(-e2) mod n^2
    mpz_powm(u2, encrProof->v2, pub->n, pub->n_squared);
    mpz_mul(gPower2, pt2->m, encrProof->e2);
    mpz_powm(gPower2, pub->n_plusone, gPower2, pub->n_squared);
    mpz_ui_sub(cPower2, 0, encrProof->e2);
    mpz_powm(cPower2, encrProof->c, cPower2, pub->n_squared);
    mpz_mul(u2, u2, gPower2);
    mpz_mul(u2, u2, cPower2);
    mpz_mod(u2, u2, pub->n_squared);

    // Rebuild hash: s = H(u1,u2,c,pt,pt2)
    std::vector<__mpz_struct> vec;
    vec.push_back(u1[0]);
    vec.push_back(u2[0]);
    vec.push_back(encrProof->c[0]);
    vec.push_back(pt1->m[0]);
    vec.push_back(pt2->m[0]);
    uint256 hash = hashMultiple(vec);
    mpz_set_str(e, hash.GetHex().c_str(), 16);


    // --- Verify ---

    bool result = true;

    // Verify hash
    result &= mpz_cmp(e, encrProof->e) == 0;

    // Verify s
    mpz_add(temp, encrProof->e1, encrProof->e2);
    mpz_mod(temp, temp, pub->n);
    mpz_mod(e, e, pub->n);
    result &= mpz_cmp(e, temp) == 0;


    // --- Clear temporary variables ---

    mpz_clear(u1);
    mpz_clear(gPower1);
    mpz_clear(cPower1);
    mpz_clear(u2);
    mpz_clear(gPower2);
    mpz_clear(cPower2);
    mpz_clear(e);
    mpz_clear(temp);

    return result;
}

paillier_partialdecryption_proof_t*
paillier_dec( paillier_partialdecryption_proof_t* res,
                            paillier_pubkey_t* pub,
                            paillier_partialkey_t* prv,
                            paillier_ciphertext_pure_t* ct )
{
    mpz_t exp;

    /* initialize our integers */

    mpz_init(exp);
    if( !res )
    {
        res = (paillier_partialdecryption_proof_t*) malloc(sizeof(paillier_partialdecryption_proof_t));
        mpz_init(res->decryption);
    }


    res->id=prv->id;

    /* compute c_i=c^(2*delta*s_i) */

    mpz_mul(exp, pub->delta, prv->s);
    mpz_mul_ui(exp, exp, 2);
    mpz_powm(res->decryption, ct->c, exp, pub->n_squared);


    /* clear temporary integers */

    mpz_clear(exp);

    return res;
}

paillier_partialdecryption_proof_t*
paillier_dec_proof( paillier_pubkey_t* pub,
                    paillier_partialkey_t* prv,
                    paillier_ciphertext_pure_t* ct,
                    paillier_get_rand_t get_rand,
                    const char* r_hex )
{
    mpz_t r;
    mpz_t a;
    mpz_t b;
    gmp_randstate_t rand;

    paillier_partialdecryption_proof_t* partDecrProof;
    partDecrProof = (paillier_partialdecryption_proof_t*) malloc(sizeof(paillier_partialdecryption_proof_t));

    if ( r_hex )
        mpz_init_set_str(r, r_hex, 16);
    else {
        mpz_init(r);
        init_rand(rand, get_rand, pub->bits / 8 + 1);

        int hashLength = 256;
        mpz_urandomb(r, rand, 3*pub->bits + hashLength);
    }
    mpz_init(a);
    mpz_init(b);
    mpz_init(partDecrProof->c4);
    mpz_init(partDecrProof->ci2);
    mpz_init(partDecrProof->e);
    mpz_init(partDecrProof->z);
    mpz_init(partDecrProof->decryption);

    // c4 = c^4 mod n^(s+1)
    mpz_powm_ui(partDecrProof->c4, ct->c, 4, pub->n_squared);
    // a = c^4r mod n^(s+1)
    mpz_powm(a, partDecrProof->c4, r, pub->n_squared);
    // b = v^r mod n^(s+1)
    mpz_powm(b, pub->v, r, pub->n_squared);

    // partial-decrypt ciphertext (ci = c^(2*Delta*si))
    paillier_dec(partDecrProof, pub, prv, ct);
    // ci^2 mod n^(s+1)
    mpz_powm_ui(partDecrProof->ci2, partDecrProof->decryption, 2, pub->n_squared);

    // hash: H(a,b,c4,ci2)
    uint256 hash = hash4(a, b, partDecrProof->c4, partDecrProof->ci2);
    mpz_set_str(partDecrProof->e, hash.GetHex().c_str(), 16);
    // z = r + e*si*delta
    mpz_mul(partDecrProof->z, prv->s, partDecrProof->e);
    mpz_mul(partDecrProof->z, partDecrProof->z, pub->delta);
    mpz_add(partDecrProof->z, r, partDecrProof->z);

    // clear local variables
    mpz_clear(r);
    mpz_clear(a);
    mpz_clear(b);
    if ( !r_hex )
        gmp_randclear(rand);

    return partDecrProof;
}

bool
paillier_verify_decryption( paillier_pubkey_t* pub,
                            paillier_partialdecryption_proof_t* dec_proof )
{
    mpz_t a;
    mpz_t b;
    mpz_t e;
    mpz_t temp;

    mpz_init(a);
    mpz_init(b);
    mpz_init(e);
    mpz_init(temp);

    // tries to compute the original a = c^4z * ci^(2*-e)
    mpz_powm(a, dec_proof->c4, dec_proof->z, pub->n_squared);
    mpz_ui_sub(temp, 0, dec_proof->e);
    mpz_powm(temp, dec_proof->ci2, temp, pub->n_squared);
    mpz_mul(a, a, temp);
    mpz_mod(a, a, pub->n_squared);

    // tries to compute the original b = v^z * vi^(-e)
    mpz_powm(b, pub->v, dec_proof->z, pub->n_squared);
    mpz_ui_sub(temp, 0, dec_proof->e);
    mpz_powm(temp, pub->verificationKeys[dec_proof->id - 1]->v, temp, pub->n_squared);
    mpz_mul(b, b, temp);
    mpz_mod(b, b, pub->n_squared);

    // tries to rehash the value H(a, b, c^4, ci2)
    uint256 hash = hash4(a, b, dec_proof->c4, dec_proof->ci2);
    mpz_set_str(e, hash.GetHex().c_str(), 16);

    // see if the original hash is equal to the guessed hash
    bool result = mpz_cmp(e, dec_proof->e) == 0;

    // clear temporary variables
    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(e);
    mpz_clear(temp);

    return result;
}

paillier_plaintext_t*
paillier_combining( paillier_plaintext_t* res,
                            paillier_pubkey_t* pub,
                            paillier_partialdecryption_proof_t** partDecr)
{
    mpz_t cprime;
    mpz_t divisor;
    mpz_t lambda;
    mpz_t exp;
    mpz_t factor;
    mpz_t L;

    if( !res )
    {
        res = (paillier_plaintext_t*) malloc(sizeof(paillier_plaintext_t));
        mpz_init(res->m);
    }

    /* initialize our integers */

    mpz_init(cprime);
    mpz_init(divisor);
    mpz_init(lambda);
    mpz_init(exp);
    mpz_init(factor);
    mpz_init(L);

    mpz_set_ui(cprime, 1);
    for (int i = 0; i < pub->threshold; ++i) {
        mpz_set(lambda, pub->delta);

        for (int j = 0; j < pub->threshold; ++j) {
            if (j != i)
            {
                mpz_mul_si(lambda, lambda, -partDecr[j]->id);
                mpz_set_si(divisor, partDecr[i]->id-partDecr[j]->id);
                mpz_tdiv_q(lambda, lambda, divisor);
            }
        }
        mpz_mul_ui(exp, lambda, 2);
        mpz_powm(factor, partDecr[i]->decryption, exp, pub->n_squared);
        mpz_mul(cprime, cprime, factor);
        mpz_mod(cprime, cprime, pub->n_squared);
    }

    mpz_sub_ui(L, cprime, 1);
    mpz_div(L, L, pub->n);
    mpz_mul(res->m, L, pub->combineSharesConstant);
    mpz_mod(res->m, res->m, pub->n);

    /* clear temporary integers */
    mpz_clear(cprime);
    mpz_clear(lambda);
    mpz_clear(exp);
    mpz_clear(factor);
    mpz_clear(L);

    return res;
}

void
paillier_mul( paillier_pubkey_t* pub,
                            paillier_ciphertext_pure_t* res,
                            paillier_ciphertext_pure_t* ct0,
                            paillier_ciphertext_pure_t* ct1 )
{
    mpz_mul(res->c, ct0->c, ct1->c);
    mpz_mod(res->c, res->c, pub->n_squared);
}

void
paillier_exp( paillier_pubkey_t* pub,
                            paillier_ciphertext_pure_t* res,
                            paillier_ciphertext_pure_t* ct,
                            paillier_plaintext_t* pt )
{
    mpz_powm(res->c, ct->c, pt->m, pub->n_squared);
}

paillier_plaintext_t*
paillier_plaintext_from_ui( unsigned long int x )
{
    paillier_plaintext_t* pt;

    pt = (paillier_plaintext_t*) malloc(sizeof(paillier_plaintext_t));
    mpz_init_set_ui(pt->m, x);

    return pt;
}

paillier_plaintext_t*
paillier_plaintext_from_bytes( void* m, int len )
{
    paillier_plaintext_t* pt;

    pt = (paillier_plaintext_t*) malloc(sizeof(paillier_plaintext_t));
    mpz_init(pt->m);
    mpz_import(pt->m, len, 1, 1, 0, 0, m);

    return pt;
}

void*
paillier_plaintext_to_bytes( int len, paillier_plaintext_t* pt )
{
    void* buf0;
    void* buf1;
    size_t written;

    buf0 = mpz_export(0, &written, 1, 1, 0, 0, pt->m);

    if( written == len )
        return buf0;

    buf1 = malloc(len);
    memset(buf1, 0, len);

    if( written == 0 )
        /* no need to copy anything, pt->m = 0 and buf0 was not allocated */
        return buf1;
    else if( written < len )
        /* pad with leading zeros */
        memcpy(buf1 + (len - written), buf0, written);
    else
        /* truncate leading garbage */
        memcpy(buf1, buf0 + (written - len), len);

    free(buf0);

    return buf1;
}

paillier_plaintext_t*
paillier_plaintext_from_str( char* str )
{
    return paillier_plaintext_from_bytes(str, strlen(str));
}

char*
paillier_plaintext_to_str( paillier_plaintext_t* pt )
{
    char* buf;
    size_t len;

    buf = (char*) mpz_export(0, &len, 1, 1, 0, 0, pt->m);
    buf = (char*) realloc(buf, len + 1);
    buf[len] = 0;

    return buf;
}

paillier_ciphertext_pure_t*
paillier_ciphertext_from_bytes( void* c, int len )
{
    paillier_ciphertext_pure_t* ct;

    ct = (paillier_ciphertext_pure_t*) malloc(sizeof(paillier_ciphertext_pure_t));
    mpz_init(ct->c);
    mpz_import(ct->c, len, 1, 1, 0, 0, c);

    return ct;
}

void*
paillier_ciphertext_to_bytes( int len, paillier_ciphertext_pure_t* ct )
{
    void* buf;
    int cur_len;

    cur_len = mpz_sizeinbase(ct->c, 2);
    cur_len = PAILLIER_BITS_TO_BYTES(cur_len);
    buf = malloc(len);
    memset(buf, 0, len);
    mpz_export(buf + (len - cur_len), 0, 1, 1, 0, 0, ct->c);

    return buf;
}

void
paillier_freepolynomialpoint( paillier_polynomial_point_t* p )
{
    mpz_clear(p->p);
    free(p);
}

void
paillier_freeverificationkey( paillier_verificationkey_t* v )
{
    mpz_clear(v->v);
    free(v);
}

void
paillier_freepartdecryptionproof( paillier_partialdecryption_proof_t* pdp )
{
    mpz_clear(pdp->c4);
    mpz_clear(pdp->ci2);
    mpz_clear(pdp->decryption);
    mpz_clear(pdp->e);
    mpz_clear(pdp->z);
    free(pdp);
}

void
paillier_freepubkey( paillier_pubkey_t* pub )
{
    mpz_clear(pub->n);
    mpz_clear(pub->n_squared);
    mpz_clear(pub->n_plusone);
    mpz_clear(pub->delta);
    mpz_clear(pub->v);
    mpz_clear(pub->combineSharesConstant);

    for (int i = 0; i < pub->decryptServers; ++i) {
        paillier_freeverificationkey(pub->verificationKeys[i]);
    }
    free(pub->verificationKeys);
    free(pub);
}

void
paillier_freepartkey( paillier_partialkey_t* prv )
{
    mpz_clear(prv->s);
    free(prv);
}

void
paillier_freepartkeysarray( paillier_partialkey_t** prv, int length )
{
    for (int i = 0; i < length; ++i)
        paillier_freepartkey(prv[i]);
    free(prv);
}

void
paillier_freeplaintext( paillier_plaintext_t* pt )
{
    mpz_clear(pt->m);
    free(pt);
}

void
paillier_freeciphertext( paillier_ciphertext_pure_t *ct)
{
    mpz_clear(ct->c);
    free(ct);
}

void
paillier_freeciphertextproof( paillier_ciphertext_proof_t *ct)
{
    mpz_clear(ct->c);
    mpz_clear(ct->e);
    mpz_clear(ct->e1);
    mpz_clear(ct->v1);
    mpz_clear(ct->e2);
    mpz_clear(ct->v2);
    free(ct);
}

void
paillier_get_rand_file( void* buf, int len, char* file )
{
    FILE* fp;
    void* p;

    fp = fopen(file, "r");

    p = buf;
    while( len )
    {
        size_t s;
        s = fread(p, 1, len, fp);
        p += s;
        len -= s;
    }

    fclose(fp);
}

void
paillier_get_rand_devrandom( void* buf, int len )
{
    paillier_get_rand_file(buf, len, "/dev/random");
}

void
paillier_get_rand_devurandom( void* buf, int len )
{
    paillier_get_rand_file(buf, len, "/dev/urandom");
}

paillier_ciphertext_pure_t*
paillier_create_enc_zero()
{
    paillier_ciphertext_pure_t* ct;

    /* make a NON-RERANDOMIZED encryption of zero for the purposes of
         homomorphic computation */

    /* note that this is just the number 1 */

    ct = (paillier_ciphertext_pure_t*) malloc(sizeof(paillier_ciphertext_pure_t));
    mpz_init_set_ui(ct->c, 1);

    return ct;
}
