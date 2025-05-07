#pragma once
#include "definitions.hpp"
#include "serialization/messages.hpp"
#include "serialization/serialization.hpp"
#include <boost/functional/hash.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <msquic.h>
#include <string>
#include <strong_types.hpp>
#include <unordered_map>
#include <utilities.hpp>
#include <variant>
#include <vector>

namespace rvn
{
// We could not find the object requested

class TrackIdentifier
{
    std::shared_ptr<std::pair<std::vector<std::string>, std::string>> trackIdentifierInternal_;

public:
    struct Hash
    {
        std::uint64_t operator()(const TrackIdentifier& id) const
        {
            std::uint64_t hash = 0;
            for (const auto& ns : id.tnamespace())
                boost::hash_combine(hash, ns);

            boost::hash_combine(hash, id.tname());
            return hash;
        }
    };
    using Equal = std::equal_to<TrackIdentifier>;

    const std::vector<std::string>& tnamespace() const noexcept
    {
        return trackIdentifierInternal_->first;
    }
    const std::string& tname() const noexcept
    {
        return trackIdentifierInternal_->second;
    }

    TrackIdentifier(std::vector<std::string> tracknamespace, std::string trackname);

    bool operator==(const TrackIdentifier& other) const
    {
        return tnamespace() == other.tnamespace() && tname() == other.tname();
    }

    friend inline std::ostream& operator<<(std::ostream& os, const TrackIdentifier& id)
    {
        os << "TrackIdentifier{tracknamespace=[";
        for (const auto& ns : id.tnamespace())
        {
            os << ns << ",";
        }
        os << "], trackname=" << id.tname() << "}";
        return os;
    }
};

class GroupIdentifier : public TrackIdentifier
{
public:
    GroupId groupId_;

    bool operator==(const GroupIdentifier& other) const
    {
        return TrackIdentifier::operator==(other) && groupId_ == other.groupId_;
    }

    friend inline std::ostream& operator<<(std::ostream& os, const GroupIdentifier& id)
    {
        os << static_cast<const TrackIdentifier&>(id) << ", groupId=" << id.groupId_;
        return os;
    }

    GroupIdentifier(TrackIdentifier trackIdentifier, GroupId groupId);
};

class ObjectIdentifier : public GroupIdentifier
{
    // we cache the subgroup id within the object identifier
    mutable std::optional<SubGroupId> subgroupId_;

public:
    ObjectId objectId_;

    bool operator==(const ObjectIdentifier& other) const
    {
        return GroupIdentifier::operator==(other) && objectId_ == other.objectId_;
    }

    friend inline std::ostream& operator<<(std::ostream& os, const ObjectIdentifier& id)
    {
        os << static_cast<const GroupIdentifier&>(id) << ", objectId=" << id.objectId_;
        return os;
    }

    ObjectIdentifier(GroupIdentifier groupIdentifier, ObjectId objectId);
    ObjectIdentifier(TrackIdentifier trackIdentifier, GroupId groupId, ObjectId objectId);
};

class Object
{
    std::uint8_t flag_;

public:
    // clang-format off
    struct GroupTerminator { static constexpr std::uint8_t flag_ = 1; };
    struct TrackTerminator { static constexpr std::uint8_t flag_ = 3; };
    // clang-format on

    // should be read only if flags_ == 0
    QUIC_BUFFER* payload_;

    // clang-format off
    bool is_group_terminator() const noexcept{ return std::countr_one(flag_) >= 1; }
    bool is_track_terminator() const noexcept{ return std::countr_one(flag_) >= 2; }
    bool is_valid_group() const noexcept{ return flag_ == 0;}
    // clang-format on

    Object(QUIC_BUFFER* payload) : flag_(0), payload_(payload)
    {
    }

    Object(GroupTerminator) : flag_(GroupTerminator::flag_), payload_(nullptr)
    {
    }

