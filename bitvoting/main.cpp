/*=============================================================================

Bitvoting Main

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#include "helper.h"
#include "tests/test.h"
#include "settings.h"
#include "controller.h"
#include "miner.h"
#include "net/network.h"
#include "net/protocols/pingpong.h"
#include "net/protocols/initialize.h"
#include "net/protocols/heartbeat.h"
#include "net/protocols/transactions.h"
#include "net/protocols/blocks.h"

#include <stdexcept>
#include <string>
#include <signal.h>

#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread.hpp>
#include <boost/foreach.hpp>

#include <QApplication>

// ==========================================================================
// Functions and Signal Handlers for a proper shutdown...

volatile bool fRequestShutdown = false;

void StartShutdown()
{
    fRequestShutdown = true;
}

bool ShutdownRequested()
{
    return fRequestShutdown;
}

void HandleSIGTERM(int)
{
    StartShutdown();
}

void WaitForShutdown()
{
    bool fShutdown = ShutdownRequested();

    while (!fShutdown)
    {
        Helper::Sleep(200);
        fShutdown = ShutdownRequested();
    }
}

// ==========================================================================
// Main entry point:

//#define RUN_TESTS

int main(int argc, char* argv[])
{
#ifdef RUN_TESTS

    test_start();
    Log::i("(Main) ALL TESTS WERE SUCCESSFUL!");

    return 0;

#endif

    try
    {
        // parse arguments from command line and config file
        if (!Settings::ParseArguments(argc, argv))
            return 1;
    }
    catch (const std::exception& e)
    {
        Log::e("(Main) Could not parse arguments: %s", e.what());
        return 1;
    }

    // initialize data directory
    const boost::filesystem::path &dataDir = Helper::GetDataDir();
    const char* dataDirStr = dataDir.string().c_str();

    if (!boost::filesystem::is_directory(dataDir))
    {
        Log::e("(Main) Specified data directory \"%s\" does not exist.", dataDirStr);
        return 1;
    }

    // make sure only a single process is using the data directory.
    boost::filesystem::path pathLockFile = dataDir / ".lock";
    FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);

    try
    {
        static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
        if (!lock.try_lock())
        {
            Log::e("(Main) Cannot obtain a lock on data directory %s. Application is probably already running.", dataDirStr);
            return 1;
        }
    }
    catch(const boost::interprocess::interprocess_exception& e)
    {
        Log::e("(Main) Cannot obtain a lock on data directory %s. Application is probably already running.\n%s", dataDirStr, e.what());
        return 1;
    }

    // register signal handlers (clean shutdown on SIGTERM)
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = HandleSIGTERM;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    try
    {
        Log::i("(Main) Initializing threads...");
        boost::thread_group threadGroup;

        Log::i("(Main) Initializing network...");
        Network network(threadGroup);

        // initialize protocols
        PingPongProtocol pingPong(network, threadGroup);
        InitializeProtocol init(network);
        HeartbeatProtocol heartbeat(network, threadGroup);
        TransactionsProtocol transactions(network);
        BlocksProtocol blocks(network);

        // initialize mining
        MiningManager mining(&threadGroup, blocks);

        // initialize application
        QApplication application(argc, argv);
        MainWindow gui;
        Controller controller(gui, mining, transactions, blocks);

        // make initial connections
        BOOST_FOREACH(std::string peer, Settings::GetInitialPeers())
        {
            try
            {
                // parse string and connect
                network.Connect(Helper::GetEndpoint(peer));
            }
            catch(const std::invalid_argument&)
            {
                Log::w("(Main) Could not parse %s as an IPv4 address!", peer.c_str());
            }
            catch(const std::exception& e)
            {
                Log::w("(Main) Exception: %s", e.what());
            }
        }

        // show GUI and start application
        gui.show();
        int ret = application.exec();

        // waiting for shutdown signal
        //WaitForShutdown();

        Log::i("(Main) Waiting for threads to finish...");

        network.Shutdown();

        // wait 5sec for threads to finish, then interrupt
        Helper::Sleep(5000);
        threadGroup.interrupt_all();

        threadGroup.join_all();

        Log::i("(Main) Threads finished! Goodbye!");

        return ret;
    }
    catch (const std::exception& e)
    {
        Log::e("(Main) Critical Exception: %s", e.what());
        return 1;
    }
}
