// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "util.h"
#include "database/db.h"
#include "bitcoin/hash.h"
#include "settings.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>
#include <boost/version.hpp>

#include <openssl/rand.h>

#include <stdint.h>
#include <sstream>

using namespace std;
using namespace boost;


CDBEnv bitdb;

CDBEnv::CDBEnv() :  dbenv(DB_CXX_NO_EXCEPTIONS)
{
    envIsInit = false;
}

CDBEnv::~CDBEnv()
{
   Shutdown();
}

void CDBEnv::Shutdown()
{
    if (!envIsInit)
        return;

    envIsInit = false;
    int ret = dbenv.close(0);

    if (ret != 0)
        printError("Error shutting down database environment");

    DbEnv(0).remove(path.string().c_str(), 0);
}

bool CDBEnv::Open(const boost::filesystem::path& pathIn)
{
    if (envIsInit)
        return true;

    boost::this_thread::interruption_point();

    path = pathIn;
    filesystem::path pathLogDir = path / "database";
    boost::filesystem::create_directories(pathLogDir);
    filesystem::path pathErrorFile = path / "db.log";
    printOutput(INFO, "Open database environment");

    dbenv.set_lg_dir(pathLogDir.string().c_str());
    dbenv.set_cachesize(0, 0x200000, 1); // 1 MiB should be enough for just the wallet
    dbenv.set_lg_bsize(0x10000);
    dbenv.set_lg_max(1048576);
    dbenv.set_lk_max_locks(40000);
    dbenv.set_lk_max_objects(40000);
    dbenv.set_errfile(fopen(pathErrorFile.string().c_str(), "a")); // log file for debugging
    dbenv.set_flags(DB_AUTO_COMMIT, 1);
    dbenv.set_flags(DB_TXN_WRITE_NOSYNC, 1);
    dbenv.log_set_config(DB_LOG_AUTO_REMOVE, 1);
    int ret = dbenv.open(path.string().c_str(),
                     DB_CREATE     |
                     DB_INIT_LOCK  |
                     DB_INIT_LOG   |
                     DB_INIT_MPOOL |
                     DB_INIT_TXN   |
                     DB_THREAD     |
                     DB_RECOVER    |
                     DB_PRIVATE,
                     0);
    if (ret != 0)
        return printError("Error opening database environment");

    envIsInit = true;
    return true;
}


void CDBEnv::CloseDb(const string &strFile)
{
    // cs_db.lock();
    if (mapDb[strFile] != NULL)
    {
        // Close the database handle
        Db* pdb = mapDb[strFile];
        pdb->close(0);
        delete pdb;
        mapDb[strFile] = NULL;
    }
}

bool CDBEnv::RemoveDb(const string &strFile)
{
    this->CloseDb(strFile);

    //cs_db.lock();
    int rc = dbenv.dbremove(NULL, strFile.c_str(), NULL, DB_AUTO_COMMIT);
    return (rc == 0);
}

void CDBEnv::Flush(bool fShutdown)
{
    int64_t nStart = GetTimeMillis();
    // Flush log data to the actual data file on all files that are not in use
//    printOutput(INFO, "db", "CDBEnv::Flush : Flush(%s)%s\n", fShutdown ? "true" : "false", fDbEnvInit ? "" : " database not started");
    if (!envIsInit)
        return;
    {
        cs_db.lock();
        map<string, int>::iterator mi = mapFileUseCount.begin();
        while (mi != mapFileUseCount.end())
        {
            string strFile = (*mi).first;
            int nRefCount = (*mi).second;
           // LogPrint("db", "CDBEnv::Flush : Flushing %s (refcount = %d)...\n", strFile, nRefCount);
            if (nRefCount == 0)
            {
                // Move log data to the dat file
                CloseDb(strFile);
                printOutput(INFO, "CDBEnv::Flush checkpoint\n");
                dbenv.txn_checkpoint(0, 0, 0);
                printOutput(INFO, "CDBEnv::Flush detach\n");
                dbenv.lsn_reset(strFile.c_str(), 0);

                printOutput(INFO, "CDBEnv::Flush closed\n");

                mapFileUseCount.erase(mi++);
            }
            else
                mi++;
        }
      //  LogPrint("db", "CDBEnv::Flush : Flush(%s)%s took %15dms\n", fShutdown ? "true" : "false", fDbEnvInit ? "" : " database not started", GetTimeMillis() - nStart);
        if (fShutdown)
        {
            char** listp;
            if (mapFileUseCount.empty())
            {
                dbenv.log_archive(&listp, DB_ARCH_REMOVE);
                Shutdown();
                boost::filesystem::remove_all(path / "database");
            }
        }
    }
}


