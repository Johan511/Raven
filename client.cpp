#include <iostream>
#include <msquic.h>

const QUIC_REGISTRATION_CONFIG RegConfig = {"quicsample",
                                            QUIC_EXECUTION_PROFILE_LOW_LATENCY};
const QUIC_API_TABLE *MsQuic;

const uint16_t UdpPort = 4567;

HQUIC Registration;
HQUIC Configuration;

_IRQL_requires_max_(PASSIVE_LEVEL)
    _Function_class_(QUIC_LISTENER_CALLBACK) QUIC_STATUS QUIC_API
    ClientConnectionCallback(_In_ HQUIC Listener, _In_opt_ void *Context,
                             _Inout_ QUIC_LISTENER_EVENT *Event) {
  std::cout << "ClientConnectionCallback" << '\n';
  return 0;
}

const QUIC_BUFFER Alpn = {sizeof("Raven") - 1, (uint8_t *)"Raven"};

_IRQL_requires_max_(DISPATCH_LEVEL)
    _Function_class_(QUIC_CONNECTION_CALLBACK) QUIC_STATUS QUIC_API
    ClientConnectionCallback(_In_ HQUIC Connection, _In_opt_ void *Context,
                             _Inout_ QUIC_CONNECTION_EVENT *Event) {
  std::cout << "ClientConnectionCallback"
            << "\n";
  return QUIC_STATUS_SUCCESS;
}

BOOLEAN
ClientLoadConfiguration(BOOLEAN Unsecure) {
  QUIC_SETTINGS Settings = {0};
  //
  // Configures the client's idle timeout.
  //
  Settings.IsSet.IdleTimeoutMs = TRUE;

  //
  // Configures a default client configuration, optionally disabling
  // server certificate validation.
  //
  QUIC_CREDENTIAL_CONFIG CredConfig;
  memset(&CredConfig, 0, sizeof(CredConfig));
  CredConfig.Type = QUIC_CREDENTIAL_TYPE_NONE;
  CredConfig.Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
  if (Unsecure) {
    CredConfig.Flags |= QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
  }

  //
  // Allocate/initialize the configuration object, with the configured ALPN
  // and settings.
  //
  QUIC_STATUS Status = QUIC_STATUS_SUCCESS;
  if (QUIC_FAILED(Status = MsQuic->ConfigurationOpen(
                      Registration, &Alpn, 1, &Settings, sizeof(Settings), NULL,
                      &Configuration))) {
    printf("ConfigurationOpen failed, 0x%x!\n", Status);
    return FALSE;
  }

  //
  // Loads the TLS credential part of the configuration. This is required even
  // on client side, to indicate if a certificate is required or not.
  //
  if (QUIC_FAILED(Status = MsQuic->ConfigurationLoadCredential(Configuration,
                                                               &CredConfig))) {
    printf("ConfigurationLoadCredential failed, 0x%x!\n", Status);
    return FALSE;
  }

  return TRUE;
}

int main() {
  MsQuicOpen2(&MsQuic);
  MsQuic->RegistrationOpen(&RegConfig, &Registration);

  QUIC_STATUS Status;
  HQUIC Connection = NULL;

  QUIC_ADDR Address = {0};
  QuicAddrSetFamily(&Address, QUIC_ADDRESS_FAMILY_UNSPEC);
  QuicAddrSetPort(&Address, UdpPort);

  const char *Target = "127.0.0.1";

  ClientLoadConfiguration(true);

  MsQuic->ConnectionOpen(Registration, ClientConnectionCallback, NULL,
                         &Connection);

  MsQuic->ConnectionStart(Connection, Configuration, QUIC_ADDRESS_FAMILY_UNSPEC,
                          Target, UdpPort);

  getchar();

  return 0;
}