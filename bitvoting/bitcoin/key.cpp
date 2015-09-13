// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bitcoin/key.h"

#include <iostream>

#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>

// anonymous namespace with local implementation code (OpenSSL interaction)
namespace {

// ====================================================================

// Generate a private key from just the secret parameter
int EC_KEY_regenerate_key(EC_KEY *eckey, BIGNUM *priv_key)
{
    int ok = 0;
    BN_CTX *ctx = NULL;
    EC_POINT *pub_key = NULL;

    if (!eckey) return 0;

    const EC_GROUP *group = EC_KEY_get0_group(eckey);

    if ((ctx = BN_CTX_new()) == NULL)
        goto err;

    pub_key = EC_POINT_new(group);

    if (pub_key == NULL)
        goto err;

    if (!EC_POINT_mul(group, pub_key, priv_key, NULL, NULL, ctx))
        goto err;

    EC_KEY_set_private_key(eckey,priv_key);
    EC_KEY_set_public_key(eckey,pub_key);

    ok = 1;

err:

    if (pub_key)
        EC_POINT_free(pub_key);
    if (ctx != NULL)
        BN_CTX_free(ctx);

    return(ok);
}

// ====================================================================

class CECKey {
private:
    EC_KEY *pkey;

public:
    CECKey() {
        pkey = EC_KEY_new_by_curve_name(NID_secp256k1);
        assert(pkey != NULL);
    }

    ~CECKey() {
        EC_KEY_free(pkey);
    }

    void GetSecretBytes(unsigned char vch[32]) const {
        const BIGNUM *bn = EC_KEY_get0_private_key(pkey);
        assert(bn);
        int nBytes = BN_num_bytes(bn);
        int n=BN_bn2bin(bn,&vch[32 - nBytes]);
        assert(n == nBytes);
        memset(vch, 0, 32 - nBytes);
    }

    void SetSecretBytes(const unsigned char vch[32]) {
        bool ret;
        BIGNUM bn;
        BN_init(&bn);
        ret = BN_bin2bn(vch, 32, &bn);
        assert(ret);
        ret = EC_KEY_regenerate_key(pkey, &bn);
        assert(ret);
        BN_clear_free(&bn);
    }

    void GetPrivKey(CPrivKey &privkey) {
        EC_KEY_set_conv_form(pkey, POINT_CONVERSION_COMPRESSED);
        int nSize = i2d_ECPrivateKey(pkey, NULL);
        assert(nSize);
        privkey.resize(nSize);
        unsigned char* pbegin = &privkey[0];
        int nSize2 = i2d_ECPrivateKey(pkey, &pbegin);
        assert(nSize == nSize2);
    }

    bool SetPrivKey(const CPrivKey &privkey, bool fSkipCheck=false) {
        const unsigned char* pbegin = &privkey[0];
        if (d2i_ECPrivateKey(&pkey, &pbegin, privkey.size())) {
            if(fSkipCheck)
                return true;
            
            // d2i_ECPrivateKey returns true if parsing succeeds.
            // This doesn't necessarily mean the key is valid.
            if (EC_KEY_check_key(pkey))
                return true;
        }
        return false;
    }

    void GetPubKey(CPubKey &pubkey) {
        EC_KEY_set_conv_form(pkey, POINT_CONVERSION_COMPRESSED);
        int nSize = i2o_ECPublicKey(pkey, NULL);
        assert(nSize);
        assert(nSize <= 65);
        unsigned char c[65];
        unsigned char *pbegin = c;
        int nSize2 = i2o_ECPublicKey(pkey, &pbegin);
        assert(nSize == nSize2);
        pubkey.Set(&c[0], &c[nSize]);
    }

    bool SetPubKey(const CPubKey &pubkey) {
        const unsigned char* pbegin = pubkey.begin();
        return o2i_ECPublicKey(&pkey, &pbegin, pubkey.size());
    }

    bool Sign(const uint256 &hash, std::vector<unsigned char>& vchSig) {
        vchSig.clear();
        ECDSA_SIG *sig = ECDSA_do_sign((unsigned char*)&hash, sizeof(hash), pkey);
        if (sig == NULL)
            return false;
        BN_CTX *ctx = BN_CTX_new();
        BN_CTX_start(ctx);
        const EC_GROUP *group = EC_KEY_get0_group(pkey);
        BIGNUM *order = BN_CTX_get(ctx);
        BIGNUM *halforder = BN_CTX_get(ctx);
        EC_GROUP_get_order(group, order, ctx);
        BN_rshift1(halforder, order);
        if (BN_cmp(sig->s, halforder) > 0) {
            // enforce low S values, by negating the value (modulo the order) if above order/2.
            BN_sub(sig->s, order, sig->s);
        }
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
        unsigned int nSize = ECDSA_size(pkey);
        vchSig.resize(nSize); // Make sure it is big enough
        unsigned char *pos = &vchSig[0];
        nSize = i2d_ECDSA_SIG(sig, &pos);
        ECDSA_SIG_free(sig);
        vchSig.resize(nSize); // Shrink to fit actual size
        return true;
    }