    Object(TrackTerminator) : flag_(TrackTerminator::flag_), payload_(nullptr)
    {
    }
};

/*
    Used to signal if the object has published and can be read
    Returned when the object is not found,
    and we do not want the reader to keep querying on a spin loop
    as it causes R/W contention and too many seq cst atomic operations
*/
enum class WaitStatus
{
    Wait,
    Ready
};
// TODO: possibly make wait signals strong types
using WaitSignal = std::shared_ptr<std::atomic<WaitStatus>>;
using EnrichedObjectType = std::tuple<GroupId, ObjectId, Object>;
using EnrichedObjectOrWait = std::variant<std::monostate, EnrichedObjectType, WaitSignal>;
class TrackHandle : public std::enable_shared_from_this<TrackHandle>
{
    friend class DataManager;
    friend class GroupHandle;
    class DataManager& dataManager_;

public:
    TrackIdentifier trackIdentifier_;

private:
    PublisherPriority publisherPriority_;
    std::optional<std::chrono::milliseconds> deliveryTimeout_;

public:
    std::mutex mtx_; // protects objects_, update signal
    // total order established by group id + object id
    std::map<std::tuple<GroupId, ObjectId>, Object> objects_;
    WaitSignal updateSignal_;

    // should be private but want to use std::make_shared
    TrackHandle(DataManager& dataManagerHandle, TrackIdentifier trackIdentifier);

    TrackHandle& operator=(const TrackHandle&) = delete;
    TrackHandle& operator=(TrackHandle&&) = delete;

    EnrichedObjectOrWait get_first_object();
    EnrichedObjectOrWait get_next_object(const ObjectIdentifier& objectIdentifier);
    EnrichedObjectOrWait get_latest_object(const std::optional<ObjectIdentifier>& oid);

    void add_object(GroupId groupId, ObjectId objectId, Object::GroupTerminator)
    {
        std::unique_lock l(mtx_);
        objects_.emplace(std::make_tuple(groupId, objectId),
                         Object{ Object::GroupTerminator{} });
        updateSignal_->store(WaitStatus::Ready, std::memory_order::release);
        updateSignal_ = std::make_shared<std::atomic<WaitStatus>>(WaitStatus::Wait);
    }

    void add_object(GroupId groupId, ObjectId objectId, Object::TrackTerminator)
    {
        std::unique_lock l(mtx_);
        objects_.emplace(std::make_tuple(groupId, objectId),
                         Object{ Object::TrackTerminator{} });
        updateSignal_->store(WaitStatus::Ready, std::memory_order::release);
        updateSignal_ = std::make_shared<std::atomic<WaitStatus>>(WaitStatus::Wait);
    }

    void add_object(GroupId groupId, ObjectId objectId, std::string data)
    {
        StreamHeaderSubgroupObject subgroupObject;
        subgroupObject.objectId_ = objectId;
        subgroupObject.payload_ = std::move(data);

        QUIC_BUFFER* quicBuffer = serialization::serialize(subgroupObject);
        std::unique_lock l(mtx_);
        objects_.emplace(std::make_tuple(groupId, objectId), Object{ quicBuffer });
        updateSignal_->store(WaitStatus::Ready, std::memory_order::release);
        updateSignal_ = std::make_shared<std::atomic<WaitStatus>>(WaitStatus::Wait);
    }
};

// TODO: Read and make use of https://www.sqlite.org/fasterthanfs.html

class DataManager
{
    friend class SubgroupHandle;
    friend class GroupHandle;
    friend class TrackHandle;

    RWProtected<std::unordered_map<TrackIdentifier, std::shared_ptr<TrackHandle>, TrackIdentifier::Hash, TrackIdentifier::Equal>> trackHandles_;
    RWProtected<std::unordered_map<TrackIdentifier, WaitSignal, TrackIdentifier::Hash, TrackIdentifier::Equal>> trackWaitSignals_;

    std::string get_path_string(const TrackIdentifier& trackIdentifier);
    std::string get_path_string(const GroupIdentifier& groupIdentifier);
    std::string get_path_string(const ObjectIdentifier& objectIdentifier);

    bool store_object(const GroupIdentifier& groupIdentifier,
                      ObjectId objectId,
                      std::string&& object);

public:
    std::weak_ptr<TrackHandle>
    add_track_identifier(std::vector<std::string> tracknamespace, std::string trackname);

    std::variant<std::shared_ptr<TrackHandle>, WaitSignal>
    get_track_handle(const TrackIdentifier& trackIdentifier);

    PublisherPriority get_track_publisher_priority(const TrackIdentifier& trackIdentifier);

    DataManager()
    {
    }
};
} // namespace rvn
