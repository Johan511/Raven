#include <callbacks.hpp>
#include <contexts.hpp>
#include <cstdint>
#include <memory>
#include <moqt.hpp>
#include <subscription_builder.hpp>


const std::uint16_t serverPort = 4567;
const char* data = "Hello World!";

using namespace rvn;

typedef struct QUIC_CREDENTIAL_CONFIG_HELPER
{
    QUIC_CREDENTIAL_CONFIG CredConfig;
    union
    {
        QUIC_CERTIFICATE_HASH CertHash;
        QUIC_CERTIFICATE_HASH_STORE CertHashStore;
        QUIC_CERTIFICATE_FILE CertFile;
        QUIC_CERTIFICATE_FILE_PROTECTED CertFileProtected;
    };
} QUIC_CREDENTIAL_CONFIG_HELPER;

QUIC_CREDENTIAL_CONFIG* get_cred_config()
{
    const char* CertFile = RAVEN_CERT_FILE_PATH;
    const char* KeyFile = RAVEN_KEY_FILE_PATH;

    QUIC_CREDENTIAL_CONFIG_HELPER* Config =
    (QUIC_CREDENTIAL_CONFIG_HELPER*)malloc(sizeof(QUIC_CREDENTIAL_CONFIG_HELPER));

    memset(Config, 0, sizeof(QUIC_CREDENTIAL_CONFIG_HELPER));

    Config->CredConfig.Flags = QUIC_CREDENTIAL_FLAG_NONE;
    Config->CertFile.CertificateFile = (char*)CertFile;
    Config->CertFile.PrivateKeyFile = (char*)KeyFile;
    Config->CredConfig.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
    Config->CredConfig.CertificateFile = &Config->CertFile;

    return &(Config->CredConfig);
}


int main()
{
    auto dm = std::make_shared<DataManager>();
    auto sm = std::make_shared<SubscriptionManager>(*dm);

    std::unique_ptr<MOQTServer> moqtServer = std::make_unique<MOQTServer>(dm, sm);


    QUIC_REGISTRATION_CONFIG RegConfig = { "example1", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
    moqtServer->set_regConfig(&RegConfig);

    moqtServer->set_listenerCb(callbacks::server_listener_callback);
    moqtServer->set_connectionCb(callbacks::server_connection_callback);
    moqtServer->set_controlStreamCb(callbacks::server_control_stream_callback);
    moqtServer->set_dataStreamCb(callbacks::server_data_stream_callback);

    QUIC_BUFFER AlpnBuffer = { sizeof("example1") - 1, (uint8_t*)"example1" };
    moqtServer->set_AlpnBuffers(&AlpnBuffer);

    moqtServer->set_AlpnBufferCount(1);

    const uint64_t IdleTimeoutMs = 0;
    QUIC_SETTINGS Settings;
    std::memset(&Settings, 0, sizeof(Settings));
    Settings.IdleTimeoutMs = IdleTimeoutMs;
    Settings.IsSet.IdleTimeoutMs = TRUE;
    Settings.ServerResumptionLevel = QUIC_SERVER_RESUME_AND_ZERORTT;
    Settings.IsSet.ServerResumptionLevel = TRUE;
    Settings.PeerBidiStreamCount = 1;
    Settings.IsSet.PeerBidiStreamCount = TRUE;
    moqtServer->set_Settings(&Settings, sizeof(Settings));

    moqtServer->set_CredConfig(get_cred_config());

    QUIC_ADDR Address;
    std::memset(&Address, 0, sizeof(Address));
    QuicAddrSetFamily(&Address, QUIC_ADDRESS_FAMILY_UNSPEC);
    QuicAddrSetPort(&Address, serverPort);

    moqtServer->start_listener(&Address);
    {
        char c;
        while (1)
            std::cin >> c;
    }
}
