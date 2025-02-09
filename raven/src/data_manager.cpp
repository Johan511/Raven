#include "strong_types.hpp"
#include <data_manager.hpp>
#include <fstream>

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
    void add_object(std::uint64_t objectId, std::string object)
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


    const auto& groupIdentifier = groupHandleSharedPtr->groupIdentifier_;
    return dataManager_.store_object(groupIdentifier, beginObjectId_ + numObjects_,
                                     std::move(object));
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


std::string DataManager::get_path_string(const TrackIdentifier& trackIdentifier)
{
    std::string pathString = DATA_DIRECTORY;
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

bool DataManager::store_object(const GroupIdentifier& groupIdentifier,
                               std::uint64_t objectId,
                               std::string&& object)
{
    std::string pathString = get_path_string(groupIdentifier) + std::to_string(objectId);

    std::ofstream file(pathString);
    if (!file.is_open())
        return false;

    file << std::move(object);
    return true;
}

std::optional<ObjectId> DataManager::get_first_object(const GroupIdentifier& groupIdentifier)
{
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(groupIdentifier);
    if (iter == objectHierarchy_.end())
        return std::nullopt;

    TrackHandle& trackHandle = *iter->second;
    std::shared_lock l2(trackHandle.groupHandlesMtx_);

    auto groupHandleIter = trackHandle.groupHandles_.find(groupIdentifier.groupId_);
    if (groupHandleIter == trackHandle.groupHandles_.end())
        return std::nullopt;

    GroupHandle& groupHandle = *groupHandleIter->second;
    std::shared_lock l3(groupHandle.objectIdsMtx_);

    if (groupHandle.objectIds_.empty())
        return std::nullopt;

    return ObjectId(*groupHandle.objectIds_.begin());
}

std::optional<GroupId> DataManager::get_first_group(const TrackIdentifier& trackIdentifier)
{
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(trackIdentifier);
    if (iter == objectHierarchy_.end())
        return std::nullopt;

    TrackHandle& trackHandle = *iter->second;
    std::shared_lock l2(trackHandle.groupHandlesMtx_);

    if (trackHandle.groupHandles_.empty())
        return std::nullopt;

    return trackHandle.groupHandles_.begin()->first;
}

std::optional<ObjectId> DataManager::get_latest_object(const GroupIdentifier& groupIdentifier)
{
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(groupIdentifier);
    if (iter == objectHierarchy_.end())
        return std::nullopt;

    TrackHandle& trackHandle = *iter->second;
    std::shared_lock l2(trackHandle.groupHandlesMtx_);

    auto groupHandleIter = trackHandle.groupHandles_.find(groupIdentifier.groupId_);
    if (groupHandleIter == trackHandle.groupHandles_.end())
        return std::nullopt;

    GroupHandle& groupHandle = *groupHandleIter->second;
    std::shared_lock l3(groupHandle.objectIdsMtx_);

    if (groupHandle.objectIds_.empty())
        return std::nullopt;

    return ObjectId(*groupHandle.objectIds_.rbegin() & (~(1ULL << 63)));
}

ObjectOrStatus DataManager::get_object(const ObjectIdentifier& objectIdentifier)
{
    std::shared_lock l(objectHierarchyMtx_);

    auto iter = objectHierarchy_.find(objectIdentifier);
    if (iter == objectHierarchy_.end())
        return DoesNotExist{ "Track does not exist" };

    TrackHandle& trackHandle = *iter->second;
    std::shared_lock l2(trackHandle.groupHandlesMtx_);

    auto groupHandleIter = trackHandle.groupHandles_.find(objectIdentifier.groupId_);
    if (groupHandleIter == trackHandle.groupHandles_.end())
        return DoesNotExist{ "Group does not exist" };

    GroupHandle& groupHandle = *groupHandleIter->second;
    std::shared_lock l3(groupHandle.objectIdsMtx_);

    if (!groupHandle.has_object_id(objectIdentifier.objectId_))
        return DoesNotExist{ "Object does not exist" };

    std::string pathString = get_path_string(objectIdentifier);
    std::ifstream file(pathString);
    if (!file.is_open())
        return NotFound{ "Object not found" };

    std::string object;
    std::move(file) >> object;

    return object;
}
}; // namespace rvn
