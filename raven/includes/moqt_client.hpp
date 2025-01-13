#pragma once
////////////////////////////////////////////
#include <msquic.h>
////////////////////////////////////////////
#include <atomic>
#include <cstdint>
////////////////////////////////////////////
#include <contexts.hpp>
#include <message_handlers.hpp>
#include <protobuf_messages.hpp>
#include <serialization/serialization.hpp>
#include <subscription_manager.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>
////////////////////////////////////////////


namespace rvn
{
class MOQTClient : public MOQT
{
public:
    // ConnectionState is not default constructable
    // so we need to have a optional wrapper
    std::optional<ConnectionState> connectionState;
    StableContainer<MPMCQueue<std::string>> objectQueues;


    void handle_message_internal(ConnectionState& connectionState,
                                 HQUIC streamHandle,
                                 const auto* receiveInformation)
    {
        utils::ASSERT_LOG_THROW(connectionState.get_control_stream().has_value(),
                                "Trying to interpret control message without "
                                "control stream");

        const QUIC_BUFFER* buffers = receiveInformation->Buffers;

        std::stringstream iStringStream(
        std::string(reinterpret_cast<const char*>(buffers[0].Buffer), buffers[0].Length));

        return handle_message(connectionState, streamHandle, iStringStream);
    }

    void handle_message_internal(ConnectionState& connectionState,
                                 HQUIC streamHandle,
                                 std::stringstream& iStringStream)
    {
        google::protobuf::io::IstreamInputStream istream(&iStringStream);

        protobuf_messages::MessageHeader header =
        serialization::deserialize<protobuf_messages::MessageHeader>(istream);

        /* TODO, split into client and server interpret functions helps reduce
         * the number of branches NOTE: This is the message received, which
         * means that the client will interpret the server's message and vice
         * verse CLIENT_SETUP is received by server and SERVER_SETUP is received
         * by client
         */

        StreamState& streamState =
        *get_stream_state(connectionState.connection_.get(), streamHandle);

        MessageHandler messageHandler(*this, connectionState);

        switch (header.messagetype())
        {
            case protobuf_messages::MoQtMessageType::SERVER_SETUP:
            {
                // SERVER sends to CLIENT
                messageHandler.template handle_message<protobuf_messages::ServerSetupMessage>(connectionState, istream);
                ravenConnectionSetupFlag_.store(true, std::memory_order_release);
                break;
            }
            case protobuf_messages::MoQtMessageType::OBJECT_STREAM:
            {

                messageHandler.template handle_message<protobuf_messages::ObjectStreamMessage>(connectionState, istream);

                break;
            }
            default: LOGE("Unknown control message type", header.messagetype());
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // these functions will later be pushed into cgUtils
    // utils::MOQTComplexGetterUtils *cgUtils{this};

    StreamState* get_stream_state(HQUIC connectionHandle, HQUIC streamHandle)
    {
        rvn::utils::ASSERT_LOG_THROW(connectionState->connection_.get() == connectionHandle,
                                     "Connection handle does not match");

        if (connectionState->get_control_stream().has_value() &&
            connectionState->get_control_stream().value().stream.get() == streamHandle)
        {
            return &connectionState->get_control_stream().value();
        }

        auto& dataStreams = connectionState->get_data_streams();

        auto streamIter =
        std::find_if(dataStreams.begin(), dataStreams.end(),
                     [streamHandle](const StreamState& streamState)
                     { return streamState.stream.get() == streamHandle; });

        if (streamIter != dataStreams.end())
        {
            return &(*streamIter);
        }

        return nullptr;
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    auto subscribe(protobuf_messages::SubscribeMessage&& subscribeMessage)
    {
        objectQueues.emplace_back();
        connectionState->objectQueue = --(objectQueues.end());

        protobuf_messages::MessageHeader subscribeHeader;
        subscribeHeader.set_messagetype(protobuf_messages::MoQtMessageType::SUBSCRIBE);


        QUIC_BUFFER* quicBuffer =
        serialization::serialize(subscribeHeader, subscribeMessage);

        connectionState->enqueue_control_buffer(quicBuffer);

        return connectionState->objectQueue;
    }


    MOQTClient();

    void start_connection(QUIC_ADDRESS_FAMILY Family, const char* ServerName, uint16_t ServerPort);

    protobuf_messages::ClientSetupMessage get_clientSetupMessage();

    QUIC_STATUS accept_data_stream(HQUIC connection, HQUIC streamHandle)
    {
        return connectionState->accept_data_stream(streamHandle);
    }

    // atomic flags for multi thread synchronization
    // make sure no connections are accepted until whole setup required is completed
    std::atomic_bool quicConnectionStateSetupFlag_{};

    // make sure no communication untill setup messages are exchanged
    std::atomic_bool ravenConnectionSetupFlag_{};
};
} // namespace rvn
