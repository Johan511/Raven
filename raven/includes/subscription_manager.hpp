#pragma once

#include <chrono>
#include <data_manager.hpp>
#include <definitions.hpp>
#include <memory>
#include <optional>
#include <serialization/messages.hpp>
#include <serialization/serialization.hpp>
#include <strong_types.hpp>
#include <utilities.hpp>

namespace rvn
{

struct ConnectionState;

struct SubscriptionStateErr
{
    // clang-format off
    struct ConnectionExpired{};
    // clang-format on
};

// bool is true => subscription fulfilled
// bool is false => continue to next object
using FulfillSomeReturn = std::variant<bool, SubscriptionStateErr::ConnectionExpired>;

enum class NextOperation
{
    First,
    Next,
    Latest
};

// TODO: remove concept of minor subscription
class MinorSubscriptionState
{
    friend class SubscriptionState;
    class SubscriptionState* subscriptionState_;
    std::optional<ObjectIdentifier> previouslySentObject_;

    NextOperation nextOperation_;
    std::optional<ObjectIdentifier> lastObjectToBeSent_;

    PublisherPriority trackPublisherPriority_;
    bool mustBeSent_;
    std::optional<WaitSignal> waitSignal_;

    std::optional<std::chrono::milliseconds> subscribeDeliveryTimeout_;

public:
    MinorSubscriptionState(SubscriptionState& subscriptionState,
                           NextOperation nextOperation,
                           std::optional<ObjectIdentifier> lastObjectToBeSent,
                           PublisherPriority trackPublisherPriority,
                           std::optional<std::chrono::milliseconds> deliveryTimeout);

    // returs true if minor subscription state has been fulfilled
    FulfillSomeReturn fulfill_some_minor();

    // need this function to be inlined (for better performance) as it is called
    // in tight loop
    inline bool is_waiting_for_object();

    // TODO: cleanup in destructor, notify client that minor subscription has
    // ended WARNING: adding destructor will disable implicitly generated move
    // operations
};

// Each subscription state corresponds to one subscription message
class SubscriptionState
{
    friend MinorSubscriptionState;
    // NOTE: should be protected by checking if it actually exists
    std::weak_ptr<ConnectionState> connectionStateWeakPtr_;
    // can not be reference because we need Subscription State to be assignable
    // (while removing it from vector)
    DataManager* dataManager_;
    class SubscriptionManager* subscriptionManager_;
    SubscribeMessage subscriptionMessage_;

    std::shared_ptr<TrackHandle> trackHandle_;
    std::vector<MinorSubscriptionState> minorSubscriptionStates_;

    void error_handler(SubscriptionStateErr::ConnectionExpired);

public:
    bool cleanup_;
    SubscriptionState(std::weak_ptr<ConnectionState>&& connectionState,
                      DataManager& dataManager,
                      SubscriptionManager& subscriptionManager,
                      SubscribeMessage subscriptionMessage);

    FulfillSomeReturn fulfill_some();

    std::weak_ptr<ConnectionState>& get_connection_state_weak_ptr() noexcept
    {
        return connectionStateWeakPtr_;
    }

    const std::weak_ptr<ConnectionState>& get_connection_state_weak_ptr() const noexcept
    {
        return connectionStateWeakPtr_;
    }

    // TODO: removing this destructor causes groups not to advance or abort,
    // need to investigate
    ~SubscriptionState() = default;
};

struct ThreadLocalState
{
    SubscriptionManager& subscriptionManager_;
    // subscription state which this thread is handling
    // has to be stable container because we have pointers back to subscription
    // state
    StableContainer<SubscriptionState> subscriptionStates_;

    void operator()();
};

class SubscriptionManager
{
    friend struct ThreadLocalState;
    class DataManager& dataManager_;
    std::atomic<bool> cleanup_;

    // holds subscriptions messages which need to be processed and start executing
    MPMCQueue<std::tuple<std::weak_ptr<ConnectionState>, SubscribeMessage>> subscriptionQueue_;

    std::vector<ThreadLocalState> threadLocalStates_;
    // thread pool to manage subscriptions
    std::vector<std::jthread> threadPool_;

public:
    SubscriptionManager(DataManager& dataManager, std::size_t numThreads = 1);
    void add_subscription(std::weak_ptr<ConnectionState> connectionStateWeakPtr,
                          SubscribeMessage subscribeMessage);

    // Error Handling functions
    void mark_subscription_cleanup(SubscriptionState& subscriptionState);
    void notify_subscription_error(SubscriptionState& subscriptionState);

    ~SubscriptionManager();
};
} // namespace rvn
