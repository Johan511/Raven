#pragma once
////////////////////////////////////////////
#include <moqt_base.hpp>
#include <serialization/messages.hpp>
////////////////////////////////////////////
#include <atomic>
#include <cstdint>
////////////////////////////////////////////
#include <contexts.hpp>
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
    std::shared_ptr<ConnectionState> connectionState;
    StableContainer<MPMCQueue<std::string>> objectQueues;

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

    auto subscribe(SubscribeMessage&& subscribeMessage)
    {
        QUIC_BUFFER* quicBuffer = serialization::serialize(subscribeMessage);
        connectionState->send_control_buffer(quicBuffer);
    }


    MOQTClient();

    void start_connection(QUIC_ADDRESS_FAMILY Family, const char* ServerName, uint16_t ServerPort);

    ClientSetupMessage get_clientSetupMessage();

    QUIC_STATUS accept_data_stream(HQUIC streamHandle)
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
