#include "test_blockchain.h"

#include "paillier/paillier.h"
#include "block.h"
#include "database/blockchaindb.h"
#include "store.h"

#include "transactions/election.h"
#include "transactions/tally.h"
#include "transactions/trustee_tally.h"
#include "transactions/vote.h"

#define MAX_QUESTIONS 3
#define MAX_VOTERS 5
#define MAX_TRUSTEES 2
#define MAX_TRANSACTIONS 3
#define MAX_BLOCKS 5

void random_transaction_election(Transaction** out)
{
    Log::i("(Test) - Creating new txElection...");

    Election* election = new Election();
    election->name = "Random Election";
    election->probableEndingTime = Helper::GenerateRandomUInt();

    int nQuestions = Helper::GenerateRandom(MAX_QUESTIONS - 1) + 1;
    for (int i = 0; i < nQuestions; i++)
        election->questions.push_back(Question("Question #" + std::to_string(i)));

    int nVoters = Helper::GenerateRandom(MAX_VOTERS - 1) + 1;
    for (int i = 0; i < nVoters; i++)
        election->voters.insert(Helper::GenerateRandom160());

    int nTrustees = Helper::GenerateRandom(MAX_TRUSTEES - 1) + 1;
    for (int i = 0; i < nTrustees; i++)
        election->trustees.insert(Helper::GenerateRandom160());

    // create random keys
    paillier_pubkey_t* publicKey = NULL;
    paillier_partialkey_t** privateKeys = NULL;
    paillier_keygen(256, nTrustees, nTrustees, &publicKey, &privateKeys, paillier_get_rand_devurandom);

    election->encPubKey = publicKey;

    paillier_freepartkeysarray(privateKeys, nTrustees);

    TxElection* result = new TxElection();
    result->election = election;

    *out = result;
}

void random_transaction_tally(Transaction** out)
{
    Log::i("(Test) - Creating new txTally...");

    TxTally* result = new TxTally();
    result->election = Helper::GenerateRandom256();
    result->endElection = (Helper::GenerateRandom() > 0.5);
    result->lastBlock = Helper::GenerateRandom256();

    *out = result;
}

void random_transaction_trustee_tally(Transaction** out)
{
    Log::i("(Test) - Creating new txTrusteeTally...");

    TxTrusteeTally* result = new TxTrusteeTally();
    result->tally = Helper::GenerateRandom256();

    int nTrustees = Helper::GenerateRandom(MAX_TRUSTEES - 1) + 1;
    paillier_pubkey_t* publicKey = NULL;
    paillier_partialkey_t** privateKeys = NULL;
    paillier_keygen(256, nTrustees, nTrustees, &publicKey, &privateKeys, paillier_get_rand_devurandom);

    int nVotes = Helper::GenerateRandom(MAX_VOTERS - 1) + 1;
    for (int i = 0; i < nVotes; i++)
    {
        PLAINTEXT_SELECTION choice = PLAINTEXT_SELECTION::FIRST;
        if (Helper::GenerateRandom() > 0.5)
            choice = PLAINTEXT_SELECTION::SECOND;

        paillier_ciphertext_proof_t* cipher = paillier_enc_proof(publicKey, choice, paillier_get_rand_devurandom, NULL);

        int j = Helper::GenerateRandom(nTrustees - 1);
        paillier_partialdecryption_proof_t* proof = paillier_dec_proof(publicKey, privateKeys[j], cipher, paillier_get_rand_devurandom, NULL);

        paillier_freeciphertextproof(cipher);

        TalliedBallots ballots;
        ballots.questionID = Helper::GenerateRandom160();
        ballots.answers = proof;

        result->partialDecryption.insert(ballots);
    }

    paillier_freepartkeysarray(privateKeys, nTrustees);
    paillier_freepubkey(publicKey);

    *out = result;
}

void random_transaction_vote(Transaction** out)
{
    Log::i("(Test) - Creating new txVote...");

    TxVote* result = new TxVote();
    result->election = Helper::GenerateRandom256();

    int nTrustees = Helper::GenerateRandom(MAX_TRUSTEES - 1) + 1;
    paillier_pubkey_t* publicKey = NULL;
    paillier_partialkey_t** privateKeys = NULL;
    paillier_keygen(256, nTrustees, nTrustees, &publicKey, &privateKeys, paillier_get_rand_devurandom);
    paillier_freepartkeysarray(privateKeys, nTrustees);

    int n = Helper::GenerateRandom(MAX_QUESTIONS - 1) + 1;
    for (int i = 0; i < n; i++)
    {
        PLAINTEXT_SELECTION choice = PLAINTEXT_SELECTION::FIRST;
        if (Helper::GenerateRandom() > 0.5)
            choice = PLAINTEXT_SELECTION::SECOND;

        paillier_ciphertext_proof_t* cipher = paillier_enc_proof(publicKey, choice, paillier_get_rand_devurandom, NULL);

        EncryptedBallot ballot;
        ballot.questionID = Helper::GenerateRandom160();
        ballot.answer = cipher;

        result->ballots.insert(ballot);
    }

    paillier_freepubkey(publicKey);

    *out = result;
}

void(*create_transaction[])(Transaction**) = {
            &random_transaction_election, &random_transaction_tally,
            &random_transaction_trustee_tally, &random_transaction_vote
        };

