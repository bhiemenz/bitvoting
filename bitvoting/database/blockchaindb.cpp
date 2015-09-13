#include "database/blockchaindb.h"
#include "export.h"

#include <istream>
#include <ostream>

#include <boost/foreach.hpp>

BITVOTING_CLASS_EXPORT_IMPLEMENT(Signable)
BITVOTING_CLASS_EXPORT_IMPLEMENT(Transaction)
#include "transactions/vote.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(TxVote)
#include "transactions/election.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(TxElection)
#include "transactions/tally.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(TxTally)
#include "transactions/trustee_tally.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(TxTrusteeTally)
#include "block.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(Block)

// ================================================================

void BlockChainDB::loadMetaData()
{
    this->Read("latestBlock", this->latestBlock);
    this->Read("currentLocation", this->currentLocation);
}

// ----------------------------------------------------------------

void BlockChainDB::saveMetaData()
{
    this->Write("latestBlock", this->latestBlock);
    this->Write("currentLocation", this->currentLocation);
}

// ----------------------------------------------------------------

bool BlockChainDB::saveBlockInfo(const uint256 &bHash, BlockInfo &bInfo)
{
    return this->Write(std::make_pair(std::string("bl"), bHash), bInfo);
}

// ----------------------------------------------------------------

bool BlockChainDB::getBlockInfo(const uint256 &bHash, BlockInfo &bInfoOut)
{
    return this->Read(std::make_pair(std::string("bl"), bHash), bInfoOut);
}

// ----------------------------------------------------------------

bool BlockChainDB::hasBlockInfo(const uint256 &bHash)
{
    return this->Exists(std::make_pair(std::string("bl"), bHash));
}

// ----------------------------------------------------------------

bool BlockChainDB::removeBlockInfo(const uint256 &bHash)
{
    return this->Erase(std::make_pair(std::string("bl"), bHash));
}

// ----------------------------------------------------------------

bool BlockChainDB::saveLocator(const uint256 &hash, Locator &loc)
{
    return this->Write(std::make_pair(std::string("l"), hash), loc);
}

// ----------------------------------------------------------------

bool BlockChainDB::getLocator(const uint256 &hash, Locator &locOut)
{
    return this->Read(std::make_pair(std::string("l"), hash), locOut);
}

// ----------------------------------------------------------------

bool BlockChainDB::hasLocator(const uint256 &hash)
{
    return this->Exists(std::make_pair(std::string("l"), hash));
}

// ----------------------------------------------------------------

bool BlockChainDB::removeLocator(const uint256 &hash)
{
    return this->Erase(std::make_pair(std::string("l"), hash));
}

// ----------------------------------------------------------------

boost::filesystem::path BlockChainDB::getPath(unsigned int id)
{
    // (example: blockfile_0006072612.bin)
    std::ostringstream os;
    os << "blockfile_" << std::setfill('0') << std::setw(10) << id << std::string(".bin");
    boost::filesystem::path filename = boost::filesystem::path(os.str());
    return boost::filesystem::path(PATH_DATABASE_DIR / filename);
}

// ================================================================

uint256& BlockChainDB::getGenesisBlock()
{
    return BlockChainDB::GetInstance().genesisBlock;
}

// ----------------------------------------------------------------

BlockChainStatus BlockChainDB::addBlock(Block *block)
{
    BlockChainDB& db = BlockChainDB::GetInstance();

    boost::mutex::scoped_lock(db.mutex);

    // check if this block is in order
    if (db.latestBlock != block->header.hashPrevBlock)
        return BC_INVALID_BLOCK;

    // check if new file has to be generated
    boost::filesystem::path blockfile = db.getPath(db.currentLocation.id);

    if(!boost::filesystem::exists(blockfile))
    {
        // create new empty blockfile
        std::ofstream stream(blockfile.c_str(), std::ios_base::out);
        stream.close();

        db.currentLocation.blockPos = 0;
    }

    // open blockfile
    std::ofstream stream(blockfile.c_str(),
                         std::ios_base::out | std::ios_base::binary |
                         std::ios_base::app | std::ios_base::ate);

    if(!stream.is_open())
        return BC_FILE_CORRUPT;

    // get ending position in blockfile
    std::streampos position = stream.tellp();
    if(position < 0 || position != db.currentLocation.blockPos)
        return BC_FILE_CORRUPT;

    // initialize block info for new block
    BlockInfo bInfo(db.currentLocation, block->header.hashPrevBlock);

    // store meta information about block
    uint256 hash = block->getHash();
    db.saveBlockInfo(hash, bInfo);
    db.latestBlock = hash;

    // store meta information about each transaction
    BOOST_FOREACH(Transaction* transaction, block->transactions)
            db.saveLocator(transaction->getHash(), db.currentLocation);

    try
    {
        // save block to disk
        boost::archive::binary_oarchive oa(stream);
        oa << block;
    }
    catch(...)
    {
        return BC_FILE_CORRUPT;
    }

    // get new current position
    db.currentLocation.blockPos = stream.tellp();
    stream.close();

    // check if new blockfile should be started next time
    if (db.currentLocation.blockPos > Settings::CHAIN_BLOCK_FILE_SIZE)
        db.currentLocation.id++;

    // save current meta data
    db.saveMetaData();

    return BC_OK;
}

