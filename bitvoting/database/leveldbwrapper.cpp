// Copyright (c) 2012-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "leveldbwrapper.h"
#include "helper.h"

#include <boost/filesystem.hpp>
#include <leveldb/cache.h>
#include <leveldb/env.h>
#include <leveldb/filter_policy.h>
#include <leveldb/helpers/memenv.h>

void HandleError(const leveldb::Status &status)
{
    if (status.ok())
        return;
    if (status.IsCorruption())
        Log::e("(LevelDB) Database corrupted");
    if (status.IsIOError())
        Log::e("(LevelDB) Database I/O error");
    if (status.IsNotFound())
        Log::e("(LevelDB) Database entry missing");

    Log::e("(LevelDB) Unknown database error");
}

static leveldb::Options GetOptions(size_t nCacheSize)
{
    leveldb::Options options;
    options.block_cache = leveldb::NewLRUCache(nCacheSize / 2);
    // up to two write buffers may be held in memory simultaneously
    options.write_buffer_size = nCacheSize / 4;
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    options.compression = leveldb::kNoCompression;
    options.max_open_files = 64;
    return options;
}

// ====================================================================

LevelDBWrapper::LevelDBWrapper(const boost::filesystem::path &path, const size_t nCacheSize, bool fMemory, bool fWipe)
{
    penv = NULL;
    readoptions.verify_checksums = true;
    iteroptions.verify_checksums = true;
    iteroptions.fill_cache = false;
    syncoptions.sync = true;
    options = GetOptions(nCacheSize);
    options.create_if_missing = true;
    if (fMemory)
    {
        penv = leveldb::NewMemEnv(leveldb::Env::Default());
        options.env = penv;
    }
    else
    {
        if (fWipe)
        {
            leveldb::DestroyDB(path.string(), options);
        }
        Helper::CreateDirectories(path);
    }
    leveldb::Status status = leveldb::DB::Open(options, path.string(), &pdb);
    HandleError(status);
}

LevelDBWrapper::~LevelDBWrapper()
{
    delete pdb;
    pdb = NULL;

    delete options.filter_policy;
    options.filter_policy = NULL;

    delete options.block_cache;
    options.block_cache = NULL;

    delete penv;
    options.env = NULL;
}

bool LevelDBWrapper::WriteBatch(LevelDBBatch &batch, bool fSync)
{
    leveldb::Status status = pdb->Write(fSync ? syncoptions : writeoptions, &batch.batch);
    HandleError(status);
    return true;
}
