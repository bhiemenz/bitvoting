/*=============================================================================

All sign keys are stored in a special database accessable by the class
SignKeyDB. This store reads in all sign keys and provides several operations
to handle these keys.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_STORE_H
#define BITVOTING_STORE_H

#include "bitcoin/key.h"
#include "bitcoin/uint256.h"
#include "database/signkeydb.h"

#include <mutex>
#include <string>
#include <utility>

#include <boost/foreach.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>

// ----------------------------------------------------------------
template<class T>
class Store
{
public:

    Store(){ this->map.clear(); }
    virtual ~Store() { this->map.clear(); }

protected:

    // Storage for elements
    typedef std::map<uint160, T> Map;
    Map map;

    // ----------------------------------------------------------------

    // Add an element to store
    void addElement(const uint160 id, const T &element)
    {
        map.emplace(id, element);
    }

    // Check if element exists
    bool containsElement(const uint160 &id)
    {
        return map.count(id) > 0;
    }

    // Remove an element from store
    void removeElement(const uint160 &id)
    {
        map.erase(id);
    }

    // Get all IDs from store
    void getAllIDs(std::set<uint160> &idsOut)
    {
        idsOut.clear();
        typename Map::const_iterator mi = map.begin();
        while (mi != map.end())
        {
            idsOut.insert(mi->first);
            mi++;
        }
    }
};

// ----------------------------------------------------------------
class SignKeyStore : public Store<SignKeyPair>
{
public:

    // Generate new sign key pair and add to store and database
    static bool genNewSignKeyPair(Role role, SignKeyPair &keyOut)
    {
        // generate key
        CKey key(role);
        key.MakeNewKey();

        // calculate public key
        CPubKey pubKey = key.GetPubKey();

        SignKeyPair result = std::make_pair(key, pubKey);

        // add key to store and database
        if (!addSignKeyPair(result))
            return false;

        keyOut = result;
        return true;
    }

    // Add a sign key pair to store and database
    static bool addSignKeyPair(const SignKeyPair &keypair)
    {
        // add sign key to store
        SignKeyStore &instance = SignKeyStore::GetInstance();
        instance.addElement(keypair.second.GetID(), keypair);

        // add sign key to database
        return SignKeyDB::writeSignKey(keypair);
    }

    // Get a sign key pair from store
    static bool getSignKeyPair(const uint160 &id, SignKeyPair &keypairOut)
    {
        std::map<uint160, SignKeyPair> keymap = SignKeyStore::GetInstance().map;
        Map::const_iterator mi = keymap.find(id);
        if (mi != keymap.end())
        {
            keypairOut = mi->second;
            return true;
        }
        return false;
    }

    // Remove a sign key pair from store and database
    static void removeSignKeyPair(const uint160 &id)
    {
        // remove sign key from store
        SignKeyStore &instance = SignKeyStore::GetInstance();
        instance.removeElement(id);

        // erase sign key from database
        SignKeyDB::eraseSignKey(id);
    }

    // Get a sign public key from store
    static bool getSignPubKey(const uint160 &id, CPubKey &pubKeyOut)
    {
        SignKeyPair key;
        if (!getSignKeyPair(id, key))
            return false;

        pubKeyOut = key.second;
        return true;
    }

    // Check if sign key pair is known
    static bool containsSignKeyPair(const uint160 &id)
    {
        SignKeyStore &instance = SignKeyStore::GetInstance();
        return instance.containsElement(id);
    }

    // Get all keys of a given role
    static void getAllKeysOfType(Role role, std::vector<SignKeyPair> &keysOut)
    {
        keysOut.clear();
        std::map<uint160, SignKeyPair> keymap = SignKeyStore::GetInstance().map;

        typename Map::const_iterator mi = keymap.begin();
        while (mi != keymap.end())
        {
            SignKeyPair current = mi->second;
            if(current.first.getRole() == role)
                keysOut.push_back(current);
            mi++;
        }
    }

    // Get all keys
    static void getAllKeys(std::vector<SignKeyPair> &keysOut)
    {
        keysOut.clear();
        std::map<uint160, SignKeyPair> keymap = SignKeyStore::GetInstance().map;

        typename Map::const_iterator mi = keymap.begin();
        while (mi != keymap.end())
        {
            SignKeyPair current = mi->second;
            keysOut.push_back(current);
            mi++;
        }
    }

    // Print all keys in store
    static std::string toString()
    {
        std::map<uint160, SignKeyPair> keymap = SignKeyStore::GetInstance().map;

        std::string result("KeyStore:\n{");
        std::pair<CKeyID, SignKeyPair> entry;

        BOOST_FOREACH(entry, keymap)
        {
            result += "\n\npkID=" + entry.first.ToString();
            result += "\nkeyRole=" + roleToString(entry.second.first.getRole());
        }
        result += "\n\n}";
        return result;
    }

private:

    // Singleton
    SignKeyStore() : Store<SignKeyPair>()
    {
        // load sign keys from database
        loadAllKeysFromDatabase();
    }

    SignKeyStore(SignKeyStore const&)       = delete;
    void operator=(SignKeyStore const&)     = delete;

    static SignKeyStore& GetInstance()
    {
        static SignKeyStore instance;
        return instance;
    }

    // Load all sign keys from database
    bool loadAllKeysFromDatabase()
    {
        try
        {
            leveldb::Iterator *iter = SignKeyDB::getIterator();
            if (!iter)
            {
                Log::e("(Store) Error getting database iterator");
                return false;
            }

            for (iter->SeekToFirst(); iter->Valid(); iter->Next())
            {
                // read next item in database
                std::stringstream ssKey;
                ssKey << iter->key().ToString();
                std::stringstream ssValue;
                ssValue << iter->value().ToString();

                // key
                boost::archive::text_iarchive ia(ssKey);
                uint160 key;
                ia >> key;

                // value
                std::pair<CKey, CPubKey> ckeypair;
                boost::archive::text_iarchive iaValue(ssValue);
                iaValue >> ckeypair;

                // check key
                if (!ckeypair.first.IsValid() || !ckeypair.second.IsFullyValid() || key != ckeypair.second.GetID())
                {
                    Log::w("(Store) Could not load/verify key with id: %s", ckeypair.second.GetID().ToString().c_str());
                    continue;
                }

                // add to store
                addElement(ckeypair.second.GetID(), ckeypair);
            }

            // check for any errors found during the scan
            assert(iter->status().ok());
            delete iter;
        }
        catch (...) {
            return false;
        }

        return true;
    }
};

#endif
