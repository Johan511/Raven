/////////////////////////////////////////////
#include "strong_types.hpp"
#include <atomic>
#include <bit>
#include <chrono>
#include <iostream>
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
                                               NextOperation nextOperation,
                                               std::optional<ObjectIdentifier> lastObjectToBeSent,
                                               PublisherPriority trackPublisherPriority,
                                               std::optional<std::chrono::milliseconds> deliveryTimeout)
: subscriptionState_(std::addressof(subscriptionState)),
  nextOperation_(nextOperation), lastObjectToBeSent_(std::move(lastObjectToBeSent)),
  trackPublisherPriority_(trackPublisherPriority),
  mustBeSent_(std::countl_zero(trackPublisherPriority_.get()) == 0), // MSB is 1
  subscribeDeliveryTimeout_(deliveryTimeout)
{
}

bool MinorSubscriptionState::is_waiting_for_object()
{
    if (!waitSignal_.has_value()) [[unlikely]]
        // not waiting on object to be ready
        return true;

    // we wait on the flag, only is flag is ready, we do acquire operation
    // might have performance benefits on weaker memory models (ARM, POWERPC...)
    return (*waitSignal_)->load(std::memory_order_relaxed) == WaitStatus::Ready;

    // We only return that it is true, we have to reset the flag in the
    // fulfill_some_minor function and also have an acquire load on the flag
}

// returns true if fulfilling is done
// called only when wait signal is set, wait signal never gets unset once set
FulfillSomeReturn MinorSubscriptionState::fulfill_some_minor()
{
    if (waitSignal_.has_value()) [[likely]]
    {
        // we have to reset the flag
        (*waitSignal_)->load(std::memory_order_acquire);
        waitSignal_.reset();
    }

    auto connectionStateSharedPtr = subscriptionState_->connectionStateWeakPtr_.lock();
    if (!connectionStateSharedPtr)
        return SubscriptionStateErr::ConnectionExpired{};

    // monostate required as we do not want to default construct shit
    EnrichedObjectOrWait objectInfoOrWait;
    switch (nextOperation_)
    {
        case NextOperation::First:
        {
            objectInfoOrWait = subscriptionState_->trackHandle_->get_first_object();
            if (!std::holds_alternative<WaitSignal>(objectInfoOrWait))
                nextOperation_ = NextOperation::Next;
            break;
        }
        case NextOperation::Next:
        {
            objectInfoOrWait =
            subscriptionState_->trackHandle_->get_next_object(*previouslySentObject_);
            break;
        }
        case NextOperation::Latest:
        {
            objectInfoOrWait =
            subscriptionState_->trackHandle_->get_latest_object(previouslySentObject_);
            break;
        }
    }


    if (std::holds_alternative<WaitSignal>(objectInfoOrWait))
    {
        waitSignal_ = std::move(std::get<WaitSignal>(objectInfoOrWait));
        return false;
    }
    else
    {
        auto [groupId, objectId, object] = std::get<EnrichedObjectType>(objectInfoOrWait);

        if (object.is_track_terminator())
            return true;

        if (lastObjectToBeSent_.has_value())
        {
            if (std::make_tuple(groupId, objectId) >
                std::make_tuple(lastObjectToBeSent_->groupId_,
                                lastObjectToBeSent_->objectId_))
            {
                // we fulfilled the subscription requirement
                return true;
            }
        }

        if (previouslySentObject_.has_value())
        {
            // we do not want to copy because copying track identifier is rather
            // expensive operation (seq cst atomic add of shared_ptr)
            previouslySentObject_->groupId_ = groupId;
            previouslySentObject_->objectId_ = objectId;
        }
        else
            previouslySentObject_ =
            ObjectIdentifier{ subscriptionState_->trackHandle_->trackIdentifier_,
                              groupId, objectId };

        std::optional<std::chrono::milliseconds> timeoutDuration = subscribeDeliveryTimeout_;

        if (object.deliveryTimeout_)
        {
            if (timeoutDuration)
                timeoutDuration = std::min(*timeoutDuration, *object.deliveryTimeout_);
            else
                timeoutDuration = object.deliveryTimeout_;
        }

        QUIC_STATUS status =
        connectionStateSharedPtr->send_object(*previouslySentObject_, object.payload_,
                                              trackPublisherPriority_, timeoutDuration);
        if (QUIC_FAILED(status))
            return SubscriptionStateErr::ConnectionExpired{};
    }

    return false;
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
                // not yet fulfilled
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

    auto trackHandleOrStatus = dataManager_->get_track_handle(trackIdentifier);
    if (std::holds_alternative<WaitSignal>(trackHandleOrStatus))
    {
        // TODO: Handle wait condition
        utils::ASSERT_LOG_THROW(false, "Track is not ready", trackIdentifier);
    }

    trackHandle_ = std::get<std::shared_ptr<TrackHandle>>(std::move(trackHandleOrStatus));

    switch (filterType)
    {
        case SubscribeFilterType::LatestObject:
        {
            // TODO: check status of current group stuff in latest draft
            std::optional<GroupId> currGroupOpt =
            connectionStateSharedPtr->get_current_group(trackIdentifier);

            if (!currGroupOpt)
                currGroupOpt = GroupId(0);

            PublisherPriority trackPublisherPriority =
            dataManager_->get_track_publisher_priority(trackIdentifier);

            minorSubscriptionStates_.emplace_back(*this, NextOperation::Latest,
                                                  std::nullopt, trackPublisherPriority,
                                                  deliveryTimeoutOpt);

            break;
        }
        case SubscribeFilterType::AbsoluteStart:
        {
            // TODO
        }
        case SubscribeFilterType::AbsoluteRange:
        {
            // TODO
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
        // Why are we doing size_approx? Because constructing weak_ptr is a
        // rather expensive lock opertion We want to do it only if we believe
        // there are pending subscriptions
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
                    // This unfortunately does not seem to be using move
                    // constructor though (according to perf report)
                    // TODO: debug the issue
                    *beginIter++ = std::move(*traversalIter);
            }
            else if (std::holds_alternative<SubscriptionStateErr::ConnectionExpired>(fulfillReturn))
            {
                // Nothing to be done
            }
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
    // relaxed load and store works because there is no data dependencies with
    // cleanup_ it is just a single flag to be set, this might change if we are
    // doing more complex things before setting cleanup
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
