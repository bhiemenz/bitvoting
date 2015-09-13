#include "helper.h"
#include "settings.h"

#include <stdio.h>
#include <ctime>
#include <stdexcept>

#include <openssl/rand.h>

#include <boost/thread.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// ================================================================

static boost::filesystem::path logPath;

void
Log::log_(LogCategory c, std::string s, va_list args)
{
    // format arguments into buffer
    char buffer[512];
    vsprintf(buffer, s.c_str(), args);

    // print to console
    if (Settings::GetPrintToConsole())
    {
        // set target stream
        FILE* target = stdout;
        if (c == LogCategory::ERROR)
            target = stderr;

        Log::print(target, c, buffer);
    }

    // print to log file
    if (!Settings::GetPrintToFile())
        return;

    if (logPath.empty())
    {
        logPath = Settings::GetDirectory();
        logPath /= "log.txt";
    }

    // open log file
    FILE* logFile = fopen(logPath.string().c_str(), "a");

    if (!logFile)
        return;

    setbuf(logFile, NULL); // unbuffered

    Log::print(logFile, c, buffer);

    fclose(logFile);
}

// ----------------------------------------------------------------

void
Log::print(FILE* target, LogCategory c, const char* s)
{
    // print header, data and new line
    fprintf(target, "%s ", Log::getHeader(c).c_str());
    fprintf(target, s);
    fprintf(target, "\n");
}

// ----------------------------------------------------------------

std::string
Log::getHeader(LogCategory c)
{
    // format category and current time
    return ("[" + Log::getCategoryString(c) + "] " + Helper::FormatTime("%Y-%m-%d %H:%M:%S", time(NULL)));
}

// ----------------------------------------------------------------

std::string
Log::getCategoryString(LogCategory c)
{
    switch (c)
    {
        case INFO: return "INF";
        case WARNING: return "WRN";
        case ERROR: return "ERR";
        default: break;
    }

    return "---";
}

// ================================================================

std::string
Helper::FormatTime(const char* pszFormat, int64_t nTime)
{
    // std::locale takes ownership of the pointer
    std::locale loc(std::locale::classic(), new boost::posix_time::time_facet(pszFormat));
    std::stringstream ss;
    ss.imbue(loc);
    ss << boost::posix_time::from_time_t(nTime);
    return ss.str();
}

// ----------------------------------------------------------------

static boost::filesystem::path pathCached;

const boost::filesystem::path&
Helper::GetDataDir()
{
    boost::filesystem::path &path = pathCached;

    // check if path is already cached
    if (!path.empty())
        return path;

    path = Settings::GetDirectory();

    // create directory if necessary
    boost::filesystem::create_directories(path);

    pathCached = path;
    return path;
}

// ----------------------------------------------------------------

const boost::filesystem::path
Helper::GetHomeDir()
{
    // determine user home directory
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        return boost::filesystem::path("/");
    else
        return boost::filesystem::path(pszHome);
}

// ----------------------------------------------------------------

void
Helper::Sleep(long millis)
{
    boost::this_thread::sleep(boost::posix_time::milliseconds(millis));
}

// ----------------------------------------------------------------

const boost::uuids::uuid
Helper::GenerateUUID()
{
    // create new UUID
    return boost::uuids::random_generator()();
}

// ----------------------------------------------------------------

static boost::uuids::uuid guidCached;

const boost::uuids::uuid
Helper::GetGUID()
{
    if (!guidCached.is_nil())
        return guidCached;

    // create new GUID for this session
    guidCached = Helper::GenerateUUID();

    return guidCached;
}

// ----------------------------------------------------------------

long long
Helper::GetUNIXTimestamp()
{
    //std::time_t result = std::time(nullptr);

    /*std::cout << std::asctime(std::localtime(&result))
              << result << " seconds since the Epoch\n";*/

    /* int64_t
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_milliseconds();*/

    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

// ----------------------------------------------------------------

const std::string
Helper::GetAddressString(const boost::asio::ip::tcp::endpoint &endpoint)
{
    return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
}

// ----------------------------------------------------------------

const boost::asio::ip::tcp::endpoint
Helper::GetEndpoint(std::string value)
{
    std::vector<std::string> parts;
    boost::split(parts, value, boost::is_any_of(":"));

    if (parts.size() != 2)
        throw std::invalid_argument("Could not parse " + value + " as an IPv4 address!");

    // parse address
    boost::asio::ip::address_v4 ipaddr  =
            boost::asio::ip::address_v4::from_string(parts[0]);
    short port = atoi(parts[1].c_str());

    return boost::asio::ip::tcp::endpoint(ipaddr, port);
}

// ----------------------------------------------------------------

bool
Helper::CreateDirectories(const boost::filesystem::path& p)
{
    try
    {
        return boost::filesystem::create_directories(p);
    }
    catch (boost::filesystem::filesystem_error)
    {
        if (!boost::filesystem::exists(p) || !boost::filesystem::is_directory(p))
            throw;
    }

    // create_directory didn't create the directory, it had to have existed already
    return false;
}

// ----------------------------------------------------------------

uint160
Helper::GenerateRandom160()
{
    uint160 result;
    RAND_bytes((unsigned char*)&result, sizeof(result));
    return result;
}

// ----------------------------------------------------------------

uint256
Helper::GenerateRandom256()
{
    uint256 result;
    RAND_bytes((unsigned char*)&result, sizeof(result));
    return result;
}

// ----------------------------------------------------------------

unsigned int
Helper::GenerateRandomUInt()
{
    static std::default_random_engine *generator = new std::default_random_engine();
    unsigned int uintmax = std::numeric_limits<unsigned int>::max();
    std::uniform_int_distribution<unsigned int> distribution(0, uintmax);
    return distribution(*generator);
}

// ----------------------------------------------------------------

double
Helper::GenerateRandom()
{
    // x is in [0,1[
    return rand() / static_cast<double>(RAND_MAX);
}

// ----------------------------------------------------------------

int
Helper::GenerateRandom(int min, int max)
{
    double x = Helper::GenerateRandom();

    // [0,1[ * (max - min) + min is in [min,max[
    return min + static_cast<int>( x * (max - min + 1) );
}

// ----------------------------------------------------------------

int
Helper::GenerateRandom(int max)
{
    return Helper::GenerateRandom(0, max);
}
