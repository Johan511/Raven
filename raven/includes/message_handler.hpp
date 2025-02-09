#pragma once
#include <serialization/messages.hpp>
#include <subscription_manager.hpp>

namespace rvn
{
class MessageHandler
{
    class StreamState& streamState_;
    class SubscriptionManager& subscriptionManager_;

public:
    MessageHandler(StreamState& streamState, SubscriptionManager& subscriptionManager)
    : streamState_(streamState), subscriptionManager_(subscriptionManager) {};

    void operator()(depracated::messages::ClientSetupMessage clientSetupMessage);
    void operator()(depracated::messages::ServerSetupMessage serverSetupMessage);
    void operator()(depracated::messages::SubscribeMessage subscribeMessage);
    void operator()(const auto& unknownMessage);
};
} // namespace rvn
