#pragma once

#include "serialization.hpp"
#include "utilities.hpp"
#include <boost/functional/hash.hpp>
#include <contexts.hpp>
#include <filesystem>
#include <fstream>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>

namespace rvn
{

struct TrackIdentifier
{
    std::string tracknamespace;
    std::string trackname;

    bool operator==(const TrackIdentifier& other) const
    {
        return tracknamespace == other.tracknamespace && trackname == other.trackname;
    }
};

struct GroupIdentifier : public TrackIdentifier
{
    std::uint64_t groupId;

    bool operator==(const GroupIdentifier& other) const
    {
        return TrackIdentifier::operator==(other) && groupId == other.groupId;
    }
};

struct ObjectIdentifier : public GroupIdentifier
{
    std::uint64_t objectId;

    bool operator==(const ObjectIdentifier& other) const
    {
        return GroupIdentifier::operator==(other) && objectId == other.objectId;
    }
};

static inline std::size_t hash_value(const ObjectIdentifier& b)
{
    std::size_t seed = 0;
    boost::hash_combine(seed, b.tracknamespace);
    boost::hash_combine(seed, b.trackname);
    boost::hash_combine(seed, b.groupId);
    boost::hash_combine(seed, b.objectId);
    return seed;
}

struct RegisterSubscriptionErr
{
};

struct SubscriptionState
{
    std::optional<ObjectIdentifier> objectToSend{};

    std::optional<ObjectIdentifier> lastObjectToSend{};
};


class DataManager
{
    friend class SubscriptionManager;
    const std::string dataPath = DATA_DIRECTORY;

    std::string get_path_string(const TrackIdentifier& trackIdentifier)
    {
        return dataPath + trackIdentifier.tracknamespace + "/" +
               trackIdentifier.trackname + "/";
    }

    std::string get_path_string(const GroupIdentifier& groupIdentifier)
    {
        return get_path_string(static_cast<TrackIdentifier>(groupIdentifier)) +
               std::to_string(groupIdentifier.groupId) + "/";
    }

    std::string get_path_string(const ObjectIdentifier& objectIdentifier)
    {
        return get_path_string(static_cast<GroupIdentifier>(objectIdentifier)) +
               std::to_string(objectIdentifier.objectId);
    }

    std::optional<std::string> get_object(ObjectIdentifier objectIdentifier)
    {
        std::ifstream file(get_path_string(objectIdentifier));
        if (!file.is_open())
            return {};

        std::stringstream ss;
        ss << file.rdbuf();

        return std::move(ss).str();
    }

    std::uint64_t first_object_id(const GroupIdentifier& groupIdentifier)
    {
        std::uint64_t firstObjectId = INT_MAX;
        std::filesystem::path directoryPath = get_path_string(groupIdentifier);
        for (const auto& entry : std::filesystem::directory_iterator(directoryPath))
        {
            firstObjectId = std::min(firstObjectId, std::stoul(*--entry.path().end()));
        }

        return ++firstObjectId;
    }

    std::uint64_t latest_object_id(const GroupIdentifier& groupIdentifier)
    {
        namespace fs = std::filesystem;
        fs::path path = get_path_string(groupIdentifier);
        std::optional<fs::directory_entry> lastEntry;
        for (const auto& entry : fs::directory_iterator(path))
            lastEntry = entry;

        return std::stoull(lastEntry->path().filename());
    }

    void next_subscription_state(SubscriptionState& subscriptionState)
    {
        subscriptionState.objectToSend->objectId++;
    }
};

DECLARE_SINGLETON(DataManager);

// Required weak linkage because I need them to have external linkage (so can not use static) (to avoid Wsubobject-linkage from its use in SubscriptionManager)
#ifndef __clang__
WEAK_LINKAGE // weak linkage required only in g++ compiler, check reason
#endif
auto SubscriptionMessageHash = [](const protobuf_messages::SubscribeMessage& subscribeMessage)
{ return subscribeMessage.subscribeid(); };

#ifndef __clang__
WEAK_LINKAGE // weak linkage required only in g++ compiler, check reason
#endif
auto SubscriptionMessageKeyEqual = [](const protobuf_messages::SubscribeMessage& lhs,
                                      const protobuf_messages::SubscribeMessage& rhs)
{ return lhs.subscribeid() == rhs.subscribeid(); };


class SubscriptionManager
{
public:
    std::unordered_map<ConnectionState*, std::unordered_map<protobuf_messages::SubscribeMessage, SubscriptionState, decltype(SubscriptionMessageHash), decltype(SubscriptionMessageKeyEqual)>>
    subscriptionStates;

