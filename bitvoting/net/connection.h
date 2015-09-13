/*=============================================================================

Responsible for handling a single connection to another peer.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef CONNECTION_H
#define CONNECTION_H

#include "message.h"

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/uuid/uuid.hpp>

// ==========================================================================

typedef enum ConnectionType
{
    INBOUND     = 0x0,
    OUTBOUND    = 0x1
} ConnectionType;

// ==========================================================================

class Connection:
        public boost::enable_shared_from_this<Connection>
{
public:
    Connection(boost::asio::io_service& ioService,
               ConnectionType type = ConnectionType::INBOUND):
        type(type),
        socket(ioService) {}

    // ----------------------------------------------------------------

    void Read();
    void Write(Message*);
    void Close();

    // Register callback for all Messages
    void SetCallback(boost::function<void (Message*)>);

    // Get the underlying socket for this connection
    boost::asio::ip::tcp::socket& GetSocket();

    // Obtain type (inbound/outbound)
    inline ConnectionType GetType()
    {
        return this->type;
    }

    // Check if peers already exchanged PeerInfo Messages
    inline bool IsInitialized()
    {
        return !this->guid.is_nil();
    }

    // ----------------------------------------------------------------

    // Holds the type of this connection
    ConnectionType type;

    // Holds information about the remote peer
    boost::uuids::uuid guid;
    long version = -1;

private:
    // Internal callbacks for I/O
    void WriteCallback(const boost::system::error_code&, size_t);
    void ReadHeaderCallback(const boost::system::error_code&, size_t);
    void ReadDataCallback(const boost::system::error_code&, size_t);

    // ----------------------------------------------------------------

    // Underlying socket
    boost::asio::ip::tcp::socket socket;

    // Message callback
    boost::function<void (Message*)> callback = NULL;

    // Shutdown flag for thread safe closure
    bool shutdown = false;

    // ----------------------------------------------------------------

    // Internal help variables for I/O
    MessageHeader header;
    Message* message = NULL;

    std::string dataOut;
    std::vector<char> dataIn;
};

#endif // CONNECTION_H
