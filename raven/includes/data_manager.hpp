#pragma once
#include "definitions.hpp"
#include <boost/functional/hash.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <strong_types.hpp>
#include <unordered_map>
#include <utilities.hpp>
#include <variant>
#include <vector>

/*
Object Hierarchy Structure:

Track -> Group -> Object

Objects in a group are tagged into subgroups,
subgroups are not used to identify an object (hence not part of Object
Identifier)

Subgroups only help as hints about if the objects should/need not be sent on the
same stream

Each subgroup -> one stream

DataManager.add_track_idenifier => adds a new track and returns handle
(weak_ptr<TrackHandle>) to it (return nullptr if already exists) Stores a
mapping from TrackIdentifier to TrackHandle


User deletes Track
Deletes the pair <TrackIdentifier, std::shared_ptr<TrackHandle>> from
DataManager use_count of TrackHandle decreases by 1

Finally when user deletes TrackHandler, use_count of TrackHandler becomes 0 and
is cleaned up

Similarly TrackHandle has mapping from GroupId to std::shared_ptr<GroupHandle>
When we add group, it returns std::weak_ptr<GroupHandle>

And similarly
And when we add Subgroup to GroupHandle, it returns
std::weak_ptr<SubgroupHandle>

GroupHandle does not maintain each SubGroup or Object seperately, but rather
maintains the ObjectId ranges for each SubGroup

When we add an object to a SubGroup, We call DataManager.add_object with the
GroupIdentifier which stores the object



Object Identifier has a weak reference to TrackIdentifier (because
TrackIdentifier is a large object and we want to avoid copying it) Why not
shared_ptr? Object Identifier is not responsible for the lifetime of
TrackIdentifier, if TrackIdentifier has expired, we don't want ObjectIdentifier

NOTE: shared_ptr of any identifier or handle must be created in this file
internally
*/

namespace rvn
{
// We could not find the object requested
struct NotFound
{
    const char* reason = "Object not found";
};
// Object is not expected to exist
struct DoesNotExist
{
    const char* reason = "Object does not exist";
};

/*
    Used to signal if the object has published and can be read
    Returned when the object is not found,
    and we do not want the reader to keep querying on a spin loop
    as it causes R/W contention and too many seq cst atomic operations
*/
enum class ObjectWaitStatus
{
    Wait,
    Ready
};
using ObjectWaitSignal = std::shared_ptr<std::atomic<ObjectWaitStatus>>;
using ObjectType = std::tuple<QUIC_BUFFER*, std::optional<std::chrono::milliseconds>>;
using ObjectOrStatus = std::variant<ObjectType, ObjectWaitSignal, DoesNotExist>;

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

    SubGroupId get_subgroup_id(class DataManager& dataManager) const;

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

class SubgroupHandle
{
    friend class DataManager;
    friend class GroupHandle;
    std::weak_ptr<class GroupHandle> groupHandle_;
    class DataManager& dataManager_;

    ObjectId beginObjectId_;
    ObjectId endObjectId_;
    std::uint64_t numObjects_; // number of objects added using add_object

    SubgroupHandle(std::weak_ptr<GroupHandle> groupHandle,
                   DataManager& dataManager,
                   ObjectId beginObjectId,
                   ObjectId endObjectId)
    : groupHandle_(groupHandle), dataManager_(dataManager),
      beginObjectId_(beginObjectId), endObjectId_(endObjectId), numObjects_(0)
    {
    }

public:
    bool add_object(std::string object);

    // caps the subgroup with how many ever objects it currently has
    void cap();

    // caps the subgroup and returns another openended subgroup and return its
    // handle we return optional just in case the group has been deleted or some
    // error occurs
    std::optional<SubgroupHandle> cap_and_next();
};

// We do not allow for objects to be deleted
class GroupHandle : public std::enable_shared_from_this<GroupHandle>
{
public:
    friend class DataManager;
    friend class TrackHandle;
    friend class SubgroupHandle;
    GroupIdentifier groupIdentifier_;
    PublisherPriority publisherPriority_;
    std::optional<std::chrono::milliseconds> deliveryTimeout_;
    class DataManager& dataManager_;

    GroupHandle& operator=(const GroupHandle&) = delete;
    // GroupHandle& operator=(GroupHandle&&) = delete;

private:
    struct Comparator
    {
        bool operator()(std::uint64_t l, std::uint64_t r) const
        {
            std::uint64_t lMasked = l & (~(1ULL << 63)); //  mask the MSB
            std::uint64_t rMasked = r & (~(1ULL << 63)); //  mask the MSB

            // we want {0_begin, 1_end, 1_begin, 2_end, ...}
            if (lMasked == rMasked)
                return l > r;
            else
                return lMasked < rMasked;
        }
    };

    // stores number of concrete objects that is, objects which have been stored
    std::atomic<std::uint64_t> numStoredObjects_;

    std::shared_mutex objectIdsMtx_;
    std::set<std::uint64_t, Comparator> objectIds_;

    struct ObjectIdHash
    {
        std::uint64_t operator()(const ObjectId& oid) const noexcept
        {
            return oid.get();
        }
    };

    using ObjectIdEqual = std::equal_to<ObjectId>;

    RWProtected<std::unordered_map<ObjectId, QUIC_BUFFER*, ObjectIdHash, ObjectIdEqual>> groupCache_;
    RWProtected<std::unordered_map<ObjectId, ObjectWaitSignal, ObjectIdHash, ObjectIdEqual>> objectWaitSignals_;

public:
    GroupHandle(GroupIdentifier groupIdentifier,
                PublisherPriority publisherPriority_,
                std::optional<std::chrono::milliseconds> deliveryTimeout,
                DataManager& dataManagerHandle);