void random_transaction(Transaction** out)
{
    Transaction* result = NULL;
    Role r = Role::KEY_UNKNOWN;

    int i = Helper::GenerateRandom(3);
    create_transaction[i](&result);

    SignKeyPair skp;
    SignKeyStore::genNewSignKeyPair(r, skp);
    result->sign(skp);
    SignKeyStore::removeSignKeyPair(skp.second.GetID()); // revert auto add to db

    *out = result;
}

void random_block(Block** out)
{
    Log::i("(Test) Creating new Block...");

    Block* result = new Block();
    result->header.time = Helper::GenerateRandomUInt();
    result->header.nonce = Helper::GenerateRandomUInt();

    int num = Helper::GenerateRandom(MAX_TRANSACTIONS - 1) + 1;
    for (int i = 0; i < num; i++)
    {
        Transaction* transaction = NULL;
        random_transaction(&transaction);
        result->transactions.insert(transaction);
    }

    *out = result;
}

void test_blockchain()
{
    Log::i("(Test) # Test: Blockchain");

    BlockChainDB::clear();

    uint256 genesisHash(Settings::HASH_GENESIS_BLOCK);
    assert(BlockChainDB::getGenesisBlock() == genesisHash);
    assert(BlockChainDB::getLatestBlockHash() == BlockChainDB::getGenesisBlock());

    assert(!BlockChainDB::containsBlock(Helper::GenerateRandom256()));
    assert(!BlockChainDB::containsTransaction(Helper::GenerateRandom256()));

    Block* temp = NULL;
    assert(BlockChainDB::getLatestBlock(&temp) == BlockChainStatus::BC_IS_EMPTY);

    std::vector<Block*> tempList;
    assert(BlockChainDB::getAllBlocks(genesisHash, tempList) == BlockChainStatus::BC_OK);
    assert(BlockChainDB::getAllBlocks(genesisHash, Helper::GenerateRandom256(), tempList) == BlockChainStatus::BC_NOT_FOUND);
    assert(tempList.size() == 0);

    std::vector<Block*> list;
    uint256 lastHash = genesisHash;
    int n = Helper::GenerateRandom(MAX_BLOCKS);
    for (int i = 0; i < n; i++)
    {
        if (lastHash != genesisHash)
        {
            Block* last = NULL;
            assert(BlockChainDB::getBlock(lastHash, &last) == BlockChainStatus::BC_OK);
            assert(last->getHash() == lastHash);
            delete last;
        }

        // add a new block
        Block* block = NULL;
        random_block(&block);
        block->header.hashPrevBlock = lastHash;

        SignKeyPair skp;
        SignKeyStore::genNewSignKeyPair(Role::KEY_MINING, skp);
        block->sign(skp);
        SignKeyStore::removeSignKeyPair(skp.second.GetID()); // revert auto add to db

        list.push_back(block);
        lastHash = block->getHash();

        assert(BlockChainDB::addBlock(block) == BlockChainStatus::BC_OK);
        assert(BlockChainDB::containsBlock(lastHash));

        std::vector<Transaction*> vector(block->transactions.begin(), block->transactions.end());
        int t = Helper::GenerateRandom(vector.size() - 1);

        assert(BlockChainDB::containsTransaction(vector[t]->getHash()));
    }

    Block* last = NULL;
    assert(BlockChainDB::getLatestBlock(&last) == BlockChainStatus::BC_OK);
    assert(last->getHash() == lastHash);
    delete last;

    // add invalid
    Block* block = NULL;
    random_block(&block);
    block->header.hashPrevBlock = Helper::GenerateRandom256();
    assert(BlockChainDB::addBlock(block) == BlockChainStatus::BC_INVALID_BLOCK);
    delete block;

    // cutoff testen
    assert(BlockChainDB::cutOffAfter(Helper::GenerateRandom256()) == BlockChainStatus::BC_NOT_FOUND);
    assert(BlockChainDB::cutOffAfter(lastHash) == BlockChainStatus::BC_OK);
    int c = Helper::GenerateRandom(list.size() - 1);
    uint256 cHash = list[c]->getHash();
    assert(BlockChainDB::cutOffAfter(cHash) == BlockChainStatus::BC_OK);
    assert(BlockChainDB::getLatestBlockHash() == cHash);
    assert(BlockChainDB::containsBlock(cHash));
    int t = Helper::GenerateRandom(list[c]->transactions.size() - 1);
    std::vector<Transaction*> ts(list[c]->transactions.begin(),
                                 list[c]->transactions.end());
    assert(BlockChainDB::containsTransaction(ts[t]->getHash()));

    // free everything
    BOOST_FOREACH(Block* b, list)
    {
        BOOST_FOREACH(Transaction* t, b->transactions)
        {
            switch (t->getType())
            {
            case TxType::TX_ELECTION:
                delete ((TxElection*)t)->election->encPubKey;
                break;
            case TxType::TX_TRUSTEE_TALLY:
                BOOST_FOREACH(TalliedBallots b, ((TxTrusteeTally*)t)->partialDecryption)
                    delete b.answers;
                break;
            case TxType::TX_VOTE:
                BOOST_FOREACH(EncryptedBallot b, ((TxVote*)t)->ballots)
                    delete b.answer;
                break;
            default:
                break;
            }

            delete t;
        }

        delete b;
    }

    BlockChainDB::clear();
}
