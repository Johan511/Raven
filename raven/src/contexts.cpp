#include <contexts.hpp>
#include <moqt.hpp>
#include <stdexcept>

namespace rvn
{
StreamState& StreamManager::create_data_stream()
{
    HQUIC connectionHandle = connectionState->connection;
    StreamContext* streamContext =
    new StreamContext(connectionState->moqtObject, connectionHandle);

    auto stream =
    rvn::unique_stream(connectionState->moqtObject->get_tbl(),
                       { connectionHandle, QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
                         connectionState->moqtObject->data_stream_cb_wrapper, streamContext },
                       { QUIC_STREAM_START_FLAG_FAIL_BLOCKED |
                         QUIC_STREAM_START_FLAG_SHUTDOWN_ON_FAIL });

    dataStreams.emplace_back(std::move(stream));

    StreamState& streamState = dataStreams.back();
    streamState.set_stream_context(std::unique_ptr<StreamContext>(streamContext));

    return streamState;
}

void StreamManager::send_data_buffer()
{
    utils::ASSERT_LOG_THROW(dataBuffersToSend.size(), __FUNCTION__,
                            "called with no buffers to send");

    QUIC_BUFFER* buffer = dataBuffersToSend.front();
    dataBuffersToSend.pop();

    StreamState& streamState = create_data_stream();
    HQUIC streamHandle = streamState.stream.get();

    StreamSendContext* streamSendContext =
    new StreamSendContext(buffer, 1, streamState.streamContext.get());

    QUIC_STATUS status =
    connectionState->moqtObject->get_tbl()->StreamSend(streamHandle, buffer, 1, QUIC_SEND_FLAG_FIN,
                                                      streamSendContext);

    if (QUIC_FAILED(status))
        throw std::runtime_error("Failed to send data buffer");
}

void StreamManager::delete_data_stream(HQUIC streamHandle)
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

void StreamManager::enqueue_data_buffer(QUIC_BUFFER* buffer)
{
    dataBuffersToSend.push(buffer);
    if (dataStreams.size() < MAX_DATA_STREAMS)
        send_data_buffer();
}

void StreamManager::enqueue_control_buffer(QUIC_BUFFER* buffer)
{
    controlBuffersToSend.push(buffer);

    if (controlStreamMessageReceived)
        send_control_buffer();
}

void StreamManager::send_control_buffer()
{
    utils::ASSERT_LOG_THROW(dataBuffersToSend.empty(), __FUNCTION__,
                            "called with no buffers to send");
    utils::ASSERT_LOG_THROW(controlStreamMessageReceived,
                            "Attempting to send another control message while "
                            "there has been no reply to the previous one");

    auto buffer = controlBuffersToSend.front();
    controlBuffersToSend.pop();

    std::cout << connectionState->connection << '\n';
    StreamState& streamState = connectionState->reset_control_stream();
    // HQUIC streamHandle = streamState.stream.get();

    // StreamSendContext* streamSendContext =
    // new StreamSendContext(buffer, 1, streamState.streamContext.get());

    // QUIC_STATUS status =
    // connectionState->moqtObject->get_tbl()->StreamSend(streamHandle, buffer, 1, QUIC_SEND_FLAG_FIN,
    //                                                   streamSendContext);
    // if (QUIC_FAILED(status))
    //     throw std::runtime_error("Failed to send control message");
    // else
    //     controlStreamMessageReceived = false;
}


bool ConnectionState::check_subscription(const protobuf_messages::SubscribeMessage& subscribeMessage)
{
    // TODO: check if subscription data exists and if we are authenticated
    // to get it
    return true;
}

const std::vector<StreamState>& ConnectionState::get_data_streams() const
{
    return streamManager->dataStreams;
}

std::vector<StreamState>& ConnectionState::get_data_streams()
{
    return streamManager->dataStreams;
}

const std::optional<StreamState>& ConnectionState::get_control_stream() const
{
    return streamManager->controlStream;
}

std::optional<StreamState>& ConnectionState::get_control_stream()
{
    return streamManager->controlStream;
}


QUIC_STATUS ConnectionState::accept_data_stream(HQUIC streamHandle)
{
    auto& dataStreams = get_data_streams();

    // register new data stream into connectionState object
    dataStreams.emplace_back(rvn::unique_stream(moqtObject->get_tbl(), streamHandle));

    // set stream context for stream
    StreamState& streamState = dataStreams.back();
    streamState.set_stream_context(std::make_unique<StreamContext>(moqtObject, connection));

    // set callback handler fot the stream (MOQT sets internally)
    moqtObject->get_tbl()->SetCallbackHandler(streamHandle, (void*)MOQT::control_stream_cb_wrapper,
                                              (void*)streamState.streamContext.get());

    // registering stream can not fail
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS ConnectionState::accept_control_stream(HQUIC controlStreamHandle)
{
    StreamState controlStreamState{ rvn::unique_stream(moqtObject->get_tbl(), controlStreamHandle) };

    controlStreamState.set_stream_context(
    std::make_unique<StreamContext>(moqtObject, connection));

    moqtObject->get_tbl()
    ->SetCallbackHandler(controlStreamHandle, (void*)MOQT::control_stream_cb_wrapper,
                         (void*)controlStreamState.streamContext.get());

    this->streamManager->controlStream = std::move(controlStreamState);

    return QUIC_STATUS_SUCCESS;
}

StreamState& ConnectionState::reset_control_stream()
{
    StreamState controlStreamState(
    rvn::unique_stream(moqtObject->get_tbl(),
                       {
                       connection,
                       QUIC_STREAM_OPEN_FLAG_0_RTT,
                       MOQT::control_stream_cb_wrapper,
                       },
                       { QUIC_STREAM_START_FLAG_PRIORITY_WORK }));

    controlStreamState.set_stream_context(
    std::make_unique<StreamContext>(moqtObject, connection));

    this->streamManager->controlStream = std::move(controlStreamState);

    return this->streamManager->controlStream.value();
}

void ConnectionState::register_subscription(const protobuf_messages::SubscribeMessage& subscribeMessage,
                                            std::istream* payloadStream)
{
    protobuf_messages::MessageHeader header;
    header.set_messagetype(protobuf_messages::MoQtMessageType::OBJECT_STREAM);

    protobuf_messages::ObjectStreamMessage objectStreamMessage;
    // <DummyValues>
    objectStreamMessage.set_subscribeid(1);
    objectStreamMessage.set_trackalias(1);
    objectStreamMessage.set_groupid(1);
    objectStreamMessage.set_objectid(1);
    objectStreamMessage.set_publisherpriority(1);
    objectStreamMessage.set_objectstatus(1);
    // </DummyValues>
    std::stringstream objectStreamMessageStream;
    objectStreamMessageStream << payloadStream->rdbuf();
    std::string payload = objectStreamMessageStream.str();

    std::size_t idx = 0;
    std::size_t payloadSize = payload.size();

    while (idx != payloadSize)
    {
        std::size_t remainingSize = payloadSize - idx;
        std::size_t sendSize = std::min(remainingSize, bufferCapacity);

        std::string sendPayloadChunk{ payload.c_str() + idx, sendSize };
        idx += sendSize;

        objectStreamMessage.set_objectpayload(sendPayloadChunk);

        QUIC_BUFFER* quicBuffer = serialization::serialize(header, objectStreamMessage);

        streamManager->enqueue_data_buffer(quicBuffer);
    }
}


} // namespace rvn
