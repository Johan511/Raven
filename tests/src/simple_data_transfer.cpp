#include <callbacks.hpp>
#include <contexts.hpp>
#include <cstdint>
#include <limits>
#include <memory>
#include <moqt.hpp>
#include <subscription_builder.hpp>


const char* Target = "127.0.0.1";
const std::uint16_t serverPort = 4567;
const char* data = "Hello World!";

const char* CertFile = RAVEN_CERT_FILE_PATH;
const char* KeyFile = RAVEN_KEY_FILE_PATH;

using namespace rvn;

std::unique_ptr<MOQTClient> client_setup()
{
    std::unique_ptr<MOQTClient> moqtClient = std::make_unique<MOQTClient>();

    QUIC_REGISTRATION_CONFIG RegConfig = { "test1", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
    moqtClient->set_regConfig(&RegConfig);

    QUIC_SETTINGS Settings;
    std::memset(&Settings, 0, sizeof(Settings));
    Settings.IdleTimeoutMs = 0;
    Settings.IsSet.IdleTimeoutMs = TRUE;
    Settings.PeerUnidiStreamCount = (std::numeric_limits<std::uint16_t>::max());
    Settings.IsSet.PeerUnidiStreamCount = TRUE;
    moqtClient->set_Settings(&Settings, sizeof(Settings));

    QUIC_CREDENTIAL_CONFIG credConfig;
    std::memset(&credConfig, 0, sizeof(credConfig));
    credConfig.Type = QUIC_CREDENTIAL_TYPE_NONE;
    credConfig.Flags =
    QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
    moqtClient->set_CredConfig(&credConfig);

    QUIC_BUFFER AlpnBuffer = { sizeof("test1") - 1, (uint8_t*)"test1" };
    moqtClient->set_AlpnBuffers(&AlpnBuffer);
    moqtClient->set_AlpnBufferCount(1);

    moqtClient->set_connectionCb(rvn::callbacks::client_connection_callback);
    moqtClient->set_listenerCb(rvn::callbacks::client_listener_callback);
    moqtClient->set_controlStreamCb(rvn::callbacks::client_control_stream_callback);
    moqtClient->set_dataStreamCb(rvn::callbacks::client_data_stream_callback);

    moqtClient->start_connection(QUIC_ADDRESS_FAMILY_UNSPEC, Target, serverPort);

    return moqtClient;
}


std::unique_ptr<MOQTServer> server_setup()
{
    auto dm = std::make_shared<DataManager>();
    std::unique_ptr<MOQTServer> moqtServer = std::make_unique<MOQTServer>(dm);

    QUIC_REGISTRATION_CONFIG RegConfig = { "test1", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
    moqtServer->set_regConfig(&RegConfig);

    moqtServer->set_listenerCb(callbacks::server_listener_callback);
    moqtServer->set_connectionCb(callbacks::server_connection_callback);
    moqtServer->set_controlStreamCb(callbacks::server_control_stream_callback);
    moqtServer->set_dataStreamCb(callbacks::server_data_stream_callback);

    QUIC_BUFFER AlpnBuffer = { sizeof("test1") - 1, (uint8_t*)"test1" };
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

    // certificates
    QUIC_CERTIFICATE_FILE certFile;
    certFile.CertificateFile = (char*)CertFile;
    certFile.PrivateKeyFile = (char*)KeyFile;
    // setting up credential configuration
    QUIC_CREDENTIAL_CONFIG credConfig;
    std::memset(&credConfig, 0, sizeof(credConfig));
    credConfig.Flags = QUIC_CREDENTIAL_FLAG_NONE;
    credConfig.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
    credConfig.CertificateFile = &certFile;
    moqtServer->set_CredConfig(&credConfig);

    QUIC_ADDR Address;
    std::memset(&Address, 0, sizeof(Address));
    QuicAddrSetFamily(&Address, QUIC_ADDRESS_FAMILY_UNSPEC);
    QuicAddrSetPort(&Address, serverPort);

    moqtServer->start_listener(&Address);

    return moqtServer;
}

// TODO: use shared memory synchronization instead of `sleep_fir`
int main()
{
    /*
        Server will be out parent process and client will be child process.
    */
    if (auto childPid = fork())
    {
        // parent process, server
        std::unique_ptr<MOQTServer> moqtServer = server_setup();

        using namespace std::chrono_literals;
        // Make sure server is alive until client is setup and requests
        std::this_thread::sleep_for(3s);
    }
    else
    {
        using namespace std::chrono_literals;
        // Make sure server is alive until client is setup and requests
        std::this_thread::sleep_for(1s);

        // child process
        std::unique_ptr<MOQTClient> moqtClient = client_setup();

        SubscriptionBuilder subscriptionBuilder;
        subscriptionBuilder.set_data_range(SubscriptionBuilder::Filter::absoluteRange,
                                           { GroupId(0), ObjectId(0) },
                                           { GroupId(0), ObjectId(1) });
        subscriptionBuilder.set_track_alias(TrackAlias(0));
        subscriptionBuilder.set_track_namespace({ "tnamespace" });
        subscriptionBuilder.set_track_name("tname");
        subscriptionBuilder.set_subscriber_priority(0);
        subscriptionBuilder.set_group_order(0);

        auto subMessage = subscriptionBuilder.build();

        using namespace std::chrono_literals;
        // Make sure `register_object` is called before `subscribe`
        std::string receivedData;
        std::cout << "Received data: " << receivedData << std::endl;
        assert(receivedData == data);
    }
}
