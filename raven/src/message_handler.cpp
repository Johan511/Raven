#include <serialization/serialization.hpp>
#include <message_handler.hpp>
#include <moqt.hpp>

namespace rvn
{

void MessageHandler::operator()(depracated::messages::ClientSetupMessage clientSetupMessage)
{
    utils::LOG_EVENT(std::cout, "Client Setup Message received: \n", clientSetupMessage);
    // Send Server Setup Message
    depracated::messages::ServerSetupMessage serverSetupMessage;
    serverSetupMessage.selectedVersion_ = 0;

    streamState_.streamContext->connectionState_.send_control_buffer(
    serialization::serialize(serverSetupMessage));
}

void MessageHandler::operator()(depracated::messages::ServerSetupMessage serverSetupMessage)
{
    utils::LOG_EVENT(std::cout, "Server Setup Message received: \n", serverSetupMessage);
    MOQTClient& moqtClient =
    static_cast<MOQTClient&>(streamState_.streamContext->moqtObject_);
    moqtClient.ravenConnectionSetupFlag_.store(true, std::memory_order_release);
}

void MessageHandler::operator()(depracated::messages::SubscribeMessage subscribeMessage)
{
    utils::LOG_EVENT(std::cout, "Subscribe Message received: \n", subscribeMessage);
    TrackIdentifier trackIdentifier(subscribeMessage.trackNamespace_,
                                    subscribeMessage.trackName_);
    streamState_.connectionState_.add_track_alias(std::move(trackIdentifier),
                                                  subscribeMessage.trackAlias_);

    subscriptionManager_.add_subscription(streamState_.connectionState_.weak_from_this(),
                                          std::move(subscribeMessage));
}

void MessageHandler::operator()(const auto& unknownMessage)
{
    utils::LOG_EVENT(std::cout, "Unknown Message received: \n", unknownMessage);
    utils::ASSERT_LOG_THROW(false, "Unknown message type");
}
} // namespace rvn
