#pragma once
#include <serialization/messages.hpp>

namespace rvn
{
class MessageHandler
{
    class StreamState& streamState_;

public:
    MessageHandler(StreamState& streamState) : streamState_(streamState)
    {
    };

    void operator()(depracated::messages::ClientSetupMessage clientSetupMessage);
    void operator()(depracated::messages::ServerSetupMessage serverSetupMessage);
    void operator()(depracated::messages::SubscribeMessage subscribeMessage);
    void operator()(const auto& unknownMessage);
};
} // namespace rvn
