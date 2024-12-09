#include <memory>
#include <msquic.h>

#include <contexts.hpp>
#include <moqt.hpp>
#include <protobuf_messages.hpp>
#include <serialization.hpp>
#include <string>
#include <utilities.hpp>
#include <wrappers.hpp>

namespace rvn::callbacks
{

static constexpr auto server_listener_callback =
[](HQUIC listener, void* context, QUIC_LISTENER_EVENT* event) -> QUIC_STATUS
{
    MOQTServer* moqtServer = static_cast<MOQTServer*>(context);

    QUIC_STATUS status = QUIC_STATUS_NOT_SUPPORTED;
    switch (event->Type)
    {
        case QUIC_LISTENER_EVENT_NEW_CONNECTION:
            status = moqtServer->accept_new_connection(listener, event->NEW_CONNECTION);
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
            auto& connectionStateMap = moqtServer->get_connectionStateMap();
            utils::ASSERT_LOG_THROW(connectionStateMap.find(connection) !=
                                    connectionStateMap.end(),
                                    "Connection not found in "
                                    "connectionStateMap");
            // deletes connection related stuff in MsQuic
            MsQuic->ConnectionClose(connection);
            // free the state we maintain of the connection
            connectionStateMap.erase(connection);
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
    HQUIC connection = streamContext->connection;
    MOQT* moqtObject = streamContext->moqtObject;

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
            try
            {
                auto& connectionState = moqtObject->get_connectionStateMap().at(connection);
                moqtObject->handle_message(connectionState, controlStream,
                                           &(event->RECEIVE));
            }
            catch (const std::out_of_range& e)
            {
                // connection was removed from connection map
                LOGE("Connection not found in connectionStateMap");
                // TODO: close connection
            }
            catch (const rvn::exception::parsing_exception& e)
            {
                LOGE("Failure while trying to parse: ", e.what());
                // TODO: close connection
            }

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
[](HQUIC dataStream, void* context, QUIC_STREAM_EVENT* event)
{
    StreamContext* streamContext = static_cast<StreamContext*>(context);
    HQUIC connection = streamContext->connection;
    MOQT* moqtObject = streamContext->moqtObject;

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
            ConnectionState& connectionState =
            moqtObject->get_connectionStateMap().at(connection);
            connectionState.delete_data_stream(dataStream);
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
    HQUIC connectionHandle = streamContext->connection;
    MOQT* moqtObject = streamContext->moqtObject;

    switch (event->Type)
    {
        case QUIC_STREAM_EVENT_START_COMPLETE:
        {
            // called when attempting to setup stream connection
            break;
        }
        case QUIC_STREAM_EVENT_RECEIVE:
        {
            // Received Data Message
            StreamState* streamState =
            moqtObject->get_stream_state(connectionHandle, dataStream);

            // accumulate all data messages received and read them when closing stream
            for (std::uint64_t bufferIndex = 0;
                 bufferIndex < event->RECEIVE.BufferCount; bufferIndex++)
            {
                const QUIC_BUFFER* buffer = &event->RECEIVE.Buffers[bufferIndex];
                streamState->messageSS.write(reinterpret_cast<const char*>(
                                             buffer->Buffer),
                                             buffer->Length);
            }

            break;
        }
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
        {
            StreamState* streamState =
            moqtObject->get_stream_state(connectionHandle, dataStream);

            ConnectionState& connectionState =
            moqtObject->get_connectionStateMap().at(connectionHandle);

            moqtObject->handle_message(connectionState, dataStream, streamState->messageSS);

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
    HQUIC connection = streamContext->connection;
    MOQT* moqtObject = streamContext->moqtObject;

    switch (event->Type)
    {
        case QUIC_STREAM_EVENT_START_COMPLETE:
        {
            // called when attempting to setup stream connection
            break;
        }
        case QUIC_STREAM_EVENT_RECEIVE:
        {
            // Received Control Message
            // auto receiveInformation = event->RECEIVE;
            try
            {
                auto& connectionState = moqtObject->get_connectionStateMap().at(connection);
                moqtObject->handle_message(connectionState, controlStream,
                                           &(event->RECEIVE));
            }
            catch (const std::out_of_range& e)
            {
                // connection was removed from connection map
                LOGE("Connection not found in connectionStateMap");
                // TODO: close connection
            }
            catch (const rvn::exception::parsing_exception& e)
            {
                LOGE("Failure while trying to parse: ", e.what());
                // TODO: close connection
            }

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

    do
    {

    }while(moqtClient->connectionSetupFlag.load(std::memory_order_acquire)); 
    // wait until setup is done, once setup is done, flag is reset to false

    switch (event->Type)
    {
        case QUIC_CONNECTION_EVENT_CONNECTED:
        {
            //
            // The handshake has completed for the connection.
            //
            ConnectionState& connectionState =
            moqtClient->get_connectionStateMap().at(connectionHandle);

            protobuf_messages::MessageHeader messageHeader;
            messageHeader.set_messagetype(protobuf_messages::MoQtMessageType::CLIENT_SETUP);

            protobuf_messages::ClientSetupMessage clientSetupMessage =
            moqtClient->get_clientSetupMessage();


            QUIC_BUFFER* quicBuffer =
            serialization::serialize(messageHeader, clientSetupMessage);
            connectionState.enqueue_control_buffer(quicBuffer);

            break;
        }
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        {
            //
            // The connection has completed the shutdown process and is
            // ready to be safely cleaned up.
            //
            printf("[conn][%p] All done\n", connectionHandle);
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
            moqtClient->accept_data_stream(connectionHandle,
                                           event->PEER_STREAM_STARTED.Stream);

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
