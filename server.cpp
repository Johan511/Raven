#include <msquic.h>

#include <iostream>
#include <moqt.hpp>

auto DataStreamCallBack = [](HQUIC Stream, void* Context,
                             QUIC_STREAM_EVENT* Event) {
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

void ServerSend(HQUIC Stream) {
    void* SendBufferRaw =
        malloc(sizeof(QUIC_BUFFER) + SendBufferLength);
    if (SendBufferRaw == NULL) {
        printf("SendBuffer allocation failed!\n");
        MsQuic->StreamShutdown(
            Stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
        return;
    }
    QUIC_BUFFER* SendBuffer = (QUIC_BUFFER*)SendBufferRaw;
    SendBuffer->Buffer =
        (uint8_t*)SendBufferRaw + sizeof(QUIC_BUFFER);
    SendBuffer->Length = SendBufferLength;

    SendBuffer->Buffer[0] = 'H';
    SendBuffer->Buffer[1] = 'H';
    SendBuffer->Buffer[2] = 'N';
    SendBuffer->Buffer[3] = '\0';

    printf("[strm][%p] Sending data...\n", Stream);

    //
    // Sends the buffer over the stream. Note the FIN flag is
    // passed along with the buffer. This indicates this is the
    // last buffer on the stream and the the stream is shut down
    // (in the send direction) immediately after.
    //
    QUIC_STATUS Status;
    if (QUIC_FAILED(Status = MsQuic->StreamSend(
                        Stream, SendBuffer, 1,
                        QUIC_SEND_FLAG_FIN, SendBuffer))) {
        printf("StreamSend failed, 0x%x!\n", Status);
        free(SendBufferRaw);
        MsQuic->StreamShutdown(
            Stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
    }
}

auto ControlStreamCallback = [](HQUIC Stream, void* Context,
                                QUIC_STREAM_EVENT* Event) {
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_SEND_COMPLETE:
            free(Event->SEND_COMPLETE.ClientContext);
            break;
        case QUIC_STREAM_EVENT_RECEIVE:
            // verify subscriber
            StreamContext* context =
                static_cast<StreamContext*>(Context);
            HQUIC dataStream = NULL;
            MsQuic->StreamOpen(
                context->connection,
                QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
                (void*)MOQT::data_stream_cb_wrapper, Context,
                &dataStream);

            // TODO : check flags
            MsQuic->StreamStart(dataStream,
                                QUIC_STREAM_START_FLAG_NONE);

            MsQuic->StreamSend(dataStream, SendBuffer, 1,
                               QUIC_SEND_FLAG_FIN,
                               SendBuffer) break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            ServerSend(Stream);
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
               Should receive bidirectional stream from user and
               then start transport of media
            */

            StreamContext* StreamContext = new StreamContext(
                static_cast<MOQT*>(Context), Connection);

            MsQuic->SetCallbackHandler(
                Event->PEER_STREAM_STARTED.Stream,
                (void*)MOQT::control_stream_cb_wrapper,
                StreamContext);
            break;
        case QUIC_CONNECTION_EVENT_RESUMED:
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
};

int main() {}
