#include "connection.h"
#include "network.h"
#include "peers.h"

#include <iomanip>
#include <string>
#include <sstream>
#include <vector>

#include <boost/bind.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

// ================================================================
// These macros are needed for serialization

#include "message.h"
BOOST_CLASS_EXPORT(Message)
#include "messages/ping.h"
BOOST_CLASS_EXPORT(PingMessage)
#include "messages/pong.h"
BOOST_CLASS_EXPORT(PongMessage)
#include "messages/text.h"
BOOST_CLASS_EXPORT(TextMessage)
#include "messages/peerinfo.h"
BOOST_CLASS_EXPORT(PeerInfoMessage)
#include "messages/heartbeat.h"
BOOST_CLASS_EXPORT(HeartbeatMessage)
#include "messages/transaction.h"
BOOST_CLASS_EXPORT(TransactionMessage)
#include "messages/block.h"
BOOST_CLASS_EXPORT(BlockMessage)
#include "messages/block_request.h"
BOOST_CLASS_EXPORT(BlockRequestMessage)

#include "../export.h"

BITVOTING_CLASS_EXPORT_IMPLEMENT(Signable)
BITVOTING_CLASS_EXPORT_IMPLEMENT(Transaction)
#include "../transactions/vote.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(TxVote)
#include "../transactions/election.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(TxElection)
#include "../transactions/tally.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(TxTally)
#include "../transactions/trustee_tally.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(TxTrusteeTally)
#include "../block.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(Block)

// ================================================================

boost::asio::ip::tcp::socket&
Connection::GetSocket()
{
    return this->socket;
}

// ----------------------------------------------------------------

void
Connection::SetCallback(boost::function<void (Message*)> callback)
{
    this->callback = callback;
}

// ----------------------------------------------------------------

void
Connection::Close()
{
    if (this->shutdown) return;

    Log::i("(Network) Closing connection to %s...",
           Helper::GetAddressString(this->socket.remote_endpoint()).c_str());

    // close the connection
    Peers::RemoveConnection(shared_from_this());
    this->shutdown = true;
    this->socket.close();
}

// ----------------------------------------------------------------

void
Connection::Write(Message* message)
{
    // set TTL to 1 if not been set, drop message if TTL is le 0
    if (message->header.ttl == TTL_NOT_SET)
        message->header.ttl = 1;
    else if (message->header.ttl != TTL_INFINITE && message->header.ttl <= 0)
        return;

    // serialize the data first so we know how large it is
    std::ostringstream archive_stream;
    boost::archive::text_oarchive archive(archive_stream);
    archive << message;
    this->dataOut = archive_stream.str();

    // set header accordingly
    message->header.size = this->dataOut.size();

    // Write the serialized data to the socket. We use "gather-write" to send
    // both the header and the data in a single write operation.
    std::vector<boost::asio::const_buffer> buffers;
    buffers.push_back(boost::asio::buffer((char*)&message->header, sizeof(MessageHeader)));
    buffers.push_back(boost::asio::buffer(this->dataOut));

    Log::i("(Network) Sending: %s to %s...", message->string().c_str(), Helper::GetAddressString(this->socket.remote_endpoint()).c_str());

    // write data asynchronously
    boost::asio::async_write(this->socket,
                             buffers,
                             boost::bind(&Connection::WriteCallback,
                                         shared_from_this(),
                                         boost::asio::placeholders::error,
                                         boost::asio::placeholders::bytes_transferred
                                         ));

}

// ----------------------------------------------------------------

void
Connection::WriteCallback(const boost::system::error_code& error,
                          size_t /*bytes_transferred*/)
{
    if (error)
    {
        // stop and remove connection if problems occured
        Peers::RemoveConnection(shared_from_this());
        return;
    }
}

// ----------------------------------------------------------------

void
Connection::Read()
{
    if (this->shutdown) return;

    // start listening for (header) data asynchronously
    boost::asio::async_read(this->socket,
                            boost::asio::buffer((char*)&this->header, sizeof(MessageHeader)),
                            boost::bind(&Connection::ReadHeaderCallback,
                                        shared_from_this(),
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred
                                        ));
}

// ----------------------------------------------------------------

void
Connection::ReadHeaderCallback(const boost::system::error_code& error,
                               size_t /*bytes_transferred*/)
{
    if (error)
    {
        // check if connection was closed cleanly
        if (error == boost::asio::error::eof)
            Log::i("(Network) Connection was closed remotely %s!",
                   Helper::GetAddressString(this->socket.remote_endpoint()).c_str());
        else if (error != boost::asio::error::operation_aborted)
            Log::e("(Network) %s", error.message().c_str());

        // stop and remove connection if problems occured
        Peers::RemoveConnection(shared_from_this());
        return;
    }

    // prepare receiving buffer
    this->dataIn.resize(this->header.size);

    // start an asynchronous call to receive the (body) data.
    boost::asio::async_read(this->socket,
                            boost::asio::buffer(this->dataIn),
                            boost::bind(&Connection::ReadDataCallback,
                                        shared_from_this(),
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred
                                        ));
}

// ----------------------------------------------------------------

void
Connection::ReadDataCallback(const boost::system::error_code& error,
                             size_t /*bytes_transferred*/)
{
    if (error)
    {
        // check if connection was closed cleanly
        if (error == boost::asio::error::eof)
            Log::i("(Network) Connection was closed remotely %s!",
                   Helper::GetAddressString(this->socket.remote_endpoint()).c_str());
        else if (error != boost::asio::error::operation_aborted)
            Log::e("(Network) %s", error.message().c_str());

        // stop and remove connection if problems occured
        Peers::RemoveConnection(shared_from_this());
        return;
    }

    // increase hop, decrease ttl (if needed)
    this->header.hop++;

    if (this->header.ttl != TTL_INFINITE)
        this->header.ttl--;

    try
    {
        // extract the data structure from the data just received.
        std::string archive_data(&this->dataIn[0], this->header.size);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> this->message;
    }
    catch (const std::exception& e)
    {
        Log::e("(Network) %s", e.what());
        this->Close();
        return;
    }

    // combine header and body data
    this->message->header = this->header;

    Log::i("(Network) Received: %s from %s!",
           this->message->string().c_str(),
           Helper::GetAddressString(this->socket.remote_endpoint()).c_str());

    try
    {
        // call callback (if exists)
        if (this->callback)
            this->callback(this->message);
    }
    catch (const std::exception& e)
    {
        Log::e("(Network) In callback: %s", e.what());
    }

    delete this->message;

    // start reading again
    this->Read();
}
