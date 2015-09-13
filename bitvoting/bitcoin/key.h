// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Modified by Benedikt Hiemenz, Max Kolhagen, Markus Schmidt 

#ifndef BITCOIN_KEY_H
#define BITCOIN_KEY_H

#include "helper.h"
#include "bitcoin/allocators.h"
#include "bitcoin/hash.h"
#include "bitcoin/uint256.h"

#include <stdexcept>
#include <vector>

#include <boost/serialization/access.hpp>

// ----------------------------------------------------------------
enum Role
{
    KEY_UNKNOWN,
    KEY_TRUSTEE,  // use for trustee
    KEY_ELECTION, // use for election
    KEY_VOTE,     // use for vote
    KEY_MINING    // use for mining
};

// ----------------------------------------------------------------
// A reference to a CKey: the Hash160 of its serialized public key
class CKeyID : public uint160
{
public:
    CKeyID() : uint160(0) { }
    CKeyID(const uint160 &in) : uint160(in) { }
};

// ----------------------------------------------------------------
// CPrivKey is a serialized private key, with all parameters included (279 bytes)
typedef std::vector<unsigned char, secure_allocator<unsigned char> > CPrivKey;

// ----------------------------------------------------------------
// An encapsulated public key
class CPubKey
{
public:

    // Constructor
    CPubKey() : role(KEY_UNKNOWN) { Invalidate(); }
    CPubKey(Role role) : role(role) { Invalidate(); }

    // Construct a public key from a byte vector.
    CPubKey(const std::vector<unsigned char> &vch, Role role) : role(role)
    {
        Set(vch.begin(), vch.end());
    }

    // Initialize a public key using begin/end iterators to byte data.
    template<typename T>
    void Set(const T pbegin, const T pend)
    {
        int len = pend == pbegin ? 0 : GetLen(pbegin[0]);
        if (len && len == (pend-pbegin))
            memcpy(vch, (unsigned char*)&pbegin[0], len);
        else
            Invalidate();
    }

    // Simple read-only vector-like interface to the pubkey data.
    unsigned int size() const { return GetLen(vch[0]); }
    const unsigned char *begin() const { return vch; }
    const unsigned char *end() const { return vch + size(); }
    const unsigned char &operator[](unsigned int pos) const { return vch[pos]; }

    // ----------------------------------------------------------------

    // Get the KeyID of this public key (excluding role)
    CKeyID GetID() const { return CKeyID(Hash160(vch, vch+size())); }

    // Get the 256-bit hash of this public key (excluding role)
    uint256 GetHash() const { return Hash(vch, vch+size()); }

    // Get role for this key
    Role getRole() const { return role; }

    // Note that this is consensus critical as CheckSig() calls it!
    bool IsValid() const { return size() > 0; }

    // Fully validate whether this is a valid public key (more expensive than IsValid())
    bool IsFullyValid() const;

    // Verify a DER signature (~72 bytes). If this public key
    // is not fully valid, the return value will be false.
    bool Verify(const uint256 &hash, const std::vector<unsigned char>& vchSig) const;

    // ----------------------------------------------------------------

    friend bool operator==(const CPubKey &a, const CPubKey &b)
    {
            return a.getRole() == b.getRole() &&
                    a.vch[0] == b.vch[0] &&
                    memcmp(a.vch, b.vch, a.size()) == 0;
    }

    friend bool operator!=(const CPubKey &a, const CPubKey &b)
    {
        return !(a == b);
    }

    friend bool operator<(const CPubKey &a, const CPubKey &b)
    {
        return a.vch[0] < b.vch[0] ||
               (a.vch[0] == b.vch[0] && memcmp(a.vch, b.vch, a.size()) < 0);
    }

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        if(version == 0)
        {
            ar & role;
            ar & vch;
        }
    }

