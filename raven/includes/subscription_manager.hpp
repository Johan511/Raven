#pragma once

#include <boost/functional/hash.hpp>
#include <contexts.hpp>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <ostream>
#include <serialization/serialization.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utilities.hpp>
#include "serialization/messages.hpp"

namespace rvn
{

struct TrackIdentifier
{
    std::vector<std::string> tracknamespace;
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

    friend inline std::ostream& operator<<(std::ostream& os, const ObjectIdentifier& oid)
    {
        for (const auto& ns : oid.tracknamespace)
            os << ns << "/";
        os << oid.trackname << " " << oid.groupId << " " << oid.objectId << " ";
        return os;
    }
};


struct SubscriptionState
{
    std::optional<ObjectIdentifier> objectToSend{};

    std::optional<ObjectIdentifier> lastObjectToSend{};
};


class DataManager
{
    friend class SubscriptionManager;

    std::string get_path_string(const TrackIdentifier& trackIdentifier)
    {
        std::string pathString = DATA_DIRECTORY;
        for (const auto& ns : trackIdentifier.tracknamespace)
            pathString += ns + "/";
        pathString += trackIdentifier.trackname + "/";
        return pathString;
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


public:
    /*
        * @brief Get the object from the file system
        * @param objectIdentifier
    */
    std::optional<std::string> get_object(ObjectIdentifier objectIdentifier)
    {
        std::string objectLocation = get_path_string(objectIdentifier);
        utils::LOG_EVENT(std::cout, "Reading object from: ", objectLocation);

        if (!std::filesystem::exists(objectLocation))
        {
            utils::ASSERT_LOG_THROW(false, "Object: ", objectIdentifier,
                                    "\ndoes not exist at location\n", objectLocation);
            return {};
        }

        std::ifstream file(std::move(objectLocation));
        std::stringstream ss;
        ss << file.rdbuf();

        return std::move(ss).str();
    }

    std::uint64_t first_object_id(const GroupIdentifier& groupIdentifier)
    {
        std::uint64_t firstObjectId = std::numeric_limits<std::uint64_t>::max();
        std::filesystem::path directoryPath = get_path_string(groupIdentifier);
        for (const auto& entry : std::filesystem::directory_iterator(directoryPath))
        {
            firstObjectId = std::min(firstObjectId, std::stoul(*--entry.path().end()));
        }

        return firstObjectId;
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

    void next(SubscriptionState& subscriptionState)
    {
        subscriptionState.objectToSend->objectId++;
    }
};

DECLARE_SINGLETON(DataManager);

// Required weak linkage because I need them to have external linkage (so can not use static) (to avoid Wsubobject-linkage from its use in SubscriptionManager)
#ifndef __clang__
WEAK_LINKAGE // weak linkage required only in g++ compiler, check reason
#endif
auto SubscriptionMessageHash = [](const depracated::messages::SubscribeMessage& subscribeMessage)
{ return subscribeMessage.subscribeId_; };

#ifndef __clang__
WEAK_LINKAGE // weak linkage required only in g++ compiler, check reason
#endif
auto SubscriptionMessageKeyEqual = [](const depracated::messages::SubscribeMessage& lhs,
                                      const depracated::messages::SubscribeMessage& rhs)
{ return lhs.subscribeId_ == rhs.subscribeId_; };


class SubscriptionManager
{
public:
    std::unordered_map<ConnectionState*, std::unordered_map<depracated::messages::SubscribeMessage, SubscriptionState, decltype(SubscriptionMessageHash), decltype(SubscriptionMessageKeyEqual)>>
    subscriptionStates;

    static SubscriptionState
    build_subscription_state(depracated::messages::SubscribeMessage& subscribeMessage)
    {
        SubscriptionState subscriptionState;

        TrackIdentifier track{ subscribeMessage.trackNamespace_,
                               subscribeMessage.trackName_ };

        std::uint64_t currentGroup = 0; // TODO what is current group
        switch (subscribeMessage.filterType_)
        {
            case depracated::messages::SubscribeMessage::FilterType::LatestGroup:
            {
                GroupIdentifier latestGroup{ track, currentGroup };
                ObjectIdentifier latestObject{ latestGroup, DataManagerHandle{}
                                               -> first_object_id(latestGroup) };

                subscriptionState.objectToSend = latestObject;

                break;
            }
            case depracated::messages::SubscribeMessage::FilterType::LatestObject:
            {
                GroupIdentifier latestGroup{ track, currentGroup };
                ObjectIdentifier latestObject{ latestGroup, DataManagerHandle{}
                                               -> latest_object_id(latestGroup) };

                subscriptionState.objectToSend = latestObject;

                break;
            }
            case depracated::messages::SubscribeMessage::FilterType::AbsoluteStart:
            {
                auto [startGroupUint, startObjectUint] = *subscribeMessage.start_;

                GroupIdentifier startGroup{ track, startGroupUint };
                ObjectIdentifier startObject{ startGroup, startObjectUint };

                subscriptionState.objectToSend = startObject;

                break;
            }
            case depracated::messages::SubscribeMessage::FilterType::AbsoluteRange:
            {
                auto [startGroupUint, startObjectUint] = *subscribeMessage.start_;
                auto [endGroupUint, endObjectUint] = *subscribeMessage.start_;

                GroupIdentifier startGroup{ track, startGroupUint };
                ObjectIdentifier startObject{ startGroup, startObjectUint };

                GroupIdentifier endGroup{ track, endGroupUint };
                ObjectIdentifier endObject{ endGroup, endObjectUint };

                subscriptionState.objectToSend = startObject;
                subscriptionState.lastObjectToSend = endObject;

                break;
            }
            default:
            {
                utils::ASSERT_LOG_THROW(false, "Unknown filter type of: ",
                                        utils::to_underlying(subscribeMessage.filterType_));
            }
        }

        return subscriptionState;
    }


    void update_subscription_state(
    ConnectionState* connectionState,
    std::unordered_map<depracated::messages::SubscribeMessage, SubscriptionState>::iterator iter)
    {
    }

    bool verify_validity(const depracated::messages::SubscribeMessage& subscribeMessage)
    {
        return true;
    }


    using RegisterSubscriptionErr =
    std::optional<depracated::messages::SubscribeErrorMessage>;

    RegisterSubscriptionErr
    build_subscribe_error_message(const depracated::messages::SubscribeMessage& subscribeMessage)
    {
        return {};
    }

    RegisterSubscriptionErr
    try_register_subscription(ConnectionState& connectionState,
                              depracated::messages::SubscribeMessage&& subscribeMessage)
    {
        if (verify_validity(subscribeMessage) == false)
            return build_subscribe_error_message(subscribeMessage);


        SubscriptionState subscriptionState = build_subscription_state(subscribeMessage);
        auto [iter, inserted] =
        subscriptionStates[&connectionState].emplace(std::move(subscribeMessage),
                                                     std::move(subscriptionState));

        utils::ASSERT_LOG_THROW(inserted, "Subscription already exists");

        update_subscription_state(&connectionState, iter);

        return std::nullopt;
    }
    ~SubscriptionManager() = default;
};

DECLARE_SINGLETON(SubscriptionManager);

} // namespace rvn
