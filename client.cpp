#include <msquic.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <moqt.hpp>

using namespace rvn;
const char *Target = "127.0.0.1";
const uint16_t UdpPort = 4567;
const uint64_t IdleTimeoutMs = 1000;

QUIC_CREDENTIAL_CONFIG *get_cred_config() {
    QUIC_CREDENTIAL_CONFIG *CredConfig =
        (QUIC_CREDENTIAL_CONFIG *)malloc(sizeof(QUIC_CREDENTIAL_CONFIG));

    memset(CredConfig, 0, sizeof(QUIC_CREDENTIAL_CONFIG));
    CredConfig->Type = QUIC_CREDENTIAL_TYPE_NONE;
    CredConfig->Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
    return CredConfig;
}

auto ClientConnectionCallback = [](HQUIC Connection, void *Context, QUIC_CONNECTION_EVENT *Event) {
    const QUIC_API_TABLE *MsQuic = static_cast<MOQT *>(Context)->get_tbl();
    switch (Event->Type) {
    case QUIC_CONNECTION_EVENT_CONNECTED:
        //
        // The handshake has completed for the connection.
        //
        printf("[conn][%p] Connected\n", Connection);
        // ClientSend(Connection);
        break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
        //
        // The connection has been shut down by the transport. Generally, this
        // is the expected way for the connection to shut down with this
        // protocol, since we let idle timeout kill the connection.
        //
        if (Event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status == QUIC_STATUS_CONNECTION_IDLE) {
            printf("[conn][%p] Successfully shut down on idle.\n", Connection);
        } else {
            printf("[conn][%p] Shut down by transport, 0x%x\n", Connection,
                   Event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
        }
        break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
        //
        // The connection was explicitly shut down by the peer.
        //
        printf("[conn][%p] Shut down by peer, 0x%llu\n", Connection,
               (unsigned long long)Event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode);
        break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        //
        // The connection has completed the shutdown process and is ready to be
        // safely cleaned up.
        //
        printf("[conn][%p] All done\n", Connection);
        if (!Event->SHUTDOWN_COMPLETE.AppCloseInProgress) {
            MsQuic->ConnectionClose(Connection);
        }
        break;
    case QUIC_CONNECTION_EVENT_RESUMPTION_TICKET_RECEIVED:
        //
        // A resumption ticket (also called New Session Ticket or NST) was
        // received from the server.
        //
        printf("[conn][%p] Resumption ticket received (%u bytes):\n", Connection,
               Event->RESUMPTION_TICKET_RECEIVED.ResumptionTicketLength);
        for (uint32_t i = 0; i < Event->RESUMPTION_TICKET_RECEIVED.ResumptionTicketLength; i++) {
            printf("%.2X", (uint8_t)Event->RESUMPTION_TICKET_RECEIVED.ResumptionTicket[i]);
        }
        printf("\n");
        break;
    default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
};

auto ClientListenerCallback = [](HQUIC, void *, QUIC_LISTENER_EVENT *) {
    assert(false);
    return QUIC_STATUS_SUCCESS;
};

int main() {
    std::unique_ptr<MOQTClient> moqtClient = std::make_unique<MOQTClient>();

    QUIC_REGISTRATION_CONFIG RegConfig = {"quicsample", QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    moqtClient->set_regConfig(&RegConfig);

    QUIC_SETTINGS Settings;
    std::memset(&Settings, 0, sizeof(Settings));

    Settings.IdleTimeoutMs = IdleTimeoutMs;
    Settings.IsSet.IdleTimeoutMs = TRUE;

    moqtClient->set_Settings(&Settings, sizeof(Settings));
    moqtClient->set_CredConfig(get_cred_config());
    QUIC_BUFFER AlpnBuffer = {sizeof("sample") - 1, (uint8_t *)"sample"};
    moqtClient->set_AlpnBuffers(&AlpnBuffer);

    moqtClient->set_AlpnBufferCount(1);

    moqtClient->set_connectionCb(ClientConnectionCallback);
    moqtClient->set_listenerCb(ClientListenerCallback);

    moqtClient->start_connection(QUIC_ADDRESS_FAMILY_UNSPEC, Target, UdpPort);

    {
        char c;
        while (1)
            std::cin >> c;
    }
}
