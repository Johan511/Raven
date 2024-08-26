#include <msquic.h>

#include <moqt.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>
#include <contexts.hpp>

namespace rvn {

MOQTServer::MOQTServer() : MOQT() {};

void MOQTServer::start_listener(QUIC_ADDR *LocalAddress) {
    rvn::utils::ASSERT_LOG_THROW(secondaryCounter == full_sec_counter_value(), "secondaryCounter ",
                                 secondaryCounter, " full_sec_counter_value() ",
                                 full_sec_counter_value());

    reg = rvn::unique_registration(tbl.get(), regConfig);
    configuration = rvn::unique_configuration(
        tbl.get(), {reg.get(), AlpnBuffers, AlpnBufferCount, Settings, SettingsSize, this},
        {CredConfig});
    listener = rvn::unique_listener(tbl.get(), {reg.get(), MOQT::listener_cb_wrapper, this},
                                    {AlpnBuffers, AlpnBufferCount, LocalAddress});
}

QUIC_STATUS
MOQTServer::register_new_connection([[maybe_unused]] HQUIC listener, auto newConnectionInfo) {
    QUIC_STATUS status = QUIC_STATUS_NOT_SUPPORTED;
    HQUIC connection = NULL;
    status =
        get_tbl()->ConnectionSetConfiguration(newConnectionInfo.Connection, configuration.get());

    if (QUIC_FAILED(status)) {
        return status;
    }

    get_tbl()->SetCallbackHandler(newConnectionInfo.Connection,
                                  (void *)(this->connection_cb_wrapper), (void *)(this));

    connectionStateMap[connection] = ConnectionState{connection};

    return status;
}

QUIC_STATUS
MOQTServer::register_control_stream(HQUIC connection, auto newStreamInfo) {
    ConnectionState &connectionState = connectionStateMap.at(connection);
    utils::ASSERT_LOG_THROW(!connectionState.controlStream.has_value(),
                            "Control stream already registered by connection: ", connection);
    connectionState.controlStream = StreamState{newStreamInfo.Stream, DEFAULT_BUFFER_CAPACITY};

    StreamState &streamState = connectionState.controlStream.value();
    streamState.set_stream_context(std::make_unique<StreamContext>(this, connection));
    this->get_tbl()->SetCallbackHandler(newStreamInfo.Stream,
                                        (void *)MOQT::control_stream_cb_wrapper,
                                        (void *)streamState.streamContext.get());

    // registering stream can not fail
    return QUIC_STATUS_SUCCESS;
}

namespace callbacks {

auto server_listener_callback = [](HQUIC listener, void *context,
                                   QUIC_LISTENER_EVENT *event) -> QUIC_STATUS {
    MOQTServer *moqtServer = static_cast<MOQTServer *>(context);

    QUIC_STATUS status = QUIC_STATUS_NOT_SUPPORTED;
    switch (event->Type) {
    case QUIC_LISTENER_EVENT_NEW_CONNECTION:
        status = moqtServer->register_new_connection(listener, event->NEW_CONNECTION);
        break;
    case QUIC_LISTENER_EVENT_STOP_COMPLETE:
        // we have called stop on listener (opposite to start)
        break;
    default:
        break;
    }
    return status;
};

auto server_connection_callback = [](HQUIC connection, void *context,
                                     QUIC_CONNECTION_EVENT *event) -> QUIC_STATUS {
    MOQTServer *moqtServer = static_cast<MOQTServer *>(context);
    const QUIC_API_TABLE *MsQuic = moqtServer->get_tbl();

    switch (event->Type) {
    case QUIC_CONNECTION_EVENT_CONNECTED:
        MsQuic->ConnectionSendResumptionTicket(connection, QUIC_SEND_RESUMPTION_FLAG_NONE, 0, NULL);
        break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
        if (event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status == QUIC_STATUS_CONNECTION_IDLE) {
        } else {
        }
        break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
        break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE: {
        auto &connectionStateMap = moqtServer->get_connectionStateMap();
        utils::ASSERT_LOG_THROW(connectionStateMap.find(connection) != connectionStateMap.end(),
                                "Connection not found in connectionStateMap");
        // deletes connection related stuff in MsQuic
        MsQuic->ConnectionClose(connection);
        // free the state we maintain of the connection
        connectionStateMap.erase(connection);
        break;
    }
    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED: {
        // remotely opened streams call this callback => they must be control streams
        moqtServer->register_control_stream(connection, event->PEER_STREAM_STARTED);

        break;
    }
    case QUIC_CONNECTION_EVENT_RESUMED:
        break;
    default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
};

auto server_control_stream_callback = []([[maybe_unused]] HQUIC controlStream, void *context,
                                         QUIC_STREAM_EVENT *event) {
    StreamContext *streamContext = static_cast<StreamContext *>(context);
    HQUIC connection = streamContext->connection;
    MOQT *moqtObject = streamContext->moqtObject;

    switch (event->Type) {
    case QUIC_STREAM_EVENT_START_COMPLETE: {
        // called when attempting to setup stream connection
        LOGE("Server should never start BiDirectionalControlStream");
        break;
    }
    case QUIC_STREAM_EVENT_RECEIVE: {
        // Received Control Message
        // auto receiveInformation = event->RECEIVE;
        auto &connectionState = moqtObject->get_connectionStateMap().at(connection);
        moqtObject->interpret_control_message(connectionState, &(event->RECEIVE));
        break;
    }
    case QUIC_STREAM_EVENT_SEND_COMPLETE: {
        // Buffer has been sent

        // client context is the context used in the QUIC send function
        StreamSendContext *streamSendContext =
            static_cast<StreamSendContext *>(event->SEND_COMPLETE.ClientContext);

        streamSendContext->send_complete_cb();
        delete streamSendContext;
        break;
    }
    default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
};

auto server_data_stream_callback = [](HQUIC dataStream, void *context, QUIC_STREAM_EVENT *event) {
    StreamContext *streamContext = static_cast<StreamContext *>(context);
    HQUIC connection = streamContext->connection;
    MOQT *moqtObject = streamContext->moqtObject;

    switch (event->Type) {
    case QUIC_STREAM_EVENT_START_COMPLETE: {
        // called when attempting to setup stream connection
        LOGE("Server should never start BiDirectionalDataStream");
        break;
    }
    case QUIC_STREAM_EVENT_RECEIVE: {
        // Received Data Message
        // auto receiveInformation = event->RECEIVE;
        auto &connectionState = moqtObject->get_connectionStateMap().at(connection);
        moqtObject->interpret_data_message(connectionState, dataStream, &(event->RECEIVE));
        break;
    }
    case QUIC_STREAM_EVENT_SEND_COMPLETE: {
        // Buffer has been sent

        // client context is the context used in the QUIC send function
        StreamSendContext *streamSendContext =
            static_cast<StreamSendContext *>(event->SEND_COMPLETE.ClientContext);

        streamSendContext->send_complete_cb();
        delete streamSendContext;
        break;
    }
    default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
};

} // namespace callbacks
} // namespace rvn
