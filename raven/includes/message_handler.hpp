#pragma once
#include <serialization/messages.hpp>

namespace rvn
{
class NewMessageHandler
{
    class StreamState* streamState_;

public:
    NewMessageHandler() {}


    NewMessageHandler(StreamState* streamState) : streamState_(streamState)
    {
    }

    void operator()(depracated::messages::ClientSetupMessage clientSetupMessage)
    {
        utils::LOG_EVENT(std::cout, "Client Setup Message received: \n", clientSetupMessage);
    }

    void operator()(depracated::messages::ServerSetupMessage serverSetupMessage)
    {
        utils::LOG_EVENT(std::cout, "Server Setup Message received: \n", serverSetupMessage);
    }

    void operator()(depracated::messages::SubscribeMessage subscribeMessage)
    {
        utils::LOG_EVENT(std::cout, "Subscribe Message received: \n", subscribeMessage);
    }

    void operator()(const auto& unknownMessage)
    {
        utils::LOG_EVENT(std::cout, "Unknown Message received: \n", unknownMessage);

        utils::ASSERT_LOG_THROW(false, "Unknown message type");
    }
};
} // namespace rvn
