#include <msquic.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <moqt.hpp>

using namespace rvn;
const char* Target = "127.0.0.1";
const uint16_t UdpPort = 4567;

QUIC_CREDENTIAL_CONFIG* get_cred_config() {
    QUIC_CREDENTIAL_CONFIG* CredConfig = new QUIC_CREDENTIAL_CONFIG();
    memset(&CredConfig, 0, sizeof(CredConfig));
    CredConfig->Type = QUIC_CREDENTIAL_TYPE_NONE;
    CredConfig->Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
    CredConfig->Flags |= QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
    return CredConfig;
}

int main() {
    std::unique_ptr<MOQTClient> moqtClient = std::make_unique<MOQTClient>();

    QUIC_REGISTRATION_CONFIG RegConfig = {"quicsample", QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    moqtClient->set_regConfig(&RegConfig);

    QUIC_SETTINGS Settings = {0};
    Settings.IdleTimeoutMs = IdleTimeoutMs;
    Settings.IsSet.IdleTimeoutMs = TRUE;

    moqtClient->set_Settings(&Settings, sizeof(Settings));
    moqtClient->set_CredConfig(get_cred_config());

    moqtClient->start_connection(QUIC_ADDRESS_FAMILY_UNSPEC, Target, UdpPort);

    {
        char c;
        while (1) std::cin >> c;
    }
}
