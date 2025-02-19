#pragma once

#include <chrono>
#include <data_manager.hpp>
#include <definitions.hpp>
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
    // Very obvious ABA error over here
    /*
        lastSentBuffer_ has finished sending and the buffer has been deleted, a
       different data stream now is being sent which has the same address as lastSentBuffer
    */
    std::optional<ObjectIdentifier> previouslySentObject_;
    ObjectIdentifier objectToSend_;
    std::optional<ObjectIdentifier> lastObjectToBeSent_;

    // We need a only to find difference between 2 timepoints,
    // we don't care about system time or high resolution or anything else
    // Hence, we choose to go with steady clock
    using MonotonicClock = std::chrono::steady_clock;

    std::chrono::time_point<MonotonicClock> lastSentTime_;
    std::chrono::microseconds timeBetweenSends_;

    bool abortIfSending_;

public:
    MinorSubscriptionState(SubscriptionState& subscriptionState,
                           ObjectIdentifier objectToSend,
                           std::optional<ObjectIdentifier> lastObjectToBeSent,
                           bool abortIfSending);

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
    SubscribeMessage subscriptionMessage_;

    std::vector<MinorSubscriptionState> minorSubscriptionStates_;

    void error_handler(SubscriptionStateErr::ConnectionExpired);
    void error_handler(SubscriptionStateErr::ObjectDoesNotExist);

    FullfillSomeReturn
    add_group_subscription(const GroupHandle& groupHandle,
                           bool abortIfSending,
                           std::optional<ObjectId> beginObjectId = {},
                           std::optional<ObjectId> endObjectId = {});

public:
    bool cleanup_;
    SubscriptionState(std::weak_ptr<ConnectionState>&& connectionState,
                      DataManager& dataManager,
                      SubscriptionManager& subscriptionManager,
                      SubscribeMessage subscriptionMessage);

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
        auto connectionStateSharedPtr = connectionStateWeakPtr_.lock();
        if (!connectionStateSharedPtr)
            return;

        // TODO
        // connectionStateSharedPtr->send_subscribe_done();
    }
};

struct ThreadLocalState
{
    SubscriptionManager& subscriptionManager_;
    // subscription state which this thread is handling
    // has to be stable container because we have pointers back to subscription state
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