    bool Verify(const uint256 &hash, const std::vector<unsigned char>& vchSig) {
        if (vchSig.empty())
            return false;

        // New versions of OpenSSL will reject non-canonical DER signatures. de/re-serialize first.
        unsigned char *norm_der = NULL;
        ECDSA_SIG *norm_sig = ECDSA_SIG_new();
        const unsigned char* sigptr = &vchSig[0];
        assert(norm_sig);
        if (d2i_ECDSA_SIG(&norm_sig, &sigptr, vchSig.size()) == NULL)
        {
            /* As of OpenSSL 1.0.0p d2i_ECDSA_SIG frees and nulls the pointer on
             * error. But OpenSSL's own use of this function redundantly frees the
             * result. As ECDSA_SIG_free(NULL) is a no-op, and in the absence of a
             * clear contract for the function behaving the same way is more
             * conservative.
             */
            ECDSA_SIG_free(norm_sig);
            return false;
        }
        int derlen = i2d_ECDSA_SIG(norm_sig, &norm_der);
        ECDSA_SIG_free(norm_sig);
        if (derlen <= 0)
            return false;

        // -1 = error, 0 = bad sig, 1 = good
        bool ret = ECDSA_verify(0, (unsigned char*)&hash, sizeof(hash), norm_der, derlen, pkey) == 1;
        OPENSSL_free(norm_der);
        return ret;
    }
};

}; // end of anonymous namespace

bool ECC_InitSanityCheck()
{
    EC_KEY *pkey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if(pkey == NULL)
        return false;
    EC_KEY_free(pkey);
    return true;
}

// ====================================================================

bool CKey::Sign(const uint256 &hash, std::vector<unsigned char>& vchSig) const
{
    if (!fValid)
        return false;
    CECKey key;
    key.SetSecretBytes(vch);
    return key.Sign(hash, vchSig);
}

bool CKey::Check(const unsigned char *vch)
{
    // Do not convert to OpenSSL's data structures for range-checking keys,
    // it's easy enough to do directly.
    static const unsigned char vchMax[32] = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,
        0xBA,0xAE,0xDC,0xE6,0xAF,0x48,0xA0,0x3B,
        0xBF,0xD2,0x5E,0x8C,0xD0,0x36,0x41,0x40
    };
    bool fIsZero = true;
    for (int i=0; i<32 && fIsZero; i++)
        if (vch[i] != 0)
            fIsZero = false;
    if (fIsZero)
        return false;
    for (int i=0; i<32; i++) {
        if (vch[i] < vchMax[i])
            return true;
        if (vch[i] > vchMax[i])
            return false;
    }
    return true;
}

void CKey::MakeNewKey() {
    do {
        RAND_bytes(vch, sizeof(vch));
    } while (!Check(vch));
    fValid = true;
}

bool CKey::SetPrivKey(const CPrivKey &privkey)
{
    CECKey key;
    if (!key.SetPrivKey(privkey))
        return false;
    key.GetSecretBytes(vch);
    fValid = true;
    return true;
}

CPrivKey CKey::GetPrivKey() const
{
    assert(fValid);
    CECKey key;
    key.SetSecretBytes(vch);
    CPrivKey privkey;
    key.GetPrivKey(privkey);
    return privkey;
}

CPubKey CKey::GetPubKey() const
{
    assert(fValid);
    CECKey key;
    key.SetSecretBytes(vch);
    CPubKey pubkey(role);
    key.GetPubKey(pubkey);
    return pubkey;
}

// ====================================================================

bool CPubKey::Verify(const uint256 &hash, const std::vector<unsigned char>& vchSig) const
{
    if (!IsValid())
        return false;
    CECKey key;
    if (!key.SetPubKey(*this))
        return false;
    if (!key.Verify(hash, vchSig))
        return false;
    return true;
}

bool CPubKey::IsFullyValid() const
{
    if (!IsValid())
        return false;
    CECKey key;
    if (!key.SetPubKey(*this))
        return false;
    return true;
}

// ====================================================================

std::string roleToString(Role r)
{
    switch(r)
    {
    case KEY_ELECTION: return "key-election";
    case KEY_VOTE: return "key-vote";
    case KEY_TRUSTEE: return "key-trustee";
    default: break;
    }
    return "key-unknown";
}