    static SubscriptionState
    build_subscription_state(protobuf_messages::SubscribeMessage& subscribeMessage)
    {
        SubscriptionState subscriptionState;

        TrackIdentifier track{ subscribeMessage.tracknamespace(),
                               subscribeMessage.trackname() };

        std::uint64_t currentGroup = 0; // TODO what is current group
        switch (subscribeMessage.filtertype())
        {
            case protobuf_messages::SubscribeFilter::LatestGroup:
            {
                GroupIdentifier latestGroup{ track, currentGroup };
                ObjectIdentifier latestObject{ latestGroup, DataManagerHandle{}
                                               -> first_object_id(latestGroup) };

                subscriptionState.objectToSend = latestObject;

                break;
            }
            case protobuf_messages::SubscribeFilter::LatestObject:
            {
                GroupIdentifier latestGroup{ track, currentGroup };
                ObjectIdentifier latestObject{ latestGroup, DataManagerHandle{}
                                               -> latest_object_id(latestGroup) };

                subscriptionState.objectToSend = latestObject;

                break;
            }
            case protobuf_messages::SubscribeFilter::AbsoluteStart:
            {
                GroupIdentifier startGroup{ track, subscribeMessage.startgroup() };
                ObjectIdentifier startObject{ startGroup, subscribeMessage.startobject() };

                subscriptionState.objectToSend = startObject;

                break;
            }
            case protobuf_messages::SubscribeFilter::AbsoluteRange:
            {
                GroupIdentifier startGroup{ track, subscribeMessage.startgroup() };
                ObjectIdentifier startObject{ startGroup, subscribeMessage.startobject() };

                GroupIdentifier endGroup{ track, subscribeMessage.endgroup() };
                ObjectIdentifier endObject{ endGroup, subscribeMessage.endobject() };

                subscriptionState.objectToSend = startObject;
                subscriptionState.lastObjectToSend = endObject;

                break;
            }
            default:
            {
                utils::ASSERT_LOG_THROW(false, "Unknown filter type of: ",
                                        subscribeMessage.filtertype());
            }
        }

        return subscriptionState;
    }


    bool failed(RegisterSubscriptionErr)
    {
        return false;
    }

    RegisterSubscriptionErr
    authentication(ConnectionState&, const protobuf_messages::SubscribeMessage& subscribeMessage)
    {
        return {};
    }

    void update_subscription_state(
    ConnectionState* connectionState,
    std::unordered_map<protobuf_messages::SubscribeMessage, SubscriptionState>::iterator iter)
    {
        auto& [subscribeMessage, subscriptionState] = *iter;

        ObjectIdentifier objectIdentifier{ subscriptionState.objectToSend->tracknamespace,
                                           subscriptionState.objectToSend->trackname,
                                           subscriptionState.objectToSend->groupId,
                                           subscriptionState.objectToSend->objectId };
        // check if object is already in cache
        std::optional<std::string> objectPayloadOpt = DataManagerHandle
        {
        } -> get_object(objectIdentifier);

        // utils::ASSERT_LOG_THROW(objectPayloadOpt.has_value(),
        //                         "Buffer for object not found");
        if (!objectPayloadOpt.has_value())
        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(50ms);
            update_subscription_state(connectionState, iter);
            return;
        }

        protobuf_messages::MessageHeader header;
        header.set_messagetype(protobuf_messages::MoQtMessageType::OBJECT_STREAM);

        protobuf_messages::ObjectStreamMessage objectStreamMessage;
        objectStreamMessage.set_subscribeid(subscribeMessage.subscribeid());
        objectStreamMessage.set_trackalias(subscribeMessage.trackalias());
        objectStreamMessage.set_groupid(0);
        objectStreamMessage.set_objectid(0);
        objectStreamMessage.set_publisherpriority(0);
        // TODO: Object Status Cache
        objectStreamMessage.set_objectstatus(protobuf_messages::ObjectStatus::Normal);
        objectStreamMessage.set_objectpayload(std::move(objectPayloadOpt).value());


        QUIC_BUFFER* quicBuffer = serialization::serialize(header, objectStreamMessage);
        connectionState->enqueue_data_buffer(quicBuffer);

        DataManagerHandle
        {
        } -> next_subscription_state(subscriptionState);
    }

    RegisterSubscriptionErr
    try_register_subscription(ConnectionState& connectionState,
                              protobuf_messages::SubscribeMessage&& subscribeMessage)
    {
        RegisterSubscriptionErr errorInfo =
        authentication(connectionState, subscribeMessage);

        if (this->failed(errorInfo))
            return errorInfo;

        SubscriptionState subscriptionState = build_subscription_state(subscribeMessage);
        auto [iter, inserted] =
        subscriptionStates[&connectionState].emplace(std::move(subscribeMessage),
                                                     std::move(subscriptionState));

        utils::ASSERT_LOG_THROW(inserted, "Subscription already exists");

        update_subscription_state(&connectionState, iter);

        return errorInfo;
    }
    ~SubscriptionManager() = default;
};

DECLARE_SINGLETON(SubscriptionManager);

} // namespace rvn
