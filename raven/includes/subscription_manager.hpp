#pragma once

#include <data_manager.hpp>
#include <definitions.hpp>
#include <optional>
#include <serialization/messages.hpp>
#include <serialization/serialization.hpp>
#include <strong_types.hpp>
#include <utilities.hpp>

namespace rvn
{

class ConnectionState;

struct SubscriptionStateErr
{
    // clang-format off
    struct ConnectionExpired{};
    struct ObjectDoesNotExist{};
    // clang-format on
};

// bool is true => subscription fullfilled
// bool is false => continue to next object
using FullfillSomeReturn =
std::variant<bool, SubscriptionStateErr::ConnectionExpired, SubscriptionStateErr::ObjectDoesNotExist>;

// Each stream corresponds to one minor subscription state
class MinorSubscriptionState
{
    friend class SubscriptionState;
    class SubscriptionState* subscriptionState_;
    ObjectIdentifier objectToSend_;
    std::optional<ObjectIdentifier> lastObjectToBeSent_;

public:
    MinorSubscriptionState(SubscriptionState& subscriptionState,
                           ObjectIdentifier objectToSend,
                           std::optional<ObjectIdentifier> lastObjectToBeSent);

    // returs true if minor subscription state has been fulfilled
    FullfillSomeReturn fulfill_some_minor();

    ~MinorSubscriptionState()
    {
        // TODO: notify the connection state if it exists that cleanup should be done
    }
};

// Each subscription state corresponds to one subscription message
class SubscriptionState
{
    friend MinorSubscriptionState;
    // NOTE: should be protected by checking if it actually exists
    std::weak_ptr<ConnectionState> connectionStateWeakPtr_;
    // can not be reference because we need Subscription State to be assignable (while removing it from vector)
    DataManager* dataManager_;
    class SubscriptionManager* subscriptionManager_;
    depracated::messages::SubscribeMessage subscriptionMessage_;

    std::vector<MinorSubscriptionState> minorSubscriptionStates_;

    void error_handler(SubscriptionStateErr::ConnectionExpired);
    void error_handler(SubscriptionStateErr::ObjectDoesNotExist);

public:
    bool cleanup_;
    SubscriptionState(std::weak_ptr<ConnectionState>&& connectionState,
                      DataManager& dataManager,
                      SubscriptionManager& subscriptionManager,
                      depracated::messages::SubscribeMessage subscriptionMessage);

    FullfillSomeReturn fulfill_some();

    std::weak_ptr<ConnectionState>& get_connection_state_weak_ptr() noexcept
    {
        return connectionStateWeakPtr_;
    }

    const std::weak_ptr<ConnectionState>& get_connection_state_weak_ptr() const noexcept
    {
        return connectionStateWeakPtr_;
    }

    ~SubscriptionState()
    {
        utils::ASSERT_LOG_THROW(cleanup_, "Subscription not meant for cleanup "
                                          "is being destroyed");
    }
};

class SubscriptionManager
{
    class DataManager& dataManager_;

    // holds subscriptions messages which need to be processed and start executing
    MPMCQueue<std::tuple<std::weak_ptr<ConnectionState>, depracated::messages::SubscribeMessage>> subscriptionQueue_;
    struct ThreadLocalState
    {
        SubscriptionManager& subscriptionManager_;
        // subscription state which this thread is handling
        std::vector<SubscriptionState> subscriptionStates_;

        void operator()()
        {
            while (true)
            {
                auto& subscriptionQueue_ = subscriptionManager_.subscriptionQueue_;

                std::tuple<std::weak_ptr<ConnectionState>, depracated::messages::SubscribeMessage> subscriptionTuple;
                while (subscriptionQueue_.try_dequeue(subscriptionTuple))
                {
                    auto connectionStateWeakPtr =
                    std::move(std::get<0>(subscriptionTuple));
                    auto subscriptionMessage = std::move(std::get<1>(subscriptionTuple));

                    subscriptionStates_.emplace_back(std::move(connectionStateWeakPtr),
                                                     subscriptionManager_.dataManager_,
                                                     subscriptionManager_,
                                                     std::move(subscriptionMessage));
                    if (subscriptionStates_.back().cleanup_)
                        subscriptionStates_.pop_back();
                }

                auto beginIter = subscriptionStates_.begin();
                auto endIter = subscriptionStates_.end();

                for (auto traversalIter = beginIter; traversalIter != endIter; ++traversalIter)
                {
                    auto fullfillReturn = traversalIter->fulfill_some();

                    if (std::holds_alternative<bool>(fullfillReturn))
                    // subscription is being fulfilled with no issues
                    {
                        if (!std::get<bool>(fullfillReturn))
                            // subscription is yet to be fulfilled
                            // all other cases the subscription state is destroyed
                            // NOTE: destructor only called if a different subscription is moved into its place
                            // coz Wolfgang
                            *beginIter++ = std::move(*traversalIter);
                    }
                    else if (std::holds_alternative<SubscriptionStateErr::ConnectionExpired>(fullfillReturn))
                    {
                        // Nothing to be done
                    }
                    else if (std::holds_alternative<SubscriptionStateErr::ObjectDoesNotExist>(fullfillReturn))
                        subscriptionManager_.notify_subscription_error(*traversalIter);
                    else
                        assert(false);
                }
            }
        }
    };
    std::vector<struct ThreadLocalState> threadLocalStates_;
    // thread pool to manage subscriptions
    std::vector<std::jthread> threadPool_;

public:
    SubscriptionManager(DataManager& dataManager, std::size_t numThreads = 1);
    void add_subscription(std::weak_ptr<ConnectionState> connectionStateWeakPtr,
                          depracated::messages::SubscribeMessage subscribeMessage);


    // Error Handling functions
    void mark_subscription_cleanup(SubscriptionState& subscriptionState);
    void notify_subscription_error(SubscriptionState& subscriptionState);
};
} // namespace rvn
