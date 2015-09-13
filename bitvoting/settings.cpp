#include "settings.h"
#include "helper.h"

#include <iostream>
#include <fstream>
#include <iterator>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

// ================================================================

namespace po = boost::program_options;

po::variables_map vm;

// helper function to simplify the main part.
template<class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, ", "));
    return os;
}

// ================================================================

bool
Settings::ParseArguments(int argc, char** argv)
{
    short port;

    // Declare a group of options that will be
    // allowed only on command line
    po::options_description generic("Generic options");
    generic.add_options()
            ("help", "produce help message")
            ("data-dir,d", po::value<std::string>(),
             "path in home to the configuration directory");

    // Declare a group of options that will be
    // allowed both on command line and in
    // config file
    po::options_description config("Configuration");
    config.add_options()
            ("port,p", po::value<short>(&port)->default_value(8580),
             "port on which peer should listen for incoming connections")
            ("flooding-ttl,t", po::value<int>(),
             "determines the TTL of flooded messages (default 3)")
            ("max-connections,c", po::value<unsigned int>(),
             "maximum connections that will be allowed (default 32)")
            ("log-cli", po::value<bool>(),
             "if application should log to console (default yes)")
            ("log-file", po::value<bool>(),
             "if application should log to log file (default yes)")
            ("threads-mining", po::value<bool>(),
             "how many threads should be used for mining (default 2)");

    // Hidden options, will be allowed both on command line and
    // in config file, but will not be shown to the user.
    po::options_description hidden("Hidden options");
    hidden.add_options()
            ("connect", po::value< std::vector<std::string> >()->composing(),
             "peers that should initially be connected to");

    // options, that are only accessible in the config file
    po::options_description conly("Configuration file only");
    conly.add_options()
            ("heartbeat", po::value<long>(),
             "interval in which heartbeat messages should be sent (msec, default 30min)")
            ("duplicate-validity", po::value<long>(),
             "determines how long messages should be remembered (valid) for duplicate checking (msec, default 1min)")
            ("ping-interval", po::value<long>(),
             "interval in which neighborhood should be pinged for new connections (msec, default 5min)");

    // assemble options
    po::options_description cmdline_options;
    cmdline_options.add(generic).add(config).add(hidden);

    po::options_description config_file_options;
    config_file_options.add(config).add(hidden).add(conly);

    po::options_description visible("Allowed options");
    visible.add(generic).add(config);

    po::positional_options_description p;
    p.add("connect", -1);

    // actual parsing
    po::store(po::command_line_parser(argc, argv).
              options(cmdline_options).positional(p).run(), vm);
    po::notify(vm);

    // print help if requested
    if (vm.count("help"))
    {
        std::cout << visible << std::endl;
        return false;
    }

    boost::filesystem::path configDir = Helper::GetDataDir();
    Log::i("(Settings) Directory: \t\t%s", configDir.string().c_str());

    std::string configFile = configDir.string() + "/config.cfg";

    // check for the presence of a config file
    std::ifstream ifs(configFile.c_str());
    if (ifs)
    {
        po::store(parse_config_file(ifs, config_file_options), vm);
        po::notify(vm);
    }
    else
    {
        Log::i("(Settings) -> No config file found...");
    }

    Log::i("(Settings) Listening Port: \t\t%d", port);
    Log::i("(Settings) Flooding TTL: \t\t%d", Settings::GetFloodingTTL());
    Log::i("(Settings) Heartbeat Interval: \t%lu", Settings::GetHeartbeatInterval());
    Log::i("(Settings) Duplicate Validity: \t%lu", Settings::GetDuplicateValidity());
    Log::i("(Settings) Ping Interval: \t\t%lu", Settings::GetPingInterval());
    Log::i("(Settings) Max. Connections: \t\t%d", Settings::GetMaxConnections());
    Log::i("(Settings) Mining Threads: \t\t%d", Settings::GetMiningThreads());
    Log::i("(Settings) Log to File: \t\t%d", Settings::GetPrintToFile());

    return true;
}

// ----------------------------------------------------------------

short
Settings::GetPort()
{
    if (vm.count("port"))
        return vm["port"].as<short>();

    return Settings::defaultPort;
}

// ----------------------------------------------------------------

std::string
Settings::GetDirectory()
{
    if (vm.count("data-dir"))
        return vm["data-dir"].as<std::string>();

    return Helper::GetHomeDir().string() + "/" + Settings::defaultDirectory;
}

// ----------------------------------------------------------------

std::vector<std::string>
Settings::GetInitialPeers()
{
    if (vm.count("connect"))
        return vm["connect"].as< std::vector<std::string> >();

    return std::vector<std::string>();
}

// ----------------------------------------------------------------

int
Settings::GetFloodingTTL()
{
    if (vm.count("flooding-ttl"))
        return vm["flooding-ttl"].as<int>();

    return Settings::defaultFloodTTL;
}

// ----------------------------------------------------------------

long
Settings::GetHeartbeatInterval()
{
    if (vm.count("heartbeat"))
        return vm["heartbeat"].as<long>();

    return Settings::defaultHeartbeatInterval;
}

// ----------------------------------------------------------------

long
Settings::GetDuplicateValidity()
{
    if (vm.count("duplicate-validity"))
        return vm["duplicate-validity"].as<long>();

    return Settings::defaultDuplicateValidity;
}

// ----------------------------------------------------------------

long
Settings::GetPingInterval()
{
    if (vm.count("ping-interval"))
        return vm["ping-interval"].as<long>();

    return Settings::defaultPingInterval;
}

// ----------------------------------------------------------------

unsigned int
Settings::GetMaxConnections()
{
    if (vm.count("max-connections"))
        return vm["max-connections"].as<unsigned int>();

    return Settings::defaultMaxConnections;
}

// ----------------------------------------------------------------

bool
Settings::GetPrintToConsole()
{
    if (vm.count("log-cli"))
        return vm["log-cli"].as<bool>();

    return Settings::defaultLogToConsole;
}

// ----------------------------------------------------------------

bool
Settings::GetPrintToFile()
{
    if (vm.count("log-file"))
        return vm["log-file"].as<bool>();

    return Settings::defaultLogToFile;
}

// ----------------------------------------------------------------

unsigned int
Settings::GetMiningThreads()
{
    if (vm.count("threads-mining"))
        return vm["threads-mining"].as<unsigned int>();

    return Settings::defaultMiningThreads;
}
