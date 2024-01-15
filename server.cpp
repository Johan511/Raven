#include <iostream>
#include <msquic.h>

const QUIC_REGISTRATION_CONFIG RegConfig = {"quicsample",
                                            QUIC_EXECUTION_PROFILE_LOW_LATENCY};
const QUIC_API_TABLE *MsQuic;

const uint16_t UdpPort = 4567;

HQUIC Registration;

_IRQL_requires_max_(PASSIVE_LEVEL)
    _Function_class_(QUIC_LISTENER_CALLBACK) QUIC_STATUS QUIC_API
    ServerListenerCallback(_In_ HQUIC Listener, _In_opt_ void *Context,
                           _Inout_ QUIC_LISTENER_EVENT *Event) {
  UNREFERENCED_PARAMETER(Listener);
  UNREFERENCED_PARAMETER(Context);
  QUIC_STATUS Status = QUIC_STATUS_NOT_SUPPORTED;
  switch (Event->Type) {
  case QUIC_LISTENER_EVENT_NEW_CONNECTION:
    //
    // A new connection is being attempted by a client. For the handshake to
    // proceed, the server must provide a configuration for QUIC to use. The
    // app MUST set the callback handler before returning.
    //
    MsQuic->SetCallbackHandler(Event->NEW_CONNECTION.Connection,
                               (void *)ServerConnectionCallback, NULL);
    Status = MsQuic->ConnectionSetConfiguration(
        Event->NEW_CONNECTION.Connection, Configuration);
    break;
  default:
    break;
  }
  return Status;
}

const QUIC_BUFFER Alpn = {sizeof("Raven") - 1, (uint8_t *)"Raven"};

int main() {
  MsQuicOpen2(&MsQuic);
  MsQuic->RegistrationOpen(&RegConfig, &Registration);

  QUIC_STATUS Status;
  HQUIC Listener = NULL;

  QUIC_ADDR Address = {0};
  QuicAddrSetFamily(&Address, QUIC_ADDRESS_FAMILY_UNSPEC);
  QuicAddrSetPort(&Address, UdpPort);

  MsQuic->ListenerOpen(Registration, ServerListenerCallback, NULL, &Listener);

  MsQuic->ListenerStart(Listener, &Alpn, 1, &Address);

  getchar();

  return 0;
}