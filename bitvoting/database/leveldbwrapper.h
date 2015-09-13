/*=============================================================================

Base class to operation on a database. LevelDB works as a key/value database.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_LEVELDBWRAPPER_H
#define BITVOTING_LEVELDBWRAPPER_H

#include <sstream>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/serialization/utility.hpp>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

// ----------------------------------------------------------------
// Print error to console
void HandleError(const leveldb::Status &status);

// ----------------------------------------------------------------
// Batch of changes queued to be written to a LevelDBWrapper
class LevelDBBatch
{
public:

    template<typename K, typename V>
    void Write(const K& key, const V& value)
    {
        // key
        std::stringstream keyStream;
        boost::archive::text_oarchive oaKey(keyStream);
        oaKey << key;
        std::string strKey = keyStream.str();
        leveldb::Slice slKey((char*)strKey.c_str(), strKey.size());

        // value
        std::stringstream valueStream;
        boost::archive::text_oarchive oaValue(valueStream);
        oaValue << value;
        std::string strValue = valueStream.str();
        leveldb::Slice slValue((char*)strValue.c_str(), strValue.size());
        batch.Put(slKey, slValue);
    }

    template<typename K>
    void Erase(const K& key)
    {
        // key
        std::stringstream keyStream;
        boost::archive::text_oarchive oaKey(keyStream);
        oaKey << key;
        std::string strKey = keyStream.str();
        leveldb::Slice slKey((char*)strKey.c_str(), strKey.size());
        batch.Delete(slKey);
    }

private:

    friend class LevelDBWrapper;

    leveldb::WriteBatch batch;
};

// ----------------------------------------------------------------
class LevelDBWrapper
{
public:

    LevelDBWrapper(const boost::filesystem::path &path, size_t nCacheSize, bool fMemory = false, bool fWipe = false);
    ~LevelDBWrapper();

    // ----------------------------------------------------------------

    template<typename K, typename V>
    bool Read(const K& key, V& value)
    {
        // key
        std::stringstream keyStream;
        boost::archive::text_oarchive oaKey(keyStream);
        oaKey << key;
        std::string strKey = keyStream.str();
        leveldb::Slice slKey((char*)strKey.c_str(), strKey.size());

        // value
        std::string strValue;
        leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
        if (!status.ok())
        {
            if (status.IsNotFound())
                return false;
            HandleError(status);
        }
        try
        {
            std::stringstream outStream(strValue.data());
            boost::archive::text_iarchive ia(outStream);
            ia >> value;
        }
        catch(std::exception &e)
        {
            return false;
        }
        return true;
    }

    template<typename K>
    bool Exists(const K& key)
    {
        // key
        std::stringstream keyStream;
        boost::archive::text_oarchive oaKey(keyStream);
        oaKey << key;
        std::string strKey = keyStream.str();
        leveldb::Slice slKey((char*)strKey.c_str(), strKey.size());

        // value
        std::string strValue;
        leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
        if (!status.ok()) {
            if (status.IsNotFound())
                return false;
            HandleError(status);
        }
        return true;
    }

    template<typename K, typename V>
    bool Write(const K& key, const V& value, bool fSync = false)
    {
        LevelDBBatch batch;
        batch.Write(key, value);
        return WriteBatch(batch, fSync);
    }

    template<typename K>
    bool Erase(const K& key, bool fSync = false)
    {
        LevelDBBatch batch;
        batch.Erase(key);
        return WriteBatch(batch, fSync);
    }

    bool WriteBatch(LevelDBBatch &batch, bool fSync = false);

    bool Sync()
    {
        LevelDBBatch batch;
        return WriteBatch(batch, true);
    }

    // ----------------------------------------------------------------

    leveldb::Iterator *NewIterator()
    {
        return pdb->NewIterator(iteroptions);
    }

private:

    // Custom environment this database is using (may be NULL in case of default environment)
    leveldb::Env *penv;

    // Database options used
    leveldb::Options options;

    // Options used when reading from the database
    leveldb::ReadOptions readoptions;

    // Options used when iterating over values of the database
    leveldb::ReadOptions iteroptions;

    // Options used when writing to the database
    leveldb::WriteOptions writeoptions;

    // Options used when sync writing to the database
    leveldb::WriteOptions syncoptions;

    // The database itself
    leveldb::DB *pdb;
};

#endif
