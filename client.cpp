#include <msquic.h>

#include <callbacks.hpp>
#include <cstring>
#include <iostream>
#include <memory>
#include <moqt.hpp>
#include <subscription_builder.hpp>
#include <thread>

#include <sanitizer/lsan_interface.h>
#include <signal.h>
void handler(int signum)
{
    // __lsan_do_leak_check();
}


using namespace rvn;
const char* Target = "127.0.0.1";
const uint16_t UdpPort = 4567;
const uint64_t IdleTimeoutMs = 0;

QUIC_CREDENTIAL_CONFIG* get_cred_config()
{
    QUIC_CREDENTIAL_CONFIG* CredConfig =
    (QUIC_CREDENTIAL_CONFIG*)malloc(sizeof(QUIC_CREDENTIAL_CONFIG));

    memset(CredConfig, 0, sizeof(QUIC_CREDENTIAL_CONFIG));
    CredConfig->Type = QUIC_CREDENTIAL_TYPE_NONE;
    CredConfig->Flags =
    QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
    return CredConfig;
}


int main()
{
    // signal(SIGINT, handler);

    std::unique_ptr<MOQTClient> moqtClient = std::make_unique<MOQTClient>();

    QUIC_REGISTRATION_CONFIG RegConfig = { "quicsample", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
    moqtClient->set_regConfig(&RegConfig);

    QUIC_SETTINGS Settings;
    std::memset(&Settings, 0, sizeof(Settings));

    Settings.IdleTimeoutMs = IdleTimeoutMs;
    Settings.IsSet.IdleTimeoutMs = TRUE;
    Settings.IsSet.StreamMultiReceiveEnabled = TRUE;
    Settings.StreamMultiReceiveEnabled = TRUE;
    Settings.PeerUnidiStreamCount = (static_cast<uint16_t>(1) << 16) - 1;
    Settings.IsSet.PeerUnidiStreamCount = TRUE;

    moqtClient->set_Settings(&Settings, sizeof(Settings));
    moqtClient->set_CredConfig(get_cred_config());
    QUIC_BUFFER AlpnBuffer = { sizeof("sample") - 1, (uint8_t*)"sample" };
    moqtClient->set_AlpnBuffers(&AlpnBuffer);

    moqtClient->set_AlpnBufferCount(1);

    moqtClient->set_connectionCb(rvn::callbacks::client_connection_callback);
    moqtClient->set_listenerCb(rvn::callbacks::client_listener_callback);

    moqtClient->set_controlStreamCb(rvn::callbacks::client_control_stream_callback);
    moqtClient->set_dataStreamCb(rvn::callbacks::client_data_stream_callback);


    moqtClient->start_connection(QUIC_ADDRESS_FAMILY_UNSPEC, Target, UdpPort);


    {
        char c;
        while (1)
            std::cin >> c;
    }
}