    SubgroupHandle add_subgroup(std::uint64_t numElements);
    SubgroupHandle add_open_ended_subgroup();

    SubGroupId get_subgroup_id(ObjectId objectId) const
    {
        std::uint64_t objectIdInt = objectId.get();

        /*
            begin is small
            end is capital

                 x                                        -> upper bound gives 1,
           subgroup id is 0 x                                -> x == e0, upper bound
           gives 2, subgroup id is 1 [b0     e0) [e0     e1) [e1     e2) [e2 e3)
        */
        auto iter = objectIds_.upper_bound(objectIdInt);
        if (iter == objectIds_.begin())
            throw std::invalid_argument("ObjectId not found in GroupHandle");
        return SubGroupId(std::distance(objectIds_.begin(), iter) / 2);
    }

    bool has_object_id(ObjectId objectId);

    std::uint64_t
    num_objects_in_range(ObjectId left = ObjectId(0),
                         ObjectId right = ObjectId(std::numeric_limits<std::uint64_t>::max()));
};

class TrackHandle : public std::enable_shared_from_this<TrackHandle>
{
    friend class DataManager;
    friend class GroupHandle;
    class DataManager& dataManager_;

    TrackIdentifier trackIdentifier_;

public:
    std::shared_mutex groupHandlesMtx_;

    std::atomic<std::uint64_t> totalNumGroups_; // total number of groups including ethereal ones
    std::map<GroupId, std::shared_ptr<GroupHandle>> groupHandles_;

    // should be private because but want to use std::make_shared
    TrackHandle(DataManager& dataManagerHandle, TrackIdentifier trackIdentifier);

    std::weak_ptr<GroupHandle>
    add_group(GroupId groupId,
              PublisherPriority publisherPriority,
              std::optional<std::chrono::milliseconds> deliveryTimeout)
    {
        return add_group(
        std::make_shared<GroupHandle>(GroupIdentifier{ trackIdentifier_, groupId },
                                      publisherPriority, deliveryTimeout, dataManager_));
    }

    std::weak_ptr<GroupHandle> add_group(std::shared_ptr<GroupHandle> groupHandle)
    {
        // writer lock
        std::unique_lock<std::shared_mutex> l(groupHandlesMtx_);

        auto [iter, success] =
        groupHandles_.try_emplace(groupHandle->groupIdentifier_.groupId_,
                                  std::move(groupHandle));

        return iter->second->weak_from_this();
    }

    GroupId allot_group_id()
    {
        return GroupId(totalNumGroups_.fetch_add(1, std::memory_order_relaxed));
    }

    TrackHandle& operator=(const TrackHandle&) = delete;
    TrackHandle& operator=(TrackHandle&&) = delete;
};

// TODO: Read and make use of https://www.sqlite.org/fasterthanfs.html

class DataManager
{
    friend class SubgroupHandle;
    friend class GroupHandle;
    friend class TrackHandle;

    std::shared_mutex objectHierarchyMtx_;
    std::unordered_map<TrackIdentifier, std::shared_ptr<TrackHandle>, TrackIdentifier::Hash, TrackIdentifier::Equal> objectHierarchy_;

    std::string get_path_string(const TrackIdentifier& trackIdentifier);
    std::string get_path_string(const GroupIdentifier& groupIdentifier);
    std::string get_path_string(const ObjectIdentifier& objectIdentifier);

    bool store_object(const GroupIdentifier& groupIdentifier,
                      ObjectId objectId,
                      std::string&& object);

    bool store_object(std::shared_ptr<GroupHandle> groupHandleWeakPtr,
                      ObjectId objectId,
                      std::string&& object);

public:
    std::weak_ptr<TrackHandle>
    add_track_identifier(std::vector<std::string> tracknamespace, std::string trackname)
    {
        std::unique_lock l(objectHierarchyMtx_);

        TrackIdentifier trackIdentifier(std::move(tracknamespace), std::move(trackname));

        auto [iter, success] =
        objectHierarchy_.try_emplace(trackIdentifier,
                                     std::make_shared<TrackHandle>(*this, trackIdentifier));

        return iter->second->weak_from_this();
    }

    ObjectOrStatus get_object(const ObjectIdentifier& objectIdentifier);
    std::weak_ptr<TrackHandle> get_track_handle(const TrackIdentifier& trackIdentifier);
    std::weak_ptr<GroupHandle> get_group_handle(const GroupIdentifier& groupIdentifier);

    std::optional<GroupId> get_first_group(const TrackIdentifier&);
    std::optional<ObjectId> get_first_object(const GroupIdentifier&);

    // last object might be concrete (stored) or just registered (expected to be
    // published, at some point)
    std::optional<ObjectId> get_latest_registered_object(const GroupIdentifier&);
    std::optional<ObjectId> get_latest_concrete_object(const GroupIdentifier&);

    std::optional<PublisherPriority>
    get_publisher_priority(const GroupIdentifier& groupIdentifier);

    // returns true if it could succesfully advance
    bool next(ObjectIdentifier& objectIdentifier, std::uint64_t advanceBy = 1);

    DataManager()
    {
        // Remove data directory
        std::filesystem::remove_all(DATA_DIRECTORY);
    }
};
} // namespace rvn
