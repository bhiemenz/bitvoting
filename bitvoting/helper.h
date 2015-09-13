/*=============================================================================

Provides helper and logging functionalities.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef P2PVOTING_HELPER_H
#define P2PVOTING_HELPER_H

#include "bitcoin/uint256.h"
#include "paillier/paillier.h"

#include <string>
#include <sstream>
#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/asio.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

// ==========================================================================

enum LogCategory
{
    UNKNOWN,
    INFO,
    WARNING,
    ERROR
};

// ==========================================================================

class Log
{
public:
    // Wrapper for logging INFO
    static inline void i(std::string s, ...)
    {
        va_list args;
        va_start(args, s.c_str());
        Log::log_(LogCategory::INFO, s, args);
        va_end(args);
    }

    // Wrapper for logging WARNING
    static inline void w(std::string s, ...)
    {
        va_list args;
        va_start(args, s.c_str());
        Log::log_(LogCategory::WARNING, s, args);
        va_end(args);
    }

    // Wrapper for logging ERROR
    static inline void e(std::string s, ...)
    {
        va_list args;
        va_start(args, s.c_str());
        Log::log_(LogCategory::ERROR, s, args);
        va_end(args);
    }

    // Generic logging wrapper
    static inline void log(LogCategory c, std::string s, ...)
    {
        va_list args;
        va_start(args, s.c_str());
        Log::log_(c, s, args);
        va_end(args);
    }

private:
    // Internal logging helper method
    static void log_(LogCategory, std::string, va_list);

    // Print log to the given stream
    static void print(FILE*, LogCategory, const char*);

    // Get logging header
    static std::string getHeader(LogCategory);

    // Get category as string
    static std::string getCategoryString(LogCategory);
};

// ================================================================

class Helper
{
public:
    // Format time in the given format
    static std::string FormatTime(const char*, int64_t);

    // Obtain the data directory for bitvoting
    static const boost::filesystem::path& GetDataDir();

    // Obtain user's home directory
    static const boost::filesystem::path GetHomeDir();

    // Sleep for X msec
    static void Sleep(long);

    // Generate a unique UUID
    static const boost::uuids::uuid GenerateUUID();

    // Obtain the client's session GUID
    static const boost::uuids::uuid GetGUID();

    // Get current UNIX timestamp (sec)
    static long long GetUNIXTimestamp();

    // Format endpoint to string
    static const std::string GetAddressString(const boost::asio::ip::tcp::endpoint&);

    // Parse string to endpoint
    static const boost::asio::ip::tcp::endpoint GetEndpoint(std::string);

    // Recursively create given directory
    static bool CreateDirectories(const boost::filesystem::path&);

    // Generate a random uin160
    static uint160 GenerateRandom160();

    // Generate a random uin256
    static uint256 GenerateRandom256();

    // Generate a random unsigned integer
    static unsigned int GenerateRandomUInt();

    // Generate a random double in [0,1[
    static double GenerateRandom();

    // Generate a random number in [A,B]
    static int GenerateRandom(int, int);

    // Generate a random number in [0,A]
    static int GenerateRandom(int);

    // Serialize a given object to file
    template<typename T>
    static void SaveToFile(T&, std::string, bool = false);

    // Deserialize a given file to a specific object
    template<typename T>
    static void LoadFromFile(std::string, T&, bool = false);

    // Serialize a given object to file
    template<typename T>
    static void SaveToFile(T*, std::string, bool = false);

    // Deserialize a given file to a specific object
    template<typename T>
    static void LoadFromFile(std::string, T**, bool = false);
};

// ==========================================================================
// These need to be defined here in the header!

template<typename T>
void
Helper::SaveToFile(T& data, std::string file, bool binary)
{
    // Wrapper
    Helper::SaveToFile(&data, file, binary);
}

// ----------------------------------------------------------------

template<typename T>
void
Helper::LoadFromFile(std::string file, T& data, bool binary)
{
    // Wrapper
    T* temp = NULL;
    Helper::LoadFromFile(file, &temp, binary);
    data = *temp;
    delete temp;
}

// ----------------------------------------------------------------

template<typename T>
void
Helper::SaveToFile(T* data, std::string file, bool binary)
{
    std::ofstream ofs(file.c_str());
    if (binary)
    {
        // serialize to binary
        boost::archive::binary_oarchive oa(ofs);
        oa << data;
        return;
    }

    // serialize to text
    boost::archive::text_oarchive oa(ofs);
    oa << data;
}

// ----------------------------------------------------------------

template<typename T>
void
Helper::LoadFromFile(std::string file, T** data, bool binary)
{
    std::ifstream ifs(file.c_str());

    if (binary)
    {
        // parse from binary
        boost::archive::binary_iarchive ia(ifs);
        ia >> *data;
        return;
    }

    // parse from text
    boost::archive::text_iarchive ia(ifs);
    ia >> *data;
}

#endif //P2PVOTING_HELPER_H
