#pragma once
#include <boost/functional/hash.hpp>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <shared_mutex>
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
subgroups are not used to identify an object (hence not part of Object Identifier)

Subgroups only help as hints about if the objects should/need not be sent on the same stream

Each subgroup -> one stream

DataManager.add_track_idenifier => adds a new track and returns handle (weak_ptr<TrackHandle>) to it
                                    (return nullptr if already exists)
                                    Stores a mapping from TrackIdentifier to TrackHandle




In each TrackHandle => Stores weak_ptr to TrackIdentifier
Why not shared_ptr =>
1. Intuitive Reason: TrackHandle only references TrackIdenitifer,
                    if lifetime of TrackIdentifier ends, TrackHandle should not be able to access it
2. Programmatic Reason: We have mapping from TrackIdentifier to TrackHandle in DataManager
                        If we we decide to delete a track on T1
                        and T2 still has reference to it, Track will not be deleted and user will never know we deleted it

*NOT* a reason:
Memory leak due to cyclic shared_ptr dependency, because there is no cyclic shared_ptr dependency here

Consider this case:
shared_ptr<TrackIdenitifier> => use_count = 3: from DataManager, TrackHandle in DataManager and TrackHandle in user code
shared_ptr<TrackHandle> => use_count = 2: from TrackHandle in DataManager and TrackHandle in user code

User deletes Track
Deletes the pair <TrackIdentifier, std::shared_ptr<TrackHandle>> from DataManager
use_count of TrackHandle decreases by 1 and use_count of TrackIdentifier decreases by 2 (key deleted and reference by value also deleted)

Finally when user deletes TrackHandler, use_count of TrackHandler becomes 0 and is cleaned up along with TrackIdentifier


Similarly TrackHandle has mapping from GroupId to std::shared_ptr<GroupHandle>
When we add group, it returns std::weak_ptr<GroupHandle>

And similarly
And when we add Subgroup to GroupHandle, it returns std::weak_ptr<SubgroupHandle>

GroupHandle does not maintain each SubGroup or Object seperately, but rather maintains the ObjectId ranges for each SubGroup

When we add an object to a SubGroup, We call DataManager.add_object with the GroupIdentifier which stores the object



Object Identifier has a weak reference to TrackIdentifier (because TrackIdentifier is a large object and we want to avoid copying it)
Why not shared_ptr? Object Identifier is not responsible for the lifetime of TrackIdentifier, if TrackIdentifier has expired, we don't want ObjectIdentifier

NOTE: shared_ptr of any identifier or handle must be created in this file internally
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

using ObjectOrStatus = std::variant<std::string, NotFound, DoesNotExist>;

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

    void cap();
};

// We do not allow for objects to be deleted
class GroupHandle : public std::enable_shared_from_this<GroupHandle>
{
    friend class DataManager;
    friend class TrackHandle;
    friend class SubgroupHandle;
    class DataManager& dataManager_;
    GroupIdentifier groupIdentifier_;

    template <typename... Args>
    static std::shared_ptr<GroupHandle> create(Args&&... args)
    {
        auto ptr = new GroupHandle(std::forward<Args>(args)...);
        return std::shared_ptr<GroupHandle>(ptr);
    }

private:
    struct Comparator
    {
        bool operator()(std::uint64_t l, std::uint64_t r) const
        {
            l = l & (~(1ULL << 63)); //  mask of last bit
            r = r & (~(1ULL << 63)); //  mask of last bit

            return l < r;
        }
    };

    std::shared_mutex objectIdsMtx_;
    std::set<std::uint64_t, Comparator> objectIds_;

    GroupHandle(GroupIdentifier groupIdentifier, DataManager& dataManagerHandle)
    : groupIdentifier_(std::move(groupIdentifier)), dataManager_(dataManagerHandle)
    {
    }

public:
    SubgroupHandle add_subgroup(std::uint64_t numElements)
    {
        // writer lock
        std::unique_lock<std::shared_mutex> l(objectIdsMtx_);

        std::uint64_t beginObjectId = objectIds_.empty() ? 0 : *objectIds_.rbegin();
        beginObjectId &= (~(1ULL << 63)); // mask of last bit
        objectIds_.insert(beginObjectId);
        objectIds_.insert((beginObjectId + numElements) | (1ULL << 63));

        return SubgroupHandle(weak_from_this(), dataManager_, ObjectId(beginObjectId),
                              ObjectId(beginObjectId + numElements));
    };

