/*=============================================================================

This class handles the block chain. The block chain contains every block, which
was accepted by the network. Information about each block and its transaction
are stored in a database, the blocks themselves are saved to a file (called
block file).

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef BITVOTING_BLOCKCHAINDB_H
#define BITVOTING_BLOCKCHAINDB_H

#include "block.h"
#include "transaction.h"
#include "settings.h"
#include "bitcoin/uint256.h"
#include "database/leveldbwrapper.h"

#include <utility>

#include <boost/filesystem/path.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>

// ==========================================================================

// define base path to block chain directory
#define PATH_DATABASE_DIR boost::filesystem::path(Settings::GetDirectory()) / "databases" / "blockchain"

enum BlockChainStatus
{
    // everything ok
    BC_OK,
    // block file not found (wrong parameter etc)
    BC_NOT_FOUND,
    // could not open block file
    BC_FILE_CORRUPT,
    // block chain is empty/contains only genesis block
    BC_IS_EMPTY,
    // when trying to insert an unlinked block
    BC_INVALID_BLOCK
};

// ==========================================================================

// Provide information to find a certain block or transation in the
// block chain by saving the block file id, the block/transaction is
// stored on disk and the position within this file. For every transaction,
// the block locator is copied, because to locate a transaction in
// the block chain, the responsible block must be found first.
struct Locator
{
    // Identifier for block file on disk
    unsigned int id;

    // Identify position of block within block file
    long long int blockPos;

    // ----------------------------------------------------------------

    Locator (unsigned int fileID = 0, std::streampos posIn = 0)
    {
        this->id = fileID;
        this->blockPos = (long long int) posIn;
    }

    // ----------------------------------------------------------------

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & id;
        a & blockPos;
    }
};

// ==========================================================================

// Additional to information about the location of a block/transaction,
// the hash of its predecessor is stored for every block
struct BlockInfo
{
    // Information to locate block on disk (block file)
    Locator locator;

    // Hash of predecessor block
    uint256 preHash;

    // ----------------------------------------------------------------

    BlockInfo() {}
    BlockInfo(Locator locator, uint256 preHash):
        locator(locator),
        preHash(preHash) {}

    // ----------------------------------------------------------------

    template <typename Archive>
    void serialize(Archive& a, const unsigned int)
    {
        a & locator;
        a & preHash;
    }
};

// ==========================================================================

class BlockChainDB : public LevelDBWrapper
{
private:
    // ----------------------------------------------------------------
    // Singleton:

    BlockChainDB(const boost::filesystem::path databaseDir):
        LevelDBWrapper(databaseDir, Settings::DEFAULT_DB_CACHE)
    {
        uint256 hashGenesis(Settings::HASH_GENESIS_BLOCK);

        // check if database was already initialized
        if (!this->Read("genesisBlock", this->genesisBlock))
        {
            // if not, save initial settings
            this->genesisBlock = hashGenesis;
            this->latestBlock = hashGenesis;

            this->Write("genesisBlock", this->genesisBlock);
            this->saveMetaData();

            return;
        }

        // check for consistency
        if(this->genesisBlock != hashGenesis)
            throw std::runtime_error("genesis hash initialization error");

        this->loadMetaData();
    }

    BlockChainDB(BlockChainDB const&)    = delete;
    void operator=(BlockChainDB const&)  = delete;

    static BlockChainDB& GetInstance()
    {
        static BlockChainDB instance(PATH_DATABASE_DIR / "index");
        return instance;
    }

protected:
    boost::mutex mutex;

    // --- data ---
    uint256 genesisBlock;
    uint256 latestBlock;
    Locator currentLocation;

    // ----------------------------------------------------------------

    // Load database meta data
    void loadMetaData();

    // Save database meta data
    void saveMetaData();

    // Write block information (locator and hash of predecessor block)
    bool saveBlockInfo(const uint256 &, BlockInfo &);

    // Read block information (locator and hash of predecessor block)
    bool getBlockInfo(const uint256 &, BlockInfo &);

    // Read block information (locator and hash of predecessor block)
    bool hasBlockInfo(const uint256 &);

    // Erase block information from database
    bool removeBlockInfo(const uint256 &);

    // Write locator to database
    bool saveLocator(const uint256 &, Locator &);

    // Read locator from datase
    bool getLocator(const uint256 &, Locator &);

    // Read locator from datase
    bool hasLocator(const uint256 &);

    // Read locator from datase
    bool removeLocator(const uint256 &);

    // Get blockfile path for the given locator id
    boost::filesystem::path getPath(unsigned int);

public:
    // Get the hash of the genesis block
    static uint256& getGenesisBlock();

    // Append new block to block chain (write to disk)
    static BlockChainStatus addBlock(Block *);

    // Checks if the given block is in the blockchain
    static bool containsBlock(const uint256 &);

    // Get block of a given hash
    static BlockChainStatus getBlock(const uint256 &, Block **);

    // Load block from block chain using its disk block position
    static BlockChainStatus getBlock(const Locator &, Block **);

    // Load latest block from chain (newest block)
    static BlockChainStatus getLatestBlock(Block **);

    // Get only hash of latest block in chain
    static uint256 getLatestBlockHash();

    // Get block of a given transaction
    static BlockChainStatus getBlockByTransaction(const uint256 &, Block **);

    static bool containsTransaction(const uint256 &);

    // Load a certain transaction from block chain using its hash
    static BlockChainStatus getTransaction(const uint256 &, Transaction **);

    // Load a block and all its successors until the latest block is reached.
    // Note: The given block is part of the returning collection
    static BlockChainStatus getAllBlocks(const uint256 &, std::vector<Block*> &);

    // Load a block and all its successors until a given target is reached.
    // Note: Both start and end block will be part of that list
    static BlockChainStatus getAllBlocks(const uint256 &, const uint256 &, std::vector<Block*> &);

    // Delete all blocks after a given block
    // Note: The given block will not be deleted, but all blocks after it
    static BlockChainStatus cutOffAfter(const uint256 &);

    // print all contents of the blockchain
    static void print();

    // clears the entire blockchain
    static void clear();
};

#endif
