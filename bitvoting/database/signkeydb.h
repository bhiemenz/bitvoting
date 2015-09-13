/*=============================================================================

Providing access to the database for sign keys. The client does not use this
interface directly, because the SignKeyStore handles all request on sign key
operations and forwards it to this class if necessary.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_SIGNKEYDB_H
#define BITVOTING_SIGNKEYDB_H

#include "settings.h"
#include "bitcoin/key.h"
#include "database/leveldbwrapper.h"

#include <string>

#include <boost/filesystem/path.hpp>

class SignKeyDB : public LevelDBWrapper
{
private:

    // Use SignKeyStore as handler for sign keys to avoid DB requests
    friend class SignKeyStore;

    // Singleton
    SignKeyDB(const boost::filesystem::path databaseDir, const int64_t cacheSize = Settings::DEFAULT_DB_CACHE)
            : LevelDBWrapper(databaseDir, cacheSize)
    { }

    SignKeyDB(SignKeyDB const&)       = delete;
    void operator=(SignKeyDB const&)     = delete;

    static SignKeyDB& getInstance()
    {
        static SignKeyDB instance(boost::filesystem::path(Settings::GetDirectory()) / "databases" / "signKeys");
        return instance;
    }

    // Write a sign key to database
    static bool writeSignKey(const SignKeyPair &signKeyPair)
    {
        SignKeyDB &db = SignKeyDB::getInstance();
        return db.Write(signKeyPair.second.GetID(), signKeyPair);
    }

    // Read a sign key from database
    static bool readSignKey(const CKeyID &id, SignKeyPair &signKeyPair)
    {
        SignKeyDB &db = SignKeyDB::getInstance();
        return db.Read(id, signKeyPair);
    }

    // Erase a sign key from database
    static void eraseSignKey(const CKeyID &id)
    {
        SignKeyDB &db = SignKeyDB::getInstance();
        db.Erase(id);
    }

    // Generate iterator for database
    static leveldb::Iterator* getIterator()
    {
        SignKeyDB &db = SignKeyDB::getInstance();
        return db.NewIterator();
    }
};

#endif
