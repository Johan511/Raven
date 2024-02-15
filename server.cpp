#include <msquic.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <moqt.hpp>

using namespace rvn;

auto DataStreamCallBack = [](HQUIC Stream, void* Context,
                             QUIC_STREAM_EVENT* Event) {
    const QUIC_API_TABLE* MsQuic =
        static_cast<StreamContext*>(Context)
            ->moqtObject->get_tbl();
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_SEND_COMPLETE:
            free(Event->SEND_COMPLETE.ClientContext);
            break;
        case QUIC_STREAM_EVENT_RECEIVE:
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
            MsQuic->StreamShutdown(
                Stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
            break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            MsQuic->StreamClose(Stream);
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
};

auto ControlStreamCallback = [](HQUIC Stream, void* Context,
                                QUIC_STREAM_EVENT* Event) {
    const QUIC_API_TABLE* MsQuic =
        static_cast<StreamContext*>(Context)
            ->moqtObject->get_tbl();

    /* can not define them in switch case because crosses
     * initialization and C++ does not like it because it has
     to
     * call destructors */
    StreamContext* context = NULL;
    HQUIC dataStream = NULL;
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_SEND_COMPLETE:
            free(Event->SEND_COMPLETE.ClientContext);
            break;
        case QUIC_STREAM_EVENT_RECEIVE:
            // verify subscriber
            MsQuic->StreamOpen(
                context->connection,
                QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
                &MOQT::data_stream_cb_wrapper, Context,
                &dataStream);

            // TODO : check flags
            MsQuic->StreamStart(dataStream,
                                QUIC_STREAM_START_FLAG_NONE);

            // MsQuic->StreamSend(dataStream, SendBuffer, 1,
            //                    QUIC_SEND_FLAG_FIN,
            //                    SendBuffer);
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            // ServerSend(Stream);
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
            MsQuic->StreamShutdown(
                Stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
            break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            /*Should also shutdown remaining streams
              because it releases ownership of Context
             */
            free(Context);
            MsQuic->StreamClose(Stream);
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
};

auto ServerConnectionCallback = [](HQUIC Connection,
                                   void* Context,
                                   QUIC_CONNECTION_EVENT*
                                       Event) {
    const QUIC_API_TABLE* MsQuic =
        static_cast<MOQT*>(Context)->get_tbl();

    StreamContext* streamContext = NULL;

    switch (Event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            MsQuic->ConnectionSendResumptionTicket(
                Connection, QUIC_SEND_RESUMPTION_FLAG_NONE, 0,
                NULL);
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
            if (Event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status ==
                QUIC_STATUS_CONNECTION_IDLE) {
            } else {
            }
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            MsQuic->ConnectionClose(Connection);
            break;
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
            /*
               Should receive bidirectional stream from user
               and then start transport of media
            */

            streamContext = new StreamContext(
                static_cast<MOQT*>(Context), Connection);

            MsQuic->SetCallbackHandler(
                Event->PEER_STREAM_STARTED.Stream,
                (void*)MOQT::control_stream_cb_wrapper,
                streamContext);
            break;
        case QUIC_CONNECTION_EVENT_RESUMED:
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
};

QUIC_STATUS ServerListenerCallback(HQUIC Listener, void* Context,
                                   QUIC_LISTENER_EVENT* Event) {
    const QUIC_API_TABLE* MsQuic =
        static_cast<MOQT*>(Context)->get_tbl();

    auto moqtObject = static_cast<MOQT*>(Context);

    QUIC_STATUS Status = QUIC_STATUS_NOT_SUPPORTED;
    switch (Event->Type) {
        case QUIC_LISTENER_EVENT_NEW_CONNECTION:
            MsQuic->SetCallbackHandler(
                Event->NEW_CONNECTION.Connection,
                (void*)(moqtObject->connection_cb_wrapper),
                NULL);
            Status = MsQuic->ConnectionSetConfiguration(
                Event->NEW_CONNECTION.Connection,
                moqtObject->configuration.get());
            break;
        default:
            break;
    }
    return Status;
}

int main() {
    std::unique_ptr<MOQTServer> moqtServer =
        std::make_unique<MOQTServer>();

    QUIC_REGISTRATION_CONFIG RegConfig = {
        "quicsample", QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    moqtServer->set_regConfig(&RegConfig);

    moqtServer->set_listenerCb(ServerListenerCallback);

    QUIC_BUFFER AlpnBuffer = {sizeof("sample") - 1,
                              (uint8_t*)"sample"};
    moqtServer->set_AlpnBuffers(&AlpnBuffer);

    moqtServer->set_AlpnBufferCount(1);

    const uint64_t IdleTimeoutMs = 1000;
    QUIC_SETTINGS Settings;
    std::memset(&Settings, 0, sizeof(Settings));
    Settings.IdleTimeoutMs = IdleTimeoutMs;
    Settings.IsSet.IdleTimeoutMs = TRUE;
    Settings.ServerResumptionLevel =
        QUIC_SERVER_RESUME_AND_ZERORTT;
    Settings.IsSet.ServerResumptionLevel = TRUE;
    Settings.PeerBidiStreamCount = 1;
    Settings.IsSet.PeerBidiStreamCount = TRUE;

    moqtServer->set_Settings(&Settings, sizeof(Settings));

    QUIC_ADDR Address;
    std::memset(&Address, 0, sizeof(Address));
    QuicAddrSetFamily(&Address, QUIC_ADDRESS_FAMILY_UNSPEC);
    const uint16_t UdpPort = 4567;
    QuicAddrSetPort(&Address, UdpPort);

    moqtServer->start_listener(&Address);
}
