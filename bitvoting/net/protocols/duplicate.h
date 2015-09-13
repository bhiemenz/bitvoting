/*=============================================================================

Base class protocol for protocols who need to remember the last seen messages.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef DUPLICATE_PROTOCOL_H
#define DUPLICATE_PROTOCOL_H

#include "helper.h"
#include "settings.h"

#include <map>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <boost/thread/mutex.hpp>

// ==========================================================================

typedef std::pair<boost::uuids::uuid, long long> id_pair;
typedef std::vector<id_pair> id_memory;

// ==========================================================================

class DuplicateProtocol
{
public:
    // Registers a message as seen
    void seenMessage(Message* message)
    {
        // store message ID along with current timestamp
        this->lastSeenMutex.lock();
        this->lastSeen.push_back(id_pair(message->id, Helper::GetUNIXTimestamp()));
        this->lastSeenMutex.unlock();
    }

    // Check if a given message is a duplicate of recently seen messages
    bool checkDuplicate(Message* message)
    {
        this->lastSeenMutex.lock();

        // iterate over all last seen
        id_memory::iterator it;
        for (it = this->lastSeen.begin(); it < this->lastSeen.end();)
        {
            id_pair current = *it;

            // check if entry is still valid
            if ((Helper::GetUNIXTimestamp() - current.second) >= Settings::GetDuplicateValidity())
            {
                it = this->lastSeen.erase(it);
                continue;
            }

            // discard, if ping ID was seen
            if (message->id == current.first)
            {
                this->lastSeenMutex.unlock();
                return true;
            }

             ++it;
        }

        this->lastSeenMutex.unlock();

        // register as seen
        this->seenMessage(message);

        return false;
    }

private:
    // stores the last seen messages
    id_memory lastSeen;

    boost::mutex lastSeenMutex;
};

#endif // DUPLICATE_PROTOCOL_H
