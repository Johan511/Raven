#include "strong_types.hpp"
#include <atomic>
#include <cstdio>
#include <data_manager.hpp>
#include <filesystem>
#include <memory>

namespace rvn
{


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

TrackHandle::TrackHandle(DataManager& dataManagerHandle,
                         TrackIdentifier trackIdentifier,
                         PublisherPriority publisherPriority,
                         std::optional<std::chrono::milliseconds> deliveryTimeout)
: dataManager_(dataManagerHandle), trackIdentifier_(std::move(trackIdentifier)),
  publisherPriority_(publisherPriority), deliveryTimeout_(deliveryTimeout),
  // no update yet, waiting for it
  updateSignal_(std::make_shared<std::atomic<WaitStatus>>(WaitStatus::Wait))
{
    // create directory if it does not exist
    std::string pathString = dataManager_.get_path_string(trackIdentifier_);
    std::filesystem::create_directories(pathString);
}

EnrichedObjectOrWait TrackHandle::get_first_object()
{
    std::unique_lock l(mtx_);

    auto beginObjectIter = objects_.begin();

    if (beginObjectIter != objects_.end())
    {
        auto [groupId, objectId] = beginObjectIter->first;
        return std::make_tuple(groupId, objectId, beginObjectIter->second);
    }
    else
        return updateSignal_;
}

EnrichedObjectOrWait TrackHandle::get_next_object(const ObjectIdentifier& objectIdentifier)
{
    std::unique_lock l(mtx_);

    auto iter = objects_.upper_bound(
    std::make_tuple(objectIdentifier.groupId_, objectIdentifier.objectId_));

    if (iter != objects_.end())
    {
        auto [groupId, objectId] = iter->first;
        return std::make_tuple(groupId, objectId, iter->second);
    }
    else
        return updateSignal_;
}


// returns EnrichedObject only if there is object later than oid
EnrichedObjectOrWait
TrackHandle::get_latest_object(const std::optional<ObjectIdentifier>& oid)
{
    std::unique_lock l(mtx_);

    auto latestObjectIter = objects_.rbegin();

    if (latestObjectIter != objects_.rend())
        // either oid has no value or the latest object is later than oid
        if (!oid.has_value() || std::make_tuple(oid->groupId_, oid->objectId_) <
                                latestObjectIter->first)
        {
            auto [groupId, objectId] = latestObjectIter->first;
            return std::make_tuple(groupId, objectId, latestObjectIter->second);
        }
    return updateSignal_;
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

std::weak_ptr<TrackHandle>
DataManager::add_track_identifier(std::vector<std::string> tracknamespace,
                                  std::string trackname,
                                  PublisherPriority publisherPriority,
                                  std::optional<std::chrono::milliseconds> deliveryTimeout)
{
    auto trackIdentifier =
    TrackIdentifier(std::move(tracknamespace), std::move(trackname));

    auto [trackHandleIter, success] = trackHandles_.write(
    [&](auto& trackHandles)
    {
        return trackHandles.try_emplace(trackIdentifier,
                                        std::make_shared<TrackHandle>(*this, trackIdentifier, publisherPriority,
                                                                      deliveryTimeout));
    });

    if (success)
    {
        trackWaitSignals_.write(
        [&trackIdentifier](auto& trackWaitSignals)
        {
            auto waitSignalIter = trackWaitSignals.find(trackIdentifier);
            if (waitSignalIter != trackWaitSignals.end())
            {
                waitSignalIter->second->store(WaitStatus::Ready, std::memory_order::release);
                trackWaitSignals.erase(waitSignalIter);
            }
        });
    }

    return trackHandleIter->second->weak_from_this();
}

std::variant<std::shared_ptr<TrackHandle>, WaitSignal>
DataManager::get_track_handle(const TrackIdentifier& trackIdentifier)
{
    auto [trackHandleIter, isDereferenceable] = trackHandles_.read(
    [&trackIdentifier](const auto& trackHandles)
    {
        auto iter = trackHandles.find(trackIdentifier);
        return std::make_pair(iter, iter != trackHandles.end());
    });

    if (!isDereferenceable)
    {
        return trackWaitSignals_.write(
        [&trackIdentifier](auto& trackWaitSignals)
        {
            auto [iter, success] =
            trackWaitSignals.try_emplace(trackIdentifier,
                                         std::make_shared<std::atomic<WaitStatus>>(
                                         WaitStatus::Wait));

            return iter->second;
        });
    }
    return trackHandleIter->second;
}

PublisherPriority
DataManager::get_track_publisher_priority(const TrackIdentifier& trackIdentifier)
{
    auto [trackHandleIter, isDereferenceable] = trackHandles_.read(
    [&trackIdentifier](const auto& trackHandles)
    {
        auto iter = trackHandles.find(trackIdentifier);
        return std::make_pair(iter, iter != trackHandles.end());
    });

    if (isDereferenceable)
        return trackHandleIter->second->publisherPriority_;
    else
    {
        exit(1);
    }
}
}; // namespace rvn
