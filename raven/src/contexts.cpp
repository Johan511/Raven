#include "definitions.hpp"
#include "utilities.hpp"
#include <contexts.hpp>
#include <moqt.hpp>
#include <stdexcept>

namespace rvn
{
StreamState& ConnectionState::create_data_stream()
{
    HQUIC connectionHandle = connection_.get();
    StreamContext* streamContext = new StreamContext(moqtObject_, *this);

    auto stream =
    rvn::unique_stream(moqtObject_.get_tbl(),
                       { connectionHandle, QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
                         moqtObject_.data_stream_cb_wrapper, streamContext },
                       { QUIC_STREAM_START_FLAG_FAIL_BLOCKED |
                         QUIC_STREAM_START_FLAG_SHUTDOWN_ON_FAIL });

    dataStreams.emplace_back(std::move(stream), *this);

    StreamState& streamState = dataStreams.back();
    streamState.set_stream_context(std::unique_ptr<StreamContext>(streamContext));

    return streamState;
}

void ConnectionState::send_data_buffer()
{
    if (dataBuffersToSend.empty())
    {
        auto iter = SubscriptionManagerHandle
        {
        } -> subscriptionStates[this].begin();
        if (iter == SubscriptionManagerHandle {} -> subscriptionStates[this].end())
            return;

        return SubscriptionManagerHandle
        {
        } -> update_subscription_state(this, iter);
    }

    QUIC_BUFFER* buffer = dataBuffersToSend.front();
    dataBuffersToSend.pop();

    StreamState& streamState = create_data_stream();
    HQUIC streamHandle = streamState.stream.get();

    StreamSendContext* streamSendContext =
    new StreamSendContext(buffer, 1, streamState.streamContext.get(),
                          [this, streamHandle](StreamSendContext* context)
                          {
                              HQUIC connectionHandle =
                              context->streamContext->connectionState_
                              .connection_.get();
                              this->delete_data_stream(streamHandle);
                          });

    QUIC_STATUS status =
    moqtObject_.get_tbl()->StreamSend(streamHandle, buffer, 1,
                                      QUIC_SEND_FLAG_FIN, streamSendContext);

    if (QUIC_FAILED(status))
        throw std::runtime_error("Failed to send data buffer");
}

void ConnectionState::delete_data_stream(HQUIC streamHandle)
{
    auto streamIter =
    std::find_if(dataStreams.begin(), dataStreams.end(),
                 [streamHandle](const StreamState& streamState)
                 { return streamState.stream.get() == streamHandle; });

    utils::ASSERT_LOG_THROW(streamIter != dataStreams.end(),
                            "Attempting to delete stream which does not "
                            "exist in busy stream queue");

    dataStreams.erase(streamIter);

    if (dataStreams.size() < MAX_DATA_STREAMS)
        send_data_buffer();
}

void ConnectionState::enqueue_data_buffer(QUIC_BUFFER* buffer)
{
    utils::LOG_EVENT(std::cout, "Enqueueing data buffer of size: ", buffer->Length);
    dataBuffersToSend.push(buffer);
    if (dataStreams.size() < MAX_DATA_STREAMS)
        send_data_buffer();
}


void ConnectionState::send_control_buffer(QUIC_BUFFER* buffer, QUIC_SEND_FLAGS flags)
{
    // control messages have higher priority
    flags |= QUIC_SEND_FLAG_PRIORITY_WORK;

    StreamState* streamState = &controlStream.value();
    HQUIC streamHandle = streamState->stream.get();

    StreamSendContext* streamSendContext =
    new StreamSendContext(buffer, 1, streamState->streamContext.get());

    QUIC_STATUS status =
    moqtObject_.get_tbl()->StreamSend(streamHandle, buffer, 1, flags, streamSendContext);
    if (QUIC_FAILED(status))
        throw std::runtime_error("Failed to send control message");
}

const StableContainer<StreamState>& ConnectionState::get_data_streams() const
{
    return dataStreams;
}

StableContainer<StreamState>& ConnectionState::get_data_streams()
{
    return dataStreams;
}

const std::optional<StreamState>& ConnectionState::get_control_stream() const
{
    return controlStream;
}

std::optional<StreamState>& ConnectionState::get_control_stream()
{
    return controlStream;
}


QUIC_STATUS ConnectionState::accept_data_stream(HQUIC streamHandle)
{
    auto& dataStreams = get_data_streams();

    // register new data stream into connectionState object
    dataStreams.emplace_back(rvn::unique_stream(moqtObject_.get_tbl(), streamHandle), *this);

    // set stream context for stream
    StreamState& streamState = dataStreams.back();
    streamState.set_stream_context(std::make_unique<StreamContext>(moqtObject_, *this));

    // set callback handler fot the stream (MOQT sets internally)
    moqtObject_.get_tbl()->SetCallbackHandler(streamHandle, (void*)MOQT::data_stream_cb_wrapper,
                                              (void*)streamState.streamContext.get());

    // registering stream can not fail
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS ConnectionState::accept_control_stream(HQUIC controlStreamHandle)
{
    this->controlStream.emplace(rvn::unique_stream(moqtObject_.get_tbl(), controlStreamHandle),
                                *this);
    this->controlStream->set_stream_context(
    std::make_unique<StreamContext>(moqtObject_, *this));


    moqtObject_.get_tbl()->SetCallbackHandler(controlStreamHandle,
                                              (void*)MOQT::control_stream_cb_wrapper,
                                              (
                                              void*)controlStream->streamContext.get());

    this->controlStream->streamContext->deserializer_.emplace(
    MessageHandler(*this->controlStream));

    this->controlStream->streamContext->streamHasBeenConstructed.store(true, std::memory_order_release);


    return QUIC_STATUS_SUCCESS;
}

StreamState& ConnectionState::establish_control_stream()
{
    static int numTimesFunctionExecuted = 0;
    if (++numTimesFunctionExecuted != 1)
        utils::ASSERT_LOG_THROW(false, "establish control stream should be "
                                       "called only once in a connection");
    StreamContext* streamContext = new StreamContext(moqtObject_, *this);
    this->controlStream.emplace(rvn::unique_stream(moqtObject_.get_tbl(),
                                                   { connection_.get(), QUIC_STREAM_OPEN_FLAG_0_RTT,
                                                     MOQT::control_stream_cb_wrapper, streamContext },
                                                   { QUIC_STREAM_START_FLAG_PRIORITY_WORK }),
                                *this);

    this->controlStream->set_stream_context(std::unique_ptr<StreamContext>(streamContext));
    this->controlStream->streamContext->deserializer_.emplace(
    MessageHandler(*this->controlStream));

    this->controlStream->streamContext->streamHasBeenConstructed.store(true, std::memory_order_release);
    return this->controlStream.value();
}

} // namespace rvn
