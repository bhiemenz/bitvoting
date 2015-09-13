/*=============================================================================

This class is for providing global settings as well as reading command-line
arguments and parameters from given config files.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef SETTINGS_H
#define SETTINGS_H

#include <vector>
#include <string>

// ==========================================================================

/* Major version */
#define CLIENT_VERSION_MAJOR    0

/* Minor version */
#define CLIENT_VERSION_MINOR    0

/* Build revision */
#define CLIENT_VERSION_REVISION 1

namespace Settings
{
    // ----------------------------------------------------------------
    // Constants

    // The version of this client
    const int CLIENT_VERSION =
                           1000000 * CLIENT_VERSION_MAJOR
                         +   10000 * CLIENT_VERSION_MINOR
                         +     100 * CLIENT_VERSION_REVISION;

    // This is only needed by uint256.h
    const int PROTOCOL_VERSION = 1;

    // Default database cache size (in bytes)
    const int64_t DEFAULT_DB_CACHE = 100;

    // Maximum size of a single block file (8MB)
    const int CHAIN_BLOCK_FILE_SIZE = 1024*1024*8;

    // Hash of genesis block
    const std::string HASH_GENESIS_BLOCK = "a71b445873a2f1c0256af99d7fc0ffb117ca2fa16945ebcaa6393b60bdd8e787";

    // Number of bits of generated paillier keys
    const int PAILLIER_BITS = 1024;

    // minimum number of transactions to be mined into one block
    const int MINING_MIN_TRANSACTIONS = 1;

    // leading zero bits for hash-target (difficulty for proof-of-work)
    const int MINING_LEADING_ZEROS = 13;

    // number of nonces consumed at once by a miner-thread
    const int MINING_NONCES_AT_ONCE = 1000;

    // ----------------------------------------------------------------
    // CLI/Config default arguments

    const short defaultPort = 8580;
    const std::string defaultDirectory = ".bitvoting";
    const int defaultFloodTTL = 3;
    const long defaultHeartbeatInterval = 30 * 60 * 1000;
    const long defaultDuplicateValidity = 60 * 1000;
    const long defaultPingInterval = 5 * 60 * 1000;
    const unsigned int defaultMaxConnections = 32;
    const bool defaultLogToConsole = true;
    const bool defaultLogToFile = true;
    const unsigned int defaultMiningThreads = 2;

    // ----------------------------------------------------------------

    // Parse CLI & Config arguments
    bool ParseArguments(int, char**);

    // ----------------------------------------------------------------
    // Getter for CLI / Config arguments:

    std::vector<std::string> GetInitialPeers();
    short GetPort();
    std::string GetDirectory();
    int GetFloodingTTL();
    long GetHeartbeatInterval();
    long GetDuplicateValidity();
    long GetPingInterval();
    unsigned int GetMaxConnections();
    bool GetPrintToConsole();
    bool GetPrintToFile();
    unsigned int GetMiningThreads();
}

#endif // SETTINGS_H
