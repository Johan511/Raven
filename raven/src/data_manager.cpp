#include "serialization/messages.hpp"
#include "serialization/serialization.hpp"
#include "strong_types.hpp"
#include <atomic>
#include <cstdio>
#include <data_manager.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace depracated
{
class SubgroupHandle
{
    struct Comparator
    {
        bool operator()(std::uint64_t l, std::uint64_t r) const
        {
            l = l & (~(1ULL << 63)); //  mask of last bit
            r = r & (~(1ULL << 63)); //  mask of last bit

            return l < r;
        }
    };
    std::set<std::uint64_t, Comparator> objectIds_;

    class DataManager& dataManager_;

    void add_range(std::uint64_t l, std::uint64_t r)
    {
        r = r | (1ULL << 63); // set last bit

        auto [lIter, _1] = objectIds_.insert(l);
        auto [rIter, _2] = objectIds_.insert(r);

        // erase all entries between (lIter, rIter)
        objectIds_.erase(std::next(lIter), rIter);

        /*
            before lIter if we have another L type entry, erase the later
           one L_old------------------------------ L_new------------------
        */
        if (lIter != objectIds_.begin() && *std::prev(lIter) < l)
            // any range in between will be erased along with l_new
            objectIds_.erase(lIter);

        /*
            after rIter if we have another R type entry, erase the previous one
            --------------------------------R_old
            ------------------------R_new
        */
        auto rNext = std::next(rIter);
        if (rNext != objectIds_.end() && *rNext > r)
            // any range in between will be erased along with r_new will be deleted
            objectIds_.erase(rIter);
    }

    /*
        erases all elements in [l, r)
        if there are any holes in [l, r) (subrange with no entries)
        it ignores those holes
    */
    void remove_range(std::uint64_t l, std::uint64_t r)
    {
        r = r | (1ULL << 63); // set last bit

        auto lIter = objectIds_.lower_bound(l);
        auto rIter = objectIds_.lower_bound(r);

        objectIds_.erase(lIter, rIter);

        /*
            L_old------------------------------
                        L_del------------------
        */
        if (lIter != objectIds_.begin() && *std::prev(lIter) < l)
            // l is new end point
            objectIds_.insert(l | (~(1ULL << 63)));

        /*
            --------------------------------R_old
            ------------------------R_del
        */
        auto rNext = std::next(rIter);
        if (rNext != objectIds_.end() && *rNext > r)
            // r is new begin point
            objectIds_.insert(r & (~(1ULL << 63)));
    }

public:
    void add_object(std::uint64_t objectId, std::string)
    {
        add_range(objectId, objectId + 1);
        // TODO: store object somewhere
    }

    SubgroupHandle(DataManager& dataManager) : dataManager_(dataManager)
    {
    }
};
} // namespace depracated

