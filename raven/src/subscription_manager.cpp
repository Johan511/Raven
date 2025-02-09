#include "utilities.hpp"
#include <contexts.hpp>
#include <memory>
#include <msquic.h>
#include <optional>
#include <serialization/messages.hpp>
#include <serialization/serialization.hpp>
#include <subscription_manager.hpp>

namespace rvn
{

MinorSubscriptionState::MinorSubscriptionState(SubscriptionState& subscriptionState,
                                               ObjectIdentifier objectToSend,
                                               std::optional<ObjectIdentifier> lastObjectToBeSent)
: subscriptionState_(std::addressof(subscriptionState)),
  objectToSend_(std::move(objectToSend)),
  lastObjectToBeSent_(std::move(lastObjectToBeSent))
{
}

// returns true if fulfilling is done
FullfillSomeReturn MinorSubscriptionState::fulfill_some_minor()
{
    auto connectionStateSharedPtr = subscriptionState_->connectionStateWeakPtr_.lock();
    if (!connectionStateSharedPtr)
        return SubscriptionStateErr::ConnectionExpired{};

    auto objectOrStatus = subscriptionState_->dataManager_->get_object(objectToSend_);

    if (std::holds_alternative<DoesNotExist>(objectOrStatus))
        return SubscriptionStateErr::ObjectDoesNotExist{};
    else if (std::holds_alternative<NotFound>(objectOrStatus))
        // will try later (TODO: might lead to infinite loop, handle case by limiting retries)
        return false;
    else
    {
        auto object = std::get<std::string>(objectOrStatus);
        StreamHeaderSubgroupObject objectMsg;
        objectMsg.objectId_ = objectToSend_.objectId_;
        objectMsg.payload_ = std::move(object);
        QUIC_BUFFER* quicBuffer = serialization::serialize(objectMsg);
        connectionStateSharedPtr->send_object(objectToSend_, quicBuffer);

        return (lastObjectToBeSent_.has_value() && objectToSend_ == *lastObjectToBeSent_);
    }
}

// returns true if fulfilling is done
FullfillSomeReturn SubscriptionState::fulfill_some()
{
    bool allFullfilled = true;
    auto beginIter = minorSubscriptionStates_.begin();
    auto endIter = minorSubscriptionStates_.end();

    for (auto traversalIter = beginIter; traversalIter != endIter; ++traversalIter)
    {
        auto fullfillReturn = traversalIter->fulfill_some_minor();
        // TODO, handle return cases
    }

    return allFullfilled;
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


    switch (filterType)
    {
        case SubscribeMessage::FilterType::LatestGroup:
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
            std::optional<ObjectId> firstObjectIdOpt =
            dataManager_->get_first_object({ trackIdentifier, *currGroupOpt });

            if (!firstObjectIdOpt.has_value())
            {
                subscriptionManager_->notify_subscription_error(*this);
                return;
            }
            ObjectIdentifier firstObjectIdentifier(trackIdentifier, std::move(*currGroupOpt),
                                                   *firstObjectIdOpt);

            minorSubscriptionStates_.emplace_back(*this, firstObjectIdentifier, std::nullopt);
            break;
        }
        case SubscribeMessage::FilterType::LatestObject:
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
            auto firstObjectIdOpt =
            dataManager_->get_first_object({ trackIdentifier, *currGroupOpt });

            if (!firstObjectIdOpt.has_value())
            {
                subscriptionManager_->notify_subscription_error(*this);
                return;
            }
            ObjectIdentifier firstObjectIdentifier(trackIdentifier, std::move(*currGroupOpt),
                                                   *firstObjectIdOpt);

            minorSubscriptionStates_.emplace_back(*this, firstObjectIdentifier, std::nullopt);
            break;
        }
        case SubscribeMessage::FilterType::AbsoluteStart:
        {
            ObjectIdentifier objectToBeSent{ trackIdentifier,
                                             subscriptionMessage_.start_->group_,
                                             subscriptionMessage_.start_->object_ };
            // there is no last object to be sent because we are sending all objects

            minorSubscriptionStates_.emplace_back(*this, std::move(objectToBeSent),
                                                  std::nullopt);
            break;
        }
        case SubscribeMessage::FilterType::AbsoluteRange:
        {
            ObjectIdentifier objectToBeSent{ trackIdentifier,
                                             subscriptionMessage_.start_->group_,
                                             subscriptionMessage_.start_->object_ };

            ObjectIdentifier lastObjectToBeSent{ trackIdentifier,
                                                 subscriptionMessage_.end_->group_,
                                                 subscriptionMessage_.end_->object_ };
            break;
        }
        default:
        {
            utils::ASSERT_LOG_THROW(false, "Unknown filter type",
                                    utils::to_underlying(filterType));
        }
    }
}

SubscriptionManager::SubscriptionManager(DataManager& dataManager, std::size_t numThreads)
: dataManager_(dataManager)
{
    threadLocalStates_.reserve(numThreads);
    for (std::size_t i = 0; i < numThreads; i++)
    {
        threadLocalStates_.emplace_back(*this);
        threadPool_.emplace_back(threadLocalStates_.back());
    }
}

void SubscriptionManager::add_subscription(std::weak_ptr<ConnectionState> connectionStateWeakPtr,
                                           SubscribeMessage subscribeMessage)
{
    subscriptionQueue_.enqueue(std::make_tuple(std::move(connectionStateWeakPtr),
                                               std::move(subscribeMessage)));
}

void SubscriptionManager::mark_subscription_cleanup(SubscriptionState& subscriptionState)
{
    subscriptionState.cleanup_ = true;
}

void SubscriptionManager::notify_subscription_error(SubscriptionState& subscriptionState)
{
    mark_subscription_cleanup(subscriptionState);
    auto connectionStateWeakPtr = subscriptionState.get_connection_state_weak_ptr();

    if (auto connectionStateSharedPtr = connectionStateWeakPtr.lock())
    {
        // TODO: send subscription error
    }
}

} // namespace rvn
