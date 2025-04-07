/////////////////////////////////////////////
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <unistd.h>
#include <variant>
/////////////////////////////////////////////
#include <contexts.hpp>
#include <data_manager.hpp>
#include <msquic.h>
#include <serialization/messages.hpp>
#include <serialization/serialization.hpp>
#include <subscription_manager.hpp>
#include <timer_wheel.hpp>
#include <utilities.hpp>
/////////////////////////////////////////////

namespace rvn
{

MinorSubscriptionState::MinorSubscriptionState(SubscriptionState& subscriptionState,
                                               ObjectIdentifier objectToSend,
                                               std::optional<ObjectIdentifier> lastObjectToBeSent,
                                               bool mustBeSent,
                                               std::optional<std::chrono::milliseconds> deliveryTimeout)
: subscriptionState_(std::addressof(subscriptionState)),
  objectToSend_(std::move(objectToSend)),
  lastObjectToBeSent_(std::move(lastObjectToBeSent)), mustBeSent_(mustBeSent),
  subscribeDeliveryTimeout(deliveryTimeout)
{
}

bool MinorSubscriptionState::is_waiting_for_object()
{
    if (!objectWaitSignal_.has_value())
        // not waiting on object to be ready
        return true;

    // we wait on the flag, only is flag is ready, we do acquire operation
    // might have performance benefits on weaker memory models (ARM, POWERPC...)
    return (*objectWaitSignal_)->load(std::memory_order_relaxed) == ObjectWaitStatus::Ready;

    // We only return that it is true, we have to reset the flag in the
    // fulfill_some_minor function and also have an acquire load on the flag
}


// returns true if fulfilling is done
FulfillSomeReturn MinorSubscriptionState::fulfill_some_minor()
{
    if (objectWaitSignal_.has_value())
    {
        // we have to reset the flag
        (*objectWaitSignal_)->load(std::memory_order_acquire);
        objectWaitSignal_.reset();
    }

    auto connectionStateSharedPtr = subscriptionState_->connectionStateWeakPtr_.lock();
    if (!connectionStateSharedPtr)
        return SubscriptionStateErr::ConnectionExpired{};

    auto objectOrStatus = subscriptionState_->dataManager_->get_object(objectToSend_);

    if (std::holds_alternative<DoesNotExist>(objectOrStatus))
        return SubscriptionStateErr::ObjectDoesNotExist{};
    else if (std::holds_alternative<ObjectWaitSignal>(objectOrStatus))
    {
        objectWaitSignal_ = std::move(std::get<ObjectWaitSignal>(objectOrStatus));
        return false;
    }
    else
    {
        auto [quicBuffer, objectDeliveryTimeout] = std::get<ObjectType>(objectOrStatus);

        if ((!mustBeSent_) && previouslySentObject_.has_value())
            connectionStateSharedPtr->abort_if_sending(*previouslySentObject_);

        if (!objectDeliveryTimeout)
            // now both have value, or neither has value
            objectDeliveryTimeout = subscribeDeliveryTimeout;

        // need to check only one of them for value
        if (subscribeDeliveryTimeout)
            // if subscriber and object both mention a delivery timeout, we need to take the min of the 2
            if (*objectDeliveryTimeout > *subscribeDeliveryTimeout)
                *objectDeliveryTimeout = *subscribeDeliveryTimeout;


        QUIC_STATUS status =
        connectionStateSharedPtr->send_object(objectToSend_, quicBuffer, objectDeliveryTimeout);
        if (QUIC_FAILED(status))
            return SubscriptionStateErr::ConnectionExpired{};

        if (previouslySentObject_.has_value())
        {
            // we do not want to copy because copying track identifier is rather
            // expensive operation (seq cst atomic add of shared_ptr)
            previouslySentObject_->groupId_ = objectToSend_.groupId_;
            previouslySentObject_->objectId_ = objectToSend_.objectId_;
        }
        else
            previouslySentObject_ = objectToSend_;

        bool canAdavance = subscriptionState_->dataManager_->next(objectToSend_);

        if (!canAdavance)
            return true;
        return (lastObjectToBeSent_.has_value() && objectToSend_ == *lastObjectToBeSent_);
    }
}

// returns true if fulfilling is done
FulfillSomeReturn SubscriptionState::fulfill_some()
{
    bool allFulfilled = true;
    auto beginIter = minorSubscriptionStates_.begin();
    auto endIter = minorSubscriptionStates_.end();

    for (auto traversalIter = beginIter; traversalIter != endIter; ++traversalIter)
    {
        // by default we assume that minor subscriptions is not fulfilled
        FulfillSomeReturn fulfillReturn = false;

        // not waiting on object to be ready or waiting on it and it is ready
        // basically the mathematical logical statement: (waiting -> ready)
        if (traversalIter->is_waiting_for_object())
            fulfillReturn = traversalIter->fulfill_some_minor();

        if (std::holds_alternative<bool>(fulfillReturn))
        {
            if (std::get<bool>(fulfillReturn) == false)
            {
                if (beginIter != traversalIter)
                    *beginIter = std::move(*traversalIter);
                ++beginIter;
                allFulfilled = false;
            }
        }
        else
            return fulfillReturn;
    }

    minorSubscriptionStates_.erase(beginIter, endIter);
    return allFulfilled;
}


FulfillSomeReturn
SubscriptionState::add_group_subscription(const GroupHandle& groupHandle,
                                          bool mustBeSent,
                                          std::optional<std::chrono::milliseconds> deliveryTimeout,
                                          std::optional<ObjectId> beginObjectId,
                                          std::optional<ObjectId> endObjectId)
{
    if (beginObjectId == std::nullopt)
        beginObjectId = dataManager_->get_first_object(groupHandle.groupIdentifier_);
    if (beginObjectId == std::nullopt)
        return SubscriptionStateErr::ObjectDoesNotExist{};

    if (endObjectId == std::nullopt)
        endObjectId =
        dataManager_->get_latest_registered_object(groupHandle.groupIdentifier_);
    if (endObjectId == std::nullopt)
        return SubscriptionStateErr::ObjectDoesNotExist{};

    minorSubscriptionStates_
    .emplace_back(*this, ObjectIdentifier(groupHandle.groupIdentifier_, *beginObjectId),
                  ObjectIdentifier(groupHandle.groupIdentifier_, *endObjectId),
                  mustBeSent, deliveryTimeout);

    return false;
}


SubscriptionState::SubscriptionState(std::weak_ptr<ConnectionState>&& connectionState,
                                     DataManager& dataManager,
                                     SubscriptionManager& subscriptionManager,
                                     SubscribeMessage subscriptionMessage)
: connectionStateWeakPtr_(std::move(connectionState)),
  dataManager_(std::addressof(dataManager)),
  subscriptionManager_(std::addressof(subscriptionManager)),
  subscriptionMessage_(std::move(subscriptionMessage)), cleanup_(false)
{
    auto filterType = subscriptionMessage_.filterType_;
    auto connectionStateSharedPtr = connectionStateWeakPtr_.lock();

    if (!connectionStateSharedPtr)
    {
        subscriptionManager_->mark_subscription_cleanup(*this);
        return;
    }

    auto trackIdentifier =
    *connectionStateSharedPtr->alias_to_identifier(subscriptionMessage_.trackAlias_);

    auto deliveryTimeoutParamOpt =
    subscriptionMessage_.get_parameter<DeliveryTimeoutParameter>();
    std::optional<std::chrono::milliseconds> deliveryTimeoutOpt;
    if (deliveryTimeoutParamOpt.has_value())
        deliveryTimeoutOpt = deliveryTimeoutParamOpt->timeout_;

    switch (filterType)
    {
        case SubscribeFilterType::LatestGroup:
        {
            std::optional<GroupId> currGroupOpt =
            connectionStateSharedPtr->get_current_group(subscriptionMessage_.trackAlias_);

            if (!currGroupOpt.has_value())
                currGroupOpt = dataManager_->get_first_group(trackIdentifier);

            if (!currGroupOpt.has_value())
            {
                subscriptionManager_->notify_subscription_error(*this);
                return;
            }

            std::weak_ptr<GroupHandle> groupHandle =
            dataManager_->get_group_handle(GroupIdentifier(trackIdentifier, *currGroupOpt));

            if (auto groupHandleSharedPtr = groupHandle.lock())
                add_group_subscription(*groupHandleSharedPtr, true);
            else
            {
                subscriptionManager_->notify_subscription_error(*this);
                return;
            }

            break;
        }
        case SubscribeFilterType::LatestObject:
        {
            std::optional<GroupId> currGroupOpt =
            connectionStateSharedPtr->get_current_group(subscriptionMessage_.trackAlias_);

            if (!currGroupOpt.has_value())
                currGroupOpt = dataManager_->get_first_group(trackIdentifier);

            if (!currGroupOpt.has_value())
            {
                subscriptionManager_->notify_subscription_error(*this);
                return;
            }

            std::weak_ptr<GroupHandle> groupHandle =
            dataManager_->get_group_handle(GroupIdentifier(trackIdentifier, *currGroupOpt));

            if (auto groupHandleSharedPtr = groupHandle.lock())
            {
                auto latestObjectOpt = dataManager_->get_latest_registered_object(
                groupHandleSharedPtr->groupIdentifier_);
                if (!latestObjectOpt.has_value())
                {
                    subscriptionManager_->notify_subscription_error(*this);
                    return;
                }

                add_group_subscription(*groupHandleSharedPtr, true,
                                       deliveryTimeoutOpt, latestObjectOpt);
            }
            else
            {
                subscriptionManager_->notify_subscription_error(*this);
                return;
            }

            break;
        }
        case SubscribeFilterType::AbsoluteStart:
        {
            std::weak_ptr<TrackHandle> trackHandle =
            dataManager_->get_track_handle(trackIdentifier);

            if (auto trackHandleSharedPtr = trackHandle.lock())
            {
                std::shared_lock l(trackHandleSharedPtr->groupHandlesMtx_);
                auto groupHandleIter = trackHandleSharedPtr->groupHandles_.find(
                subscriptionMessage_.start_->group_);

                if (groupHandleIter == trackHandleSharedPtr->groupHandles_.end())
                {
                    subscriptionManager_->notify_subscription_error(*this);
                    return;
                }

                add_group_subscription(*groupHandleIter->second, true, deliveryTimeoutOpt,
                                       subscriptionMessage_.start_->object_);
                ++groupHandleIter;
                for (; groupHandleIter != trackHandleSharedPtr->groupHandles_.end(); ++groupHandleIter)
                    add_group_subscription(*groupHandleIter->second, true);
            }
            else
            {
                subscriptionManager_->notify_subscription_error(*this);
                return;
            }
            break;
        }
        case SubscribeFilterType::AbsoluteRange:
        {
            std::weak_ptr<TrackHandle> trackHandle =
            dataManager_->get_track_handle(trackIdentifier);

            if (auto trackHandleSharedPtr = trackHandle.lock())
            {
                std::shared_lock l(trackHandleSharedPtr->groupHandlesMtx_);

                auto beginGroupHandleIter = trackHandleSharedPtr->groupHandles_.find(
                subscriptionMessage_.start_->group_);
                auto endGroupHandleIter = trackHandleSharedPtr->groupHandles_.find(
                subscriptionMessage_.end_->group_);

                if (beginGroupHandleIter == trackHandleSharedPtr->groupHandles_.end() ||
                    endGroupHandleIter == trackHandleSharedPtr->groupHandles_.end())
                {
                    subscriptionManager_->notify_subscription_error(*this);
                    return;
                }

                if (beginGroupHandleIter->first == endGroupHandleIter->first)
                {
                    add_group_subscription(*beginGroupHandleIter->second, true, deliveryTimeoutOpt,
                                           subscriptionMessage_.start_->object_,
                                           subscriptionMessage_.end_->object_);
                    return;
                }
                else
                {
                    add_group_subscription(*beginGroupHandleIter->second, true, deliveryTimeoutOpt,
                                           subscriptionMessage_.start_->object_);

                    for (auto groupHandleIter = std::next(beginGroupHandleIter);
                         groupHandleIter != endGroupHandleIter; ++groupHandleIter)
                        add_group_subscription(*groupHandleIter->second, true);

                    add_group_subscription(*endGroupHandleIter->second, true, std::nullopt,
                                           subscriptionMessage_.end_->object_);
                }
            }
            else
            {
                subscriptionManager_->notify_subscription_error(*this);
                return;
            }
            break;
        }
        case SubscribeFilterType::LatestPerGroupInTrack:
        {
            std::weak_ptr<TrackHandle> trackHandle =
            dataManager_->get_track_handle(trackIdentifier);

            if (auto trackHandleSharedPtr = trackHandle.lock())
            {
                std::shared_lock l(trackHandleSharedPtr->groupHandlesMtx_);
                for (auto& groupHandleIter : trackHandleSharedPtr->groupHandles_)
                    // TODO: update it such that mustBeSent is true for base layers
                    add_group_subscription(*groupHandleIter.second, false, deliveryTimeoutOpt,
                                           dataManager_->get_latest_concrete_object(
                                           groupHandleIter.second->groupIdentifier_));
            }
            else
            {
                subscriptionManager_->notify_subscription_error(*this);
                return;
            }
            break;
        }
        default:
        {
            utils::ASSERT_LOG_THROW(false, "Unknown filter type",
                                    utils::to_underlying(filterType));
        }
    }
}

void ThreadLocalState::operator()()
{
    while (true)
    {
        // relaxed load and store works because there is no data dependencies
        // with cleanup_ used to denate that destructor of SubscriptionManager
        // is called, subscription threads should exit now
        if (subscriptionManager_.cleanup_.load(std::memory_order_relaxed)) [[unlikely]]
            break;

        auto& subscriptionQueue_ = subscriptionManager_.subscriptionQueue_;

        // If we believe it there are pending subscriptions, dequeue them
        // Why are we doing size_approx? Because constructing weak_ptr is a rather expensive lock opertion
        // We want to do it only if we believe there are pending subscriptions
        if (subscriptionQueue_.size_approx() != 0)
        {
            std::tuple<std::weak_ptr<ConnectionState>, SubscribeMessage> subscriptionTuple;
            while (subscriptionQueue_.try_dequeue(subscriptionTuple))
            {
                auto connectionStateWeakPtr = std::move(std::get<0>(subscriptionTuple));
                auto subscriptionMessage = std::move(std::get<1>(subscriptionTuple));

                subscriptionStates_.emplace_back(std::move(connectionStateWeakPtr),
                                                 subscriptionManager_.dataManager_,
                                                 subscriptionManager_,
                                                 std::move(subscriptionMessage));
                if (subscriptionStates_.back().cleanup_)
                    subscriptionStates_.pop_back();
            }
        }

        auto beginIter = subscriptionStates_.begin();
        auto endIter = subscriptionStates_.end();

        for (auto traversalIter = beginIter; traversalIter != endIter; ++traversalIter)
        {
            auto fulfillReturn = traversalIter->fulfill_some();

            if (std::holds_alternative<bool>(fulfillReturn))
            // subscription is being fulfilled with no issues
            {
                if (std::get<bool>(fulfillReturn) == false)
                    // This unfortunately does not seem to be using move constructor though (according to perf report)
                    // TODO: debug the issue
                    *beginIter++ = std::move(*traversalIter);
            }
            else if (std::holds_alternative<SubscriptionStateErr::ConnectionExpired>(fulfillReturn))
            {
                // Nothing to be done
            }
            else if (std::holds_alternative<SubscriptionStateErr::ObjectDoesNotExist>(fulfillReturn))
                subscriptionManager_.notify_subscription_error(*traversalIter);
            else
                assert(false);
        }

        subscriptionStates_.erase(beginIter, endIter);
    }
}

SubscriptionManager::SubscriptionManager(DataManager& dataManager, std::size_t numThreads)
: dataManager_(dataManager), cleanup_(false)
{
    threadLocalStates_.reserve(numThreads);
    for (std::size_t i = 0; i < numThreads; i++)
    {
        threadLocalStates_.emplace_back(*this);
        threadPool_.emplace_back(threadLocalStates_.back());
    }
}

SubscriptionManager::~SubscriptionManager()
{
    // to indicate to the threads to shutdown
    // relaxed load and store works because there is no data dependencies with cleanup_
    // it is just a single flag to be set, this might change if we are doing more complex things before setting cleanup
    cleanup_.store(true, std::memory_order_relaxed);
}

void SubscriptionManager::add_subscription(std::weak_ptr<ConnectionState> connectionStateWeakPtr,
                                           SubscribeMessage subscribeMessage)
{
    subscriptionQueue_.enqueue(std::make_tuple(std::move(connectionStateWeakPtr),
                                               std::move(subscribeMessage)));
}

void SubscriptionManager::mark_subscription_cleanup(SubscriptionState& subscriptionState)
{
    utils::LOG_EVENT(std::cout, "Marking subscription for cleanup",
                     std::addressof(subscriptionState));
    subscriptionState.cleanup_ = true;
}

void SubscriptionManager::notify_subscription_error(SubscriptionState& subscriptionState)
{
    utils::LOG_EVENT(std::cout, "Subscription Error for cleanup",
                     std::addressof(subscriptionState));
    mark_subscription_cleanup(subscriptionState);
    auto connectionStateWeakPtr = subscriptionState.get_connection_state_weak_ptr();

    if (auto connectionStateSharedPtr = connectionStateWeakPtr.lock())
    {
        // TODO: send subscription error
    }
}

} // namespace rvn