// ----------------------------------------------------------------

bool BlockChainDB::containsBlock(const uint256 &bHash)
{
    BlockChainDB& db = BlockChainDB::GetInstance();

    return db.hasBlockInfo(bHash);
}

// ----------------------------------------------------------------

BlockChainStatus BlockChainDB::getBlock(const uint256 &bHash, Block **blockOut)
{
    BlockChainDB& db = BlockChainDB::GetInstance();

    // check for block info
    BlockInfo blockInfo;
    if (!db.getBlockInfo(bHash, blockInfo))
        return BC_NOT_FOUND;

    // get block
    return BlockChainDB::getBlock(blockInfo.locator, blockOut);
}

// ----------------------------------------------------------------

BlockChainStatus BlockChainDB::getBlock(const Locator &location, Block **blockOut)
{
    BlockChainDB& db = BlockChainDB::GetInstance();

    boost::mutex::scoped_lock(db.mutex);

    // open block file
    boost::filesystem::path blockfile = db.getPath(location.id);
    if(!boost::filesystem::exists(blockfile))
        return BC_NOT_FOUND;

    // open stream
    std::ifstream stream(blockfile.c_str(), std::ios_base::in | std::ios_base::binary);

    // check if block file exist and is readable
    if(!stream.is_open())
        return BC_FILE_CORRUPT;

    try
    {
        // set position in file
        stream.seekg(location.blockPos);
        boost::archive::binary_iarchive oa(stream);
        oa >> *blockOut;
        stream.close();
    }
    catch(...)
    {
        return BC_FILE_CORRUPT;
    }

    return BC_OK;
}

// ----------------------------------------------------------------

BlockChainStatus BlockChainDB::getBlockByTransaction(const uint256 &tHash, Block **blockOut)
{
    BlockChainDB& db = BlockChainDB::GetInstance();

    // load block position on disk from database
    Locator location;
    if (!db.getLocator(tHash, location))
        return BC_NOT_FOUND;

    // get block
    return BlockChainDB::getBlock(location, blockOut);
}

// ----------------------------------------------------------------

BlockChainStatus BlockChainDB::getLatestBlock(Block **blockOut)
{
    BlockChainDB& db = BlockChainDB::GetInstance();

    if (db.genesisBlock == db.latestBlock)
        return BC_IS_EMPTY;

    // get block
    return BlockChainDB::getBlock(db.latestBlock, blockOut);
}

// ----------------------------------------------------------------

uint256 BlockChainDB::getLatestBlockHash()
{
    return BlockChainDB::GetInstance().latestBlock;
}

bool BlockChainDB::containsTransaction(const uint256 &tHash)
{
    BlockChainDB& db = BlockChainDB::GetInstance();

    return db.hasLocator(tHash);
}

// ----------------------------------------------------------------

BlockChainStatus BlockChainDB::getTransaction(const uint256 &tHash, Transaction **tOut)
{
    // load responsible block for transaction
    Block* block = NULL;
    BlockChainStatus result = BlockChainDB::getBlockByTransaction(tHash, &block);

    if (result != BC_OK)
        return result;

    // get transaction from block
    BOOST_FOREACH(Transaction* current, block->transactions)
    {
        if(tHash != current->getHash())
            continue;

        *tOut = current;
        return BC_OK;
    }

    return BC_NOT_FOUND;
}

// ----------------------------------------------------------------

BlockChainStatus BlockChainDB::getAllBlocks(const uint256 &start, std::vector<Block*> &blocksOut)
{
    BlockChainDB& db = BlockChainDB::GetInstance();

    return BlockChainDB::getAllBlocks(start, db.latestBlock, blocksOut);
}

// ----------------------------------------------------------------

BlockChainStatus BlockChainDB::getAllBlocks(const uint256 &start, const uint256 &end, std::vector<Block*> &blocksOut)
{
    BlockChainDB& db = BlockChainDB::GetInstance();

    blocksOut.clear();

    // only get first block if not genesis block
    Block* startBlock = NULL;
    BlockChainStatus result;
    if (start != db.genesisBlock)
    {
        // get start block
        result = BlockChainDB::getBlock(start, &startBlock);

        if (result != BC_OK)
            return result;
    }

    // go from back to front
    uint256 hash = end;
    while (hash != start)
    {
        // get current block
        Block* block = NULL;
        result = BlockChainDB::getBlock(hash, &block);

        if (result != BC_OK)
            return result;

        // add to result
        blocksOut.insert(blocksOut.begin(), block);

        hash = block->header.hashPrevBlock;
    }

    // only add start block if not genesis block
    if (startBlock)
        blocksOut.insert(blocksOut.begin(), startBlock);

    return BC_OK;
}

// ----------------------------------------------------------------