    SubgroupHandle add_open_ended_subgroup()
    {
        // writer lock
        std::unique_lock<std::shared_mutex> l(objectIdsMtx_);

        std::uint64_t beginObjectId = objectIds_.empty() ? 0 : *objectIds_.rbegin();
        beginObjectId &= (~(1ULL << 63)); // mask of last bit
        objectIds_.insert(beginObjectId);
        objectIds_.insert(std::numeric_limits<std::uint64_t>::max());

        return SubgroupHandle(weak_from_this(), dataManager_,
                              ObjectId(beginObjectId), ObjectId(beginObjectId));
    }

    bool has_object_id(ObjectId objectId)
    {
        // reader lock
        std::shared_lock<std::shared_mutex> l(objectIdsMtx_);

        auto iter = objectIds_.lower_bound(objectId.get());
        if (iter == objectIds_.begin())
            return false;

        return *std::prev(iter) <= objectId.get();
    }

    std::pair<ObjectId, ObjectId> get_range()
    {
        // reader lock
        std::shared_lock<std::shared_mutex> l(objectIdsMtx_);

        return { ObjectId(*objectIds_.begin()),
                 ObjectId(*objectIds_.rbegin() & (~(1ULL << 63))) };
    }
};

class TrackHandle : public std::enable_shared_from_this<TrackHandle>
{
    friend class DataManager;
    friend class GroupHandle;
    class DataManager& dataManager;

    TrackIdentifier trackIdentifier_;

    std::shared_mutex groupHandlesMtx_;
    std::map<GroupId, std::shared_ptr<GroupHandle>> groupHandles_;

    std::weak_ptr<GroupHandle> add_group(GroupId groupId)
    {
        // writer lock
        std::unique_lock<std::shared_mutex> l(groupHandlesMtx_);

        auto [iter, success] =
        groupHandles_.try_emplace(groupId, GroupHandle::create(GroupIdentifier(trackIdentifier_, groupId),
                                                               dataManager));

        return iter->second->weak_from_this();
    }

    TrackHandle(DataManager& dataManagerHandle, TrackIdentifier trackIdentifier)
    : dataManager(dataManagerHandle), trackIdentifier_(std::move(trackIdentifier))
    {
    }
};


// TODO: Read and make use of https://www.sqlite.org/fasterthanfs.html

class DataManager
{
    friend class SubgroupHandle;

    std::shared_mutex objectHierarchyMtx_;
    std::unordered_map<TrackIdentifier, std::shared_ptr<TrackHandle>, TrackIdentifier::Hash, TrackIdentifier::Equal> objectHierarchy_;

    std::string get_path_string(const TrackIdentifier& trackIdentifier);
    std::string get_path_string(const GroupIdentifier& groupIdentifier);
    std::string get_path_string(const ObjectIdentifier& objectIdentifier);

    bool store_object(const GroupIdentifier& groupIdentifier,
                      std::uint64_t objectId,
                      std::string&& object);

public:
    template <typename... Args>
    std::weak_ptr<TrackHandle> add_track_identifier(Args&&... args)
    {
        auto trackIdentifier =
        std::make_shared<TrackIdentifier>(std::forward<Args>(args)...);

        auto [iter, success] =
        objectHierarchy_.try_emplace(trackIdentifier,
                                     std::make_shared<TrackHandle>(*this, trackIdentifier));

        return success ? iter->second->weak_from_this() :
                         std::make_shared<TrackHandle>(nullptr);
    }


    ObjectOrStatus get_object(const ObjectIdentifier& objectIdentifier);

    std::optional<GroupId> get_first_group(const TrackIdentifier&);
    std::optional<ObjectId> get_first_object(const GroupIdentifier&);
    std::optional<ObjectId> get_latest_object(const GroupIdentifier&);
};
} // namespace rvn