private:

    friend class boost::serialization::access;

    // The serialized data
    unsigned char vch[65];

    Role role;

    // Compute the length of a pubkey with a given first byte
    unsigned int static GetLen(unsigned char chHeader)
    {
        if (chHeader == 2 || chHeader == 3)
            return 33;
        if (chHeader == 4 || chHeader == 6 || chHeader == 7)
            return 65;
        return 0;
    }

    // Set this key data to be invalid
    void Invalidate() { vch[0] = 0xFF; }
};

// ----------------------------------------------------------------
// An encapsulated private key
class CKey
{
public:

    // Construct an invalid private key
    CKey() : fValid(false), role(KEY_UNKNOWN) { LockObject(vch); }
    CKey(Role role) : fValid(false), role(role) { LockObject(vch); }

    // Copy constructor. This is necessary because of memlocking
    CKey(const CKey &secret) : fValid(secret.fValid), role(secret.getRole())
    {
        LockObject(vch);
        memcpy(vch, secret.vch, sizeof(vch));
    }

    // Destructor (again necessary because of memlocking)
    ~CKey()
    {
        UnlockObject(vch);
    }

    // Initialize using begin and end iterators to byte data
    template<typename T>
    void Set(const T pbegin, const T pend) {
        if (pend - pbegin != 32) {
            fValid = false;
            return;
        }
        if (Check(&pbegin[0]))
        {
            memcpy(vch, (unsigned char*)&pbegin[0], 32);
            fValid = true;
        } else {
            fValid = false;
        }
    }

    // Simple read-only vector-like interface.
    unsigned int size() const { return ( fValid ? 32 : 0); }
    const unsigned char *begin() const { return vch; }
    const unsigned char *end() const { return vch + size(); }

    // Check whether this private key is valid
    bool IsValid() const { return fValid; }

    // Get role for this key
    Role getRole() const { return role; }

    // Initialize from a CPrivKey (serialized OpenSSL private key data)
    bool SetPrivKey(const CPrivKey &vchPrivKey);

    // Generate a new private key using a cryptographic PRNG
    void MakeNewKey();

    // Convert the private key to a CPrivKey (serialized OpenSSL private key data)
    CPrivKey GetPrivKey() const;

    // Compute the public key from a private key
    CPubKey GetPubKey() const;

    // Create a DER-serialized signature
    bool Sign(const uint256 &hash, std::vector<unsigned char>& vchSig) const;

    // ----------------------------------------------------------------

    friend bool operator==(const CKey &a, const CKey &b) {
        return a.getRole() == b.getRole() &&
                a.size() == b.size() &&
                memcmp(&a.vch[0], &b.vch[0], a.size()) == 0;
    }

    friend bool operator!=(const CKey &a, const CKey &b) {
        return !(a.size() == b.size() && memcmp(&a.vch[0], &b.vch[0], a.size()) == 0);
    }

    friend bool operator<(const CKey &a, const CKey &b)
    {
        return a.vch[0] < b.vch[0] ||
               (a.vch[0] == b.vch[0] && memcmp(a.vch, b.vch, a.size()) < 0);
    }

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        if(version == 0)
        {
            ar & role;
            ar & fValid;
            ar & vch;
        }
    }

private:

    friend class boost::serialization::access;

    // Whether this private key is valid. We check for correctness when modifying
    // the key data, so fValid should always correspond to the actual state.
    bool fValid;

    Role role;

    // The actual byte data
    unsigned char vch[32];

    // Check whether the 32-byte array pointed to be vch is valid keydata
    bool static Check(const unsigned char *vch);

    // Set this key data to be invalid
    void Invalidate() { vch[0] = 0xFF; }
};

// ----------------------------------------------------------------
// Keypair for signing
typedef std::pair<CKey, CPubKey> SignKeyPair;

// ----------------------------------------------------------------
// Check that required EC support is available at runtime
bool ECC_InitSanityCheck(void);

// ----------------------------------------------------------------
// Convert role to string
std::string roleToString(Role r);

#endif