// ====================================================================

CDB::CDB(const char *pszFile, const char* pszMode)
    :
    pdb(NULL), activeTxn(NULL)
{
    int ret;

    if (pszFile == NULL)
        return;

    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));

    bool fCreate = strchr(pszMode, 'c');
    unsigned int nFlags = DB_THREAD;
    if (fCreate)
        nFlags |= DB_CREATE;

    {
        //bitdb.cs_db.lock();
        boost::filesystem::path path;
        if (!bitdb.Open(path))
            throw runtime_error("Failed to open database environment.");

        strFile = pszFile;
        ++bitdb.mapFileUseCount[strFile];
        pdb = bitdb.mapDb[strFile];
        if (pdb == NULL)
        {
            pdb = new Db(&bitdb.dbenv, 0);

            printOutput(INFO, "CDB::CDB - Opening db");
            ret = pdb->open(NULL,       // Txn pointer
                            pszFile,    // Filename
                            "main",     // Logical db name
                            DB_BTREE,   // Database type
                            nFlags,     // Flags
                            0);

            printOutput(INFO, "CDB::CDB - Finish db opening");

            if (ret != 0)
            {
                delete pdb;
                pdb = NULL;
                --bitdb.mapFileUseCount[strFile];
                strFile = "";
                throw runtime_error("CBD::CBD - Error, can not open database");
            }

            bitdb.mapDb[strFile] = pdb;
        }
      //  bitdb.cs_db.unlock();
    }
    printOutput(INFO, "CDB::CDB - Finishing constructor");
}

void CDB::Flush()
{
    if (activeTxn)
        return;

    // Flush database activity from memory pool to disk log
    unsigned int nMinutes = 0;
    if (fReadOnly)
        nMinutes = 1;

    bitdb.dbenv.txn_checkpoint(nMinutes ? (100)*1024 : 0, nMinutes, 0);
}

void CDB::Close()
{
    if (!pdb)
        return;
    if (activeTxn)
        activeTxn->abort();
    activeTxn = NULL;
    pdb = NULL;

    Flush();

    {
        //bitdb.cs_db.lock();
        --bitdb.mapFileUseCount[strFile];
    }
}
Dbc* CDB::GetCursor()
{
    if (!pdb)
        return NULL;

    Dbc* pcursor = NULL;
    int ret = pdb->cursor(NULL, &pcursor, 0);
    if (ret != 0)
        return NULL;

    return pcursor;
}

int CDB::ReadAtCursor(Dbc* pcursor, stringstream& ssKey, stringstream& ssValue, unsigned int fFlags)
{
    // Read at cursor
    Dbt datKey;
    if (fFlags == DB_SET || fFlags == DB_SET_RANGE || fFlags == DB_GET_BOTH || fFlags == DB_GET_BOTH_RANGE)
    {
        datKey.set_data(&ssKey);
        datKey.set_size(sizeof(ssKey));
    }
    Dbt datValue;
    if (fFlags == DB_GET_BOTH || fFlags == DB_GET_BOTH_RANGE)
    {
        datValue.set_data(&ssValue);
        datValue.set_size(sizeof(ssValue));
    }

    datKey.set_flags(DB_DBT_MALLOC);
    datValue.set_flags(DB_DBT_MALLOC);
    int ret = pcursor->get(&datKey, &datValue, fFlags);
    if (ret != 0)
        return ret;
    else if (datKey.get_data() == NULL || datValue.get_data() == NULL)
        return 99999;

    // Convert to streams
    ssKey.clear();
    ssKey.write((const char*)datKey.get_data(), datKey.get_size());
    ssValue.clear();
    ssValue.write((const char*)datValue.get_data(), datValue.get_size());

    std::string strKey = ssKey.str();
    std::string strValue = ssValue.str();

    // Clear and free memory
    memset(datKey.get_data(), 0, datKey.get_size());
    memset(datValue.get_data(), 0, datValue.get_size());
    free(datKey.get_data());
    free(datValue.get_data());
    return 0;
}



