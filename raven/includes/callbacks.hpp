#pragma once

#include <contexts.hpp>
#include <moqt.hpp>
#include <msquic.h>
#include <serialization/serialization.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>

namespace rvn::callbacks
{

static constexpr auto server_listener_callback =
// listener is unused because we already have the listener stored
[]([[maybe_unused]] HQUIC listener, void* context, QUIC_LISTENER_EVENT* event) -> QUIC_STATUS
{
    MOQTServer* moqtServer = static_cast<MOQTServer*>(context);

    QUIC_STATUS status = QUIC_STATUS_NOT_SUPPORTED;
    switch (event->Type)
    {
        case QUIC_LISTENER_EVENT_NEW_CONNECTION:
            status = moqtServer->accept_new_connection(event->NEW_CONNECTION);
            break;
        case QUIC_LISTENER_EVENT_STOP_COMPLETE:
            // we have called stop on listener (opposite to start_listener
            // does)
            break;
        default: break;
    }
    return status;
};

static constexpr auto server_connection_callback =
[](HQUIC connection, void* context, QUIC_CONNECTION_EVENT* event) -> QUIC_STATUS
{
    MOQTServer* moqtServer = static_cast<MOQTServer*>(context);
    const QUIC_API_TABLE* MsQuic = moqtServer->get_tbl();

    switch (event->Type)
    {
        case QUIC_CONNECTION_EVENT_CONNECTED:

            MsQuic->ConnectionSendResumptionTicket(connection, QUIC_SEND_RESUMPTION_FLAG_NONE,
                                                   0, NULL);
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
            if (event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status == QUIC_STATUS_CONNECTION_IDLE)
            {
            }
            else {}
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER: break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        {
            /*  Do not cleanup her, might cause double free error
                In destructor of MOQT Server, connection is torn down, this might call `QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE` callback which would erase the connection
                Then destructor deallocates, which causes double free
            */


            // moqtServer->cleanup_connection(connection);
            break;
        }
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
        {
            // remotely opened streams call this callback => they must
            // be control streams
            moqtServer->accept_control_stream(connection, event->PEER_STREAM_STARTED);

            break;
        }
        case QUIC_CONNECTION_EVENT_RESUMED: break;
        default: break;
    }
    return QUIC_STATUS_SUCCESS;
};

// Control Stream Open Flags = QUIC_STREAM_OPEN_FLAG_NONE |
// QUIC_STREAM_OPEN_FLAG_0_RTT Control Stream Start flags =
// QUIC_STREAM_START_FLAG_PRIORITY_WORK
static constexpr auto server_control_stream_callback =
[]([[maybe_unused]] HQUIC controlStream, void* context, QUIC_STREAM_EVENT* event)
{
    StreamContext* streamContext = static_cast<StreamContext*>(context);
    MOQT& moqtObject = streamContext->moqtObject_;
    auto& deserializer = streamContext->deserializer_;

    utils::wait_for(streamContext->streamHasBeenConstructed);
    // moqtObject.get_tbl()->StreamReceiveSetEnabled(controlStream, true);

    switch (event->Type)
    {
        case QUIC_STREAM_EVENT_START_COMPLETE:
        {
            // called when attempting to setup stream connection
            LOGE("Server should never start "
                 "BiDirectionalControlStream");
            break;
        }
        case QUIC_STREAM_EVENT_RECEIVE:
        {
            for (std::uint64_t bufferIndex = 0;
                 bufferIndex < event->RECEIVE.BufferCount; bufferIndex++)
            {
                const QUIC_BUFFER* buffer = &event->RECEIVE.Buffers[bufferIndex];

                // make a copy of the buffer, we do not need a copy, but there seems to be a bug in MsQuic with
                // lifetime management of the buffers in MultiReceive Mode
                QUIC_BUFFER* newBuffer =
                (QUIC_BUFFER*)malloc(sizeof(QUIC_BUFFER) + buffer->Length);
                newBuffer->Length = buffer->Length;
                newBuffer->Buffer = (uint8_t*)newBuffer + sizeof(QUIC_BUFFER);
                std::memcpy(newBuffer->Buffer, buffer->Buffer, buffer->Length);

                deserializer->append_buffer(
                UniqueQuicBuffer(newBuffer,
                                 rvn::QUIC_BUFFERDeleter(controlStream,
                                                         moqtObject.get_tbl()->StreamReceiveComplete)));
            }

            // https://github.com/microsoft/msquic/blob/f96015560399d60cbdd8608b6fa2120560118500/docs/Streams.md#synchronous-vs-asynchronous
            // we are consuming the buffer in the callback because we are making a copy
            return QUIC_STATUS_SUCCESS;
            break;
        }
        case QUIC_STREAM_EVENT_SEND_COMPLETE:
        {
            // Buffer has been sent

            // client context is the context used in the QUIC send
            // function
            StreamSendContext* streamSendContext =
            static_cast<StreamSendContext*>(event->SEND_COMPLETE.ClientContext);

            streamSendContext->send_complete_cb();
            delete streamSendContext;
            break;
        }
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
        {
            std::cout << "Control Stream Shutdown has been called from "
                         "server"
                      << std::endl;
            break;
        }
        default: break;
    }
    return QUIC_STATUS_SUCCESS;
};

// Data Stream Open Flags = QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL
// Data Stream Start flags = QUIC_STREAM_START_FLAG_FAIL_BLOCKED |
// QUIC_STREAM_START_FLAG_SHUTDOWN_ON_FAIL
static constexpr auto server_data_stream_callback =
[]([[maybe_unused]] HQUIC dataStream, void* context, QUIC_STREAM_EVENT* event)
{
    StreamContext* streamContext = static_cast<StreamContext*>(context);

    // TODO: wait for streamSetup
    switch (event->Type)
    {
        case QUIC_STREAM_EVENT_START_COMPLETE:
        {
            // called when attempting to setup stream connection
            break;
        }
        case QUIC_STREAM_EVENT_RECEIVE:
        {
            utils::ASSERT_LOG_THROW(false, "Server should not receive message "
                                           "on data stream");
            break;
        }
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
        {
            delete streamContext;
            break;
        }

        default: break;
    }
    return QUIC_STATUS_SUCCESS;
};

// Data Stream Open Flags = QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL
// Data Stream Start flags = QUIC_STREAM_START_FLAG_FAIL_BLOCKED |
// QUIC_STREAM_START_FLAG_SHUTDOWN_ON_FAIL
static constexpr auto client_data_stream_callback =
[](HQUIC dataStream, void* context, QUIC_STREAM_EVENT* event)
{
    StreamContext* streamContext = static_cast<StreamContext*>(context);
    ConnectionState& connectionState = streamContext->connectionState_;
    MOQT& moqtObject = streamContext->moqtObject_;

    // moqtObject.get_tbl()->StreamReceiveSetEnabled(dataStream, true);

    // TODO: wait for stream setup
    switch (event->Type)
    {
        case QUIC_STREAM_EVENT_START_COMPLETE:
        {
            // called when attempting to setup stream connection
            break;
        }
        case QUIC_STREAM_EVENT_RECEIVE:
        {
            // accumulate all data messages received and read them when closing stream
            for (std::uint64_t bufferIndex = 0;
                 bufferIndex < event->RECEIVE.BufferCount; bufferIndex++)
            {
                const QUIC_BUFFER* buffer = &event->RECEIVE.Buffers[bufferIndex];

                // make a copy of the buffer, we do not need a copy, but there seems to be a bug in MsQuic with
                // lifetime management of the buffers in MultiReceive Mode
                QUIC_BUFFER* newBuffer =
                (QUIC_BUFFER*)malloc(sizeof(QUIC_BUFFER) + buffer->Length);
                newBuffer->Length = buffer->Length;
                newBuffer->Buffer = (uint8_t*)newBuffer + sizeof(QUIC_BUFFER);
                std::memcpy(newBuffer->Buffer, buffer->Buffer, buffer->Length);

                streamContext->deserializer_->append_buffer(
                UniqueQuicBuffer(newBuffer,
                                 rvn::QUIC_BUFFERDeleter(dataStream,
                                                         moqtObject.get_tbl()->StreamReceiveComplete)));
            }

            // https://github.com/microsoft/msquic/blob/f96015560399d60cbdd8608b6fa2120560118500/docs/Streams.md#synchronous-vs-asynchronous
            // we are consuming the buffer in the callback because we are making a copy
            return QUIC_STATUS_SUCCESS;
            break;
        }
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
        {
            connectionState.delete_data_stream(dataStream);
            break;
        }
        default: break;
    }
    return QUIC_STATUS_SUCCESS;
};

// Control Stream Open Flags = QUIC_STREAM_OPEN_FLAG_NONE |
// QUIC_STREAM_OPEN_FLAG_0_RTT Control Stream Start flags =
// QUIC_STREAM_START_FLAG_PRIORITY_WORK
static constexpr auto client_control_stream_callback =
[]([[maybe_unused]] HQUIC controlStream, void* context, QUIC_STREAM_EVENT* event)
{
    StreamContext* streamContext = static_cast<StreamContext*>(context);
    MOQT& moqtObject = streamContext->moqtObject_;

    utils::wait_for(streamContext->streamHasBeenConstructed);
    // moqtObject.get_tbl()->StreamReceiveSetEnabled(controlStream, true);

    switch (event->Type)
    {
        case QUIC_STREAM_EVENT_START_COMPLETE:
        {
            // called when attempting to setup stream connection
            break;
        }
        case QUIC_STREAM_EVENT_RECEIVE:
        {
            for (std::uint64_t bufferIndex = 0;
                 bufferIndex < event->RECEIVE.BufferCount; bufferIndex++)
            {
                const QUIC_BUFFER* buffer = &event->RECEIVE.Buffers[bufferIndex];

                // make a copy of the buffer, we do not need a copy, but there seems to be a bug in MsQuic with
                // lifetime management of the buffers in MultiReceive Mode
                QUIC_BUFFER* newBuffer =
                (QUIC_BUFFER*)malloc(sizeof(QUIC_BUFFER) + buffer->Length);
                newBuffer->Length = buffer->Length;
                newBuffer->Buffer = (uint8_t*)newBuffer + sizeof(QUIC_BUFFER);
                std::memcpy(newBuffer->Buffer, buffer->Buffer, buffer->Length);

                streamContext->deserializer_->append_buffer(
                UniqueQuicBuffer(newBuffer,
                                 rvn::QUIC_BUFFERDeleter(controlStream,
                                                         moqtObject.get_tbl()->StreamReceiveComplete)));
            }

            // https://github.com/microsoft/msquic/blob/f96015560399d60cbdd8608b6fa2120560118500/docs/Streams.md#synchronous-vs-asynchronous
            // we are consuming the buffer in the callback because we are making a copy
            return QUIC_STATUS_SUCCESS;
            break;
        }
        case QUIC_STREAM_EVENT_SEND_COMPLETE:
        {
            // Buffer has been sent

            // client context is the context used in the QUIC send
            // function
            StreamSendContext* streamSendContext =
            static_cast<StreamSendContext*>(event->SEND_COMPLETE.ClientContext);

            streamSendContext->send_complete_cb();
            delete streamSendContext;
            break;
        }
        default: break;
    }
    return QUIC_STATUS_SUCCESS;
};

static constexpr auto client_connection_callback =
[](HQUIC connectionHandle, void* Context, QUIC_CONNECTION_EVENT* event)
{
    MOQTClient* moqtClient = static_cast<MOQTClient*>(Context);
    const QUIC_API_TABLE* MsQuic = moqtClient->get_tbl();

    // wait until setup is done, once setup is done, flag is reset to false
    utils::wait_for(moqtClient->quicConnectionStateSetupFlag_);

    switch (event->Type)
    {
        case QUIC_CONNECTION_EVENT_CONNECTED:
        {
            // The handshake has completed for the connection.

            ConnectionState& connectionState = *moqtClient->connectionState;
            connectionState.establish_control_stream();


            auto clientSetupMessage = moqtClient->get_clientSetupMessage();
            QUIC_BUFFER* quicBuffer = serialization::serialize(clientSetupMessage);
            connectionState.send_control_buffer(quicBuffer);

            break;
        }
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        {
            // The connection has completed the shutdown process and is
            // ready to be safely cleaned up.
            std::cout << "[conn][" << connectionHandle << "] All done" << std::endl;
            if (!event->SHUTDOWN_COMPLETE.AppCloseInProgress)
            {
                MsQuic->ConnectionClose(connectionHandle);
            }
            break;
        }
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
        {
            // remotely opened streams call this callback => they must
            // be data streams
            moqtClient->accept_data_stream(event->PEER_STREAM_STARTED.Stream);

            break;
        }
        default: break;
    }
    return QUIC_STATUS_SUCCESS;
};

static constexpr auto client_listener_callback = [](HQUIC, void*, QUIC_LISTENER_EVENT*)
{
    utils::ASSERT_LOG_THROW(false, "Client Listener Callback should never be "
                                   "called");
    return QUIC_STATUS_SUCCESS;
};

} // namespace rvn::callbacks