BlockChainStatus BlockChainDB::cutOffAfter(const uint256 &bHash)
{
    BlockChainDB& db = BlockChainDB::GetInstance();

    // nothing to do
    if (bHash == db.latestBlock)
        return BC_OK;

    // get info for first block
    BlockInfo startInfo;
    if (!db.getBlockInfo(bHash, startInfo))
        return BC_NOT_FOUND;

    // get all blocks that will be cut off
    std::vector<Block*> blocks;
    BlockChainStatus result = BlockChainDB::getAllBlocks(bHash, blocks);

    if (result != BC_OK)
        return result;

    // check if startBlock is the last block inside its blockfile
    unsigned int truncate = -1;

    BlockInfo secondInfo;
    if (!db.getBlockInfo(blocks[1]->getHash(), secondInfo))
        return BC_NOT_FOUND;

    // check if second block is inside the same file as first
    if (secondInfo.locator.id == startInfo.locator.id)
        truncate = secondInfo.locator.blockPos;

    // remove all meta data
    for (int i = 1; i < blocks.size(); i++)
    {
        Block* block = blocks[i];

        // remove all transaction meta data
        BOOST_FOREACH(Transaction* transaction, block->transactions)
        {
            db.removeLocator(transaction->getHash());
        }

        // remove block meta data
        db.removeBlockInfo(block->getHash());
    }

    // remove superfluous block files
    for (int i = db.currentLocation.id; i > startInfo.locator.id; i--)
    {
        boost::filesystem::path blockfile = db.getPath(i);
        if (!boost::filesystem::remove(blockfile))
            return BC_FILE_CORRUPT;
    }

    // update current, last
    db.latestBlock = bHash;
    db.currentLocation = startInfo.locator;

    // check if new blockfile should be started next time
    if (db.currentLocation.blockPos > Settings::CHAIN_BLOCK_FILE_SIZE)
        db.currentLocation.id++;

    // save new meta data
    db.saveMetaData();

    if (truncate == -1)
        return BC_OK;

    // cutoff inside last file
    boost::filesystem::path blockfile = db.getPath(startInfo.locator.id);
    std::ifstream stream(blockfile.c_str(), std::ios_base::in | std::ios_base::binary);

    if (!stream.is_open())
        return BC_FILE_CORRUPT;

    // truncate current block file (buffer leftovers)
    std::vector<char> buffer;
    buffer.resize(truncate);
    stream.read((char*)(&buffer[0]), truncate);
    stream.close();

    // overwrite old file with buffer
    std::ofstream ostream(blockfile.c_str(), std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    ostream.write((char*)(&buffer[0]), truncate);
    ostream.close();

    return BC_OK;
}

// ----------------------------------------------------------------

void BlockChainDB::print()
{
    BlockChainDB& db = BlockChainDB::GetInstance();

    boost::mutex::scoped_lock(db.mutex);

    Log::i("(Blockchain) Genesis Hash:\t %s", db.genesisBlock.GetHex().c_str());
    Log::i("(Blockchain) Latest Block:\t %s", db.latestBlock.GetHex().c_str());
    Log::i("(Blockchain) Current Position:\t %d (%d)", db.currentLocation.id, db.currentLocation.blockPos);

    // get start block
    uint256 hash = db.latestBlock;
    while (hash != db.genesisBlock)
    {
        Log::i("");

        BlockInfo info;
        db.getBlockInfo(hash, info);

        Log::i("(Blockchain) Block at %d (%d)", info.locator.id, info.locator.blockPos);

        Block* block = NULL;
        BlockChainDB::getBlock(info.locator, &block);

        Log::i("(Blockchain) INFO> Current: %s - Previous: %s",
               hash.GetHex().c_str(),
               info.preHash.GetHex().c_str());
        Log::i("(Blockchain) ACTU> Current: %s - Previous: %s",
               block->getHash().GetHex().c_str(),
               block->header.hashPrevBlock.GetHex().c_str());

        hash = block->header.hashPrevBlock;
    }
}

// ----------------------------------------------------------------

void BlockChainDB::clear()
{
    BlockChainDB& db = BlockChainDB::GetInstance();

    boost::mutex::scoped_lock(db.mutex);

    // remove superfluous block files
    for (int i = db.currentLocation.id; i >= 0; i--)
    {
        boost::filesystem::path blockfile = db.getPath(i);
        boost::filesystem::remove(blockfile);
    }

    // clear database
    leveldb::Iterator* iter = db.NewIterator();
    for (iter->SeekToFirst(); iter->Valid(); iter->Next())
    {
        try
        {
            // read next item in database
            std::stringstream sstr;
            sstr << iter->key().ToString();

            boost::archive::text_iarchive ia(sstr);
            std::pair<std::string, uint256> key;
            ia >> key;

            db.Erase(key);
        }
        catch(...)
        {
            continue;
        }
    }

    // reset meta data
    db.latestBlock = db.genesisBlock;
    db.currentLocation.id = 0;
    db.currentLocation.blockPos = 0;

    db.saveMetaData();
}