namespace rvn
{
bool SubgroupHandle::add_object(std::string object)
{
    utils::ASSERT_LOG_THROW(beginObjectId_ < endObjectId_,
                            "Pushing more objects than allowed");

    auto groupHandleSharedPtr = groupHandle_.lock();
    // checks if group still exists
    if (!groupHandleSharedPtr)
        return false;

    bool storeReturn = dataManager_.store_object(groupHandleSharedPtr,
                                                 ObjectId(beginObjectId_ + numObjects_++),
                                                 std::move(object));

    // if store return is true, it means object has been succesfully stored
    // hb relationship between operations till now and num objects stored makes
    // sense
    groupHandleSharedPtr->numStoredObjects_.fetch_add(storeReturn, std::memory_order_relaxed);

    return storeReturn;
}

void SubgroupHandle::cap()
{
    auto groupHandleSharedPtr = groupHandle_.lock();
    // checks if group still exists
    if (!groupHandleSharedPtr)
        return;

    endObjectId_ = beginObjectId_ + ObjectId(numObjects_);

    std::unique_lock l(groupHandleSharedPtr->objectIdsMtx_);
    auto& objectIds = groupHandleSharedPtr->objectIds_;
    auto iter = objectIds.find(beginObjectId_);
    if (iter == objectIds.end())
        return;
    ++iter;

    // change the range of the subgroup in the group
    objectIds.erase(iter);
    objectIds.insert(endObjectId_.get() | (1ULL << 63));
}

std::optional<SubgroupHandle> SubgroupHandle::cap_and_next()
{
    auto groupHandleSharedPtr = groupHandle_.lock();
    // checks if group still exists
    if (!groupHandleSharedPtr)
        return {};

    endObjectId_ = beginObjectId_ + ObjectId(numObjects_);

    std::unique_lock l(groupHandleSharedPtr->objectIdsMtx_);
    auto& objectIds = groupHandleSharedPtr->objectIds_;
    auto iter = objectIds.find(beginObjectId_);
    if (iter == objectIds.end())
        return {};
    ++iter;

    // change the range of the subgroup in the group
    objectIds.erase(iter);
    objectIds.insert(endObjectId_.get() | (1ULL << 63));

    // Duplicated code from add_open_ended_subgroup
    std::uint64_t beginObjectId = groupHandleSharedPtr->objectIds_.empty() ?
                                  0 :
                                  *groupHandleSharedPtr->objectIds_.rbegin();
    beginObjectId &= (~(1ULL << 63)); // mask of last bit
    groupHandleSharedPtr->objectIds_.insert(beginObjectId);
    groupHandleSharedPtr->objectIds_.insert(std::numeric_limits<std::uint64_t>::max());

    return SubgroupHandle(groupHandleSharedPtr, dataManager_, ObjectId(beginObjectId),
                          ObjectId(std::numeric_limits<std::uint64_t>::max()));
}

TrackIdentifier::TrackIdentifier(std::vector<std::string> trackNamespace, std::string tname)
: trackIdentifierInternal_(
  std::make_shared<std::pair<std::vector<std::string>, std::string>>(std::move(trackNamespace),
                                                                     std::move(tname)))
{
}

GroupIdentifier::GroupIdentifier(TrackIdentifier trackIdentifier, GroupId groupId)
: TrackIdentifier(std::move(trackIdentifier)), groupId_(groupId)
{
}

ObjectIdentifier::ObjectIdentifier(TrackIdentifier trackIdentifier, GroupId groupId, ObjectId objectId)
: GroupIdentifier(std::move(trackIdentifier), groupId), objectId_(objectId)
{
}

ObjectIdentifier::ObjectIdentifier(GroupIdentifier groupIdentifier, ObjectId objectId)
: GroupIdentifier(groupIdentifier), objectId_(objectId)
{
}

SubGroupId ObjectIdentifier::get_subgroup_id(DataManager& dataManager) const
{
    if (subgroupId_.has_value())
        return *subgroupId_;

    auto groupHandle = dataManager.get_group_handle(*this).lock();

    if (groupHandle == nullptr)
        throw std::invalid_argument("GroupHandle is null");

    subgroupId_ = groupHandle->get_subgroup_id(objectId_);

    return *subgroupId_;
}

GroupHandle::GroupHandle(GroupIdentifier groupIdentifier,
                         PublisherPriority publisherPriority,
                         std::optional<std::chrono::milliseconds> deliveryTimeout,
                         DataManager& dataManagerHandle)
: groupIdentifier_(std::move(groupIdentifier)), publisherPriority_(publisherPriority),
  deliveryTimeout_(deliveryTimeout), dataManager_(dataManagerHandle)
{
    // create directory if it does not exist
    std::string pathString = dataManager_.get_path_string(groupIdentifier_);
    std::filesystem::create_directories(pathString);
}

SubgroupHandle GroupHandle::add_subgroup(std::uint64_t numElements)
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

SubgroupHandle GroupHandle::add_open_ended_subgroup()
{
    // writer lock
    std::unique_lock<std::shared_mutex> l(objectIdsMtx_);

    std::uint64_t beginObjectId = objectIds_.empty() ? 0 : *objectIds_.rbegin();
    beginObjectId &= (~(1ULL << 63)); // mask of last bit
    objectIds_.insert(beginObjectId);
    objectIds_.insert(std::numeric_limits<std::uint64_t>::max());

    return SubgroupHandle(weak_from_this(), dataManager_, ObjectId(beginObjectId),
                          ObjectId(std::numeric_limits<std::uint64_t>::max()));
}

bool GroupHandle::has_object_id(ObjectId objectId)
{
    // reader lock
    std::shared_lock<std::shared_mutex> l(objectIdsMtx_);

    auto iter = objectIds_.upper_bound(objectId.get());
    if (iter == objectIds_.begin())
        return false;

    return *std::prev(iter) <= objectId.get();
}

std::uint64_t GroupHandle::num_objects_in_range(ObjectId left, ObjectId right)
{
    // reader lock
    std::shared_lock<std::shared_mutex> l(objectIdsMtx_);

    std::uint64_t numObjects = 0;

    if (objectIds_.empty())
        return numObjects;

    // beginning objectId of the subgroup left is in
    auto subGroupBeginIter = --objectIds_.upper_bound(left.get());
    while (subGroupBeginIter != objectIds_.end())
    {
        auto subGroupEndIter = std::next(subGroupBeginIter);
        ObjectId endBound = ObjectId(*subGroupEndIter & (~(1ULL << 63)));

        if (endBound >= right)
        {
            numObjects += right.get() - left.get();
            break;
        }
        else
        {
            numObjects += endBound.get() - left.get();
            subGroupBeginIter = std::next(subGroupEndIter);
        }
    }

    return numObjects;
}

TrackHandle::TrackHandle(DataManager& dataManagerHandle, TrackIdentifier trackIdentifier)
: dataManager_(dataManagerHandle), trackIdentifier_(std::move(trackIdentifier))
{
    // create directory if it does not exist
    std::string pathString = dataManager_.get_path_string(trackIdentifier_);
    std::filesystem::create_directories(pathString);
}

std::string DataManager::get_path_string(const TrackIdentifier& trackIdentifier)
{
    std::string pathString = std::string(DATA_DIRECTORY);
    for (const auto& ns : trackIdentifier.tnamespace())
        pathString += ns + "/";
    pathString += trackIdentifier.tname() + "/";
    return pathString;
}

std::string DataManager::get_path_string(const GroupIdentifier& groupIdentifier)
{
    return get_path_string(static_cast<const TrackIdentifier&>(groupIdentifier)) +
           std::to_string(groupIdentifier.groupId_) + "/";
}

std::string DataManager::get_path_string(const ObjectIdentifier& objectIdentifier)
{
    return get_path_string(static_cast<const GroupIdentifier&>(objectIdentifier)) +
           std::to_string(objectIdentifier.objectId_);
}

// returns true if object has been stored
bool DataManager::store_object(std::shared_ptr<GroupHandle> groupHandleSharedPtr,
                               ObjectId objectId,
                               std::string&& object)
{
    StreamHeaderSubgroupObject subgroupObject;
    subgroupObject.objectId_ = objectId;
    subgroupObject.payload_ = object;

    QUIC_BUFFER* quicBuffer = serialization::serialize(subgroupObject);

    groupHandleSharedPtr->groupCache_.write(
    [quicBuffer, objectId](auto& cache)
    { cache.emplace(ObjectId(objectId), quicBuffer); });

    std::string pathString =
    get_path_string(groupHandleSharedPtr->groupIdentifier_) + std::to_string(objectId);
    std::string tempFilePath = pathString + ".temp";

    std::ofstream file(tempFilePath);
    if (!file.is_open())
        return false;
    file << subgroupObject.payload_;

    groupHandleSharedPtr->objectWaitSignals_.write(
    [&objectId](auto& waitSignals)
    {
        auto iter = waitSignals.find(objectId);
        if (iter != waitSignals.end())
            iter->second->store(ObjectWaitStatus::Ready, std::memory_order_release);
    });
    std::filesystem::rename(tempFilePath, pathString);
    return true;
}

bool DataManager::store_object(const GroupIdentifier& groupIdentifier,
                               ObjectId objectId,
                               std::string&& object)
{
    auto groupHandleSharedPtr = get_group_handle(groupIdentifier).lock();
    if (groupHandleSharedPtr == nullptr)
        return false;

    return store_object(std::move(groupHandleSharedPtr), objectId, std::move(object));
}

std::optional<ObjectId> DataManager::get_first_object(const GroupIdentifier& groupIdentifier)
{
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(groupIdentifier);
    if (iter == objectHierarchy_.end())
        return std::nullopt;

    auto trackHandleSharedPtr = iter->second;
    l = std::shared_lock(trackHandleSharedPtr->groupHandlesMtx_);

    auto groupHandleIter =
    trackHandleSharedPtr->groupHandles_.find(groupIdentifier.groupId_);
    if (groupHandleIter == trackHandleSharedPtr->groupHandles_.end())
        return std::nullopt;

    std::shared_ptr<GroupHandle> groupHandleSharedPtr = groupHandleIter->second;
    l = std::shared_lock(groupHandleSharedPtr->objectIdsMtx_);

    if (groupHandleSharedPtr->objectIds_.empty())
        return std::nullopt;

    return ObjectId(*groupHandleSharedPtr->objectIds_.begin());
}

std::optional<GroupId> DataManager::get_first_group(const TrackIdentifier& trackIdentifier)
{
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(trackIdentifier);
    if (iter == objectHierarchy_.end())
        return std::nullopt;

    std::shared_ptr<TrackHandle> trackHandleSharedPtr = iter->second;
    l = std::shared_lock(trackHandleSharedPtr->groupHandlesMtx_);

    if (trackHandleSharedPtr->groupHandles_.empty())
        return std::nullopt;

    return trackHandleSharedPtr->groupHandles_.begin()->first;
}

std::optional<ObjectId>
DataManager::get_latest_registered_object(const GroupIdentifier& groupIdentifier)
{
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(groupIdentifier);
    if (iter == objectHierarchy_.end())
        return std::nullopt;

    std::shared_ptr<TrackHandle> trackHandleSharedPtr = iter->second;
    l = std::shared_lock(trackHandleSharedPtr->groupHandlesMtx_);

    auto groupHandleIter =
    trackHandleSharedPtr->groupHandles_.find(groupIdentifier.groupId_);
    if (groupHandleIter == trackHandleSharedPtr->groupHandles_.end())
        return std::nullopt;

    std::shared_ptr<GroupHandle> groupHandleSharedPtr = groupHandleIter->second;
    l = std::shared_lock(groupHandleSharedPtr->objectIdsMtx_);

    if (groupHandleSharedPtr->objectIds_.empty())
        return std::nullopt;

    return ObjectId(*groupHandleSharedPtr->objectIds_.rbegin() & (~(1ULL << 63)));
}

std::optional<ObjectId>
DataManager::get_latest_concrete_object(const GroupIdentifier& groupIdentifier)
{
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(groupIdentifier);
    if (iter == objectHierarchy_.end())
        return std::nullopt;

    std::shared_ptr<TrackHandle> trackHandleSharedPtr = iter->second;
    l = std::shared_lock(trackHandleSharedPtr->groupHandlesMtx_);

    auto groupHandleIter =
    trackHandleSharedPtr->groupHandles_.find(groupIdentifier.groupId_);
    if (groupHandleIter == trackHandleSharedPtr->groupHandles_.end())
        return std::nullopt;

    std::shared_ptr<GroupHandle> groupHandleSharedPtr = groupHandleIter->second;
    l = std::shared_lock(groupHandleSharedPtr->objectIdsMtx_);

    auto numObjects =
    groupHandleSharedPtr->numStoredObjects_.load(std::memory_order_acquire);
    if (numObjects == 0)
        return std::nullopt;

    // Race condition of someone deleting the object after the empty check is
    // not possible because of the shared_lock we hold
    return ObjectId(numObjects - 1);
}

std::optional<PublisherPriority>
DataManager::get_publisher_priority(const GroupIdentifier& groupIdentifier)
{
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(groupIdentifier);
    if (iter == objectHierarchy_.end())
        return std::nullopt;

    std::shared_ptr<TrackHandle> trackHandleSharedPtr = iter->second;
    l = std::shared_lock(trackHandleSharedPtr->groupHandlesMtx_);

    auto groupHandleIter =
    trackHandleSharedPtr->groupHandles_.find(groupIdentifier.groupId_);
    if (groupHandleIter == trackHandleSharedPtr->groupHandles_.end())
        return std::nullopt;

    return groupHandleIter->second->publisherPriority_;
}

ObjectOrStatus DataManager::get_object(const ObjectIdentifier& objectIdentifier)
{
    // we have reader lock at each step in hierarchy
    // so can be sure that nothing will be deleted (needs writer lock)
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(objectIdentifier);
    if (iter == objectHierarchy_.end())
        return DoesNotExist{ "Track does not exist" };

    auto trackHandleSharedPtr = iter->second;
    l = std::shared_lock(trackHandleSharedPtr->groupHandlesMtx_);

    auto groupHandleIter =
    trackHandleSharedPtr->groupHandles_.find(objectIdentifier.groupId_);

    if (groupHandleIter == trackHandleSharedPtr->groupHandles_.end())
        return DoesNotExist{ "Group does not exist" };

    std::shared_ptr<GroupHandle> groupHandleSharedPtr = groupHandleIter->second;
    l = std::shared_lock(groupHandleSharedPtr->objectIdsMtx_);

    if (!groupHandleSharedPtr->has_object_id(objectIdentifier.objectId_))
        return DoesNotExist{ "Object does not exist" };

    QUIC_BUFFER* quicBuffer = groupHandleSharedPtr->groupCache_.read(
    [&objectIdentifier](const auto& groupCache) -> QUIC_BUFFER*
    {
        auto iter = groupCache.find(objectIdentifier.objectId_);
        if (iter != groupCache.end())
            return iter->second;
        return nullptr;
    });

    if (quicBuffer != nullptr)
        return std::make_tuple(quicBuffer, groupHandleSharedPtr->deliveryTimeout_);

    std::string pathString = get_path_string(objectIdentifier);
    std::ifstream file(pathString);
    if (!file.is_open())
    {
        return groupHandleSharedPtr->objectWaitSignals_.write(
        [&objectIdentifier](auto& waitSignals)
        {
            auto [iter, success] =
            waitSignals.try_emplace(objectIdentifier.objectId_,
                                    std::make_shared<std::atomic<ObjectWaitStatus>>(
                                    ObjectWaitStatus::Wait));

            return iter->second;
        });
    }

    std::string object(std::istreambuf_iterator<char>(file), {});

    StreamHeaderSubgroupObject subgroupObject;
    subgroupObject.objectId_ = objectIdentifier.objectId_;
    subgroupObject.payload_ = std::move(object);

    quicBuffer = serialization::serialize(subgroupObject);

    groupHandleSharedPtr->groupCache_.write(
    [quicBuffer, &objectIdentifier](auto& cache)
    { cache.emplace(objectIdentifier.objectId_, quicBuffer); });

    return std::make_tuple(quicBuffer, groupHandleSharedPtr->deliveryTimeout_);
}

bool DataManager::next(ObjectIdentifier& objectIdentifier, std::uint64_t advanceBy)
{
    // we have reader lock at each step in hierarchy
    // so can be sure that nothing will be deleted (needs writer lock)
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(objectIdentifier);
    if (iter == objectHierarchy_.end())
        return false;

    auto trackHandleSharedPtr = iter->second;
    l = std::shared_lock(trackHandleSharedPtr->groupHandlesMtx_);

    auto groupHandleIter =
    trackHandleSharedPtr->groupHandles_.find(objectIdentifier.groupId_);
    if (groupHandleIter == trackHandleSharedPtr->groupHandles_.end())
        return false;

    // clang-format off
    // NOTE: We need lock at both group handles level and also within group handles
    // Reason: What if the track handle expires while we are searching? 
    // We need groupHandlesMtx_ because we are doing iterating over it
    // clang-format on
    std::shared_lock l3(groupHandleIter->second->objectIdsMtx_);

    while (advanceBy > 0)
    {
        ObjectId advancedObjectId = objectIdentifier.objectId_ + ObjectId(advanceBy);
        if (!groupHandleIter->second->has_object_id(advancedObjectId))
        {
            // if we have reached end of group
            // subtrack advanceBy by number of objects in current group
            advanceBy -=
            groupHandleIter->second->num_objects_in_range(objectIdentifier.objectId_);

            l3.unlock();

            auto nextGroupIter = groupHandleIter;
            do
            {
                nextGroupIter = std::next(nextGroupIter);
            } while (nextGroupIter != trackHandleSharedPtr->groupHandles_.end() &&
                     // non empty group
                     nextGroupIter->second->num_objects_in_range() > 0);

            if (nextGroupIter == trackHandleSharedPtr->groupHandles_.end())
                return false;

            // advance to next objectId
            groupHandleIter = nextGroupIter;
            l3 = std::shared_lock(groupHandleIter->second->objectIdsMtx_);

            // set it to first object in next group
            objectIdentifier.groupId_ = groupHandleIter->first;
            objectIdentifier.objectId_ =
            ObjectId(*groupHandleIter->second->objectIds_.begin());
        }
        else
        {
            objectIdentifier.objectId_ = advancedObjectId;
            break;
        }
    }

    return true;
}

std::weak_ptr<TrackHandle> DataManager::get_track_handle(const TrackIdentifier& trackIdentifier)
{
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(trackIdentifier);
    if (iter == objectHierarchy_.end())
        return {};

    return iter->second;
}

std::weak_ptr<GroupHandle> DataManager::get_group_handle(const GroupIdentifier& groupIdentifier)
{
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(groupIdentifier);
    if (iter == objectHierarchy_.end())
        return {};

    auto trackHandleSharedPtr = iter->second;
    l = std::shared_lock(trackHandleSharedPtr->groupHandlesMtx_);

    auto groupHandleIter =
    trackHandleSharedPtr->groupHandles_.find(groupIdentifier.groupId_);
    if (groupHandleIter == trackHandleSharedPtr->groupHandles_.end())
        return {};

    return groupHandleIter->second;
}

}; // namespace rvn
