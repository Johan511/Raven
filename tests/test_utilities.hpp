#pragma once

/////////////////////////////////////////////////////////
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <sys/wait.h>
/////////////////////////////////////////////////////////
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
/////////////////////////////////////////////////////////
#include <callbacks.hpp>
#include <contexts.hpp>
#include <moqt.hpp>
#include <subscription_builder.hpp>
#include <utilities.hpp>
/////////////////////////////////////////////////////////

static const char* CertFile = RAVEN_CERT_FILE_PATH;
static const char* KeyFile = RAVEN_KEY_FILE_PATH;

static inline std::unique_ptr<rvn::MOQTClient>
client_setup(std::tuple<QUIC_EXECUTION_CONFIG*, std::uint64_t> executionConfig = { nullptr, 0 },
             const char* Target = "127.0.0.1",
             std::uint16_t serverPort = 4567)
{
    std::unique_ptr<rvn::MOQTClient> moqtClient =
    std::make_unique<rvn::MOQTClient>(executionConfig);

    QUIC_REGISTRATION_CONFIG RegConfig = { "test1", QUIC_EXECUTION_PROFILE_TYPE_REAL_TIME };
    moqtClient->set_regConfig(&RegConfig);

    QUIC_SETTINGS Settings;
    std::memset(&Settings, 0, sizeof(Settings));
    Settings.IdleTimeoutMs = 0;
    Settings.IsSet.IdleTimeoutMs = TRUE;
    Settings.PeerUnidiStreamCount = (std::numeric_limits<std::uint16_t>::max());
    Settings.IsSet.PeerUnidiStreamCount = TRUE;
    Settings.IsSet.StreamMultiReceiveEnabled = TRUE;
    Settings.StreamMultiReceiveEnabled = TRUE;
    Settings.IsSet.SendBufferingEnabled = TRUE;
    Settings.SendBufferingEnabled = FALSE;
    Settings.IsSet.MaxAckDelayMs = TRUE;
    Settings.MaxAckDelayMs = 1;
    Settings.IsSet.StreamRecvWindowDefault = TRUE;
    Settings.StreamRecvWindowDefault = std::bit_floor(
    std::numeric_limits<decltype(Settings.StreamRecvWindowDefault)>::max());
    Settings.IsSet.StreamRecvBufferDefault = TRUE;
    Settings.StreamRecvBufferDefault = std::bit_floor(
    std::numeric_limits<decltype(Settings.StreamRecvBufferDefault)>::max());
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

    std::cout << "Connecting to port " << serverPort << std::endl;
    return moqtClient;
}

static inline std::unique_ptr<rvn::MOQTServer>
server_setup(std::tuple<QUIC_EXECUTION_CONFIG*, std::uint64_t> executionConfig = { nullptr, 0 },
             std::uint16_t serverPort = 4567)
{
    auto dm = std::make_shared<rvn::DataManager>();
    std::unique_ptr<rvn::MOQTServer> moqtServer =
    std::make_unique<rvn::MOQTServer>(dm, executionConfig);

    QUIC_REGISTRATION_CONFIG RegConfig = { "test1", QUIC_EXECUTION_PROFILE_TYPE_REAL_TIME };
    moqtServer->set_regConfig(&RegConfig);

    moqtServer->set_listenerCb(rvn::callbacks::server_listener_callback);
    moqtServer->set_connectionCb(rvn::callbacks::server_connection_callback);
    moqtServer->set_controlStreamCb(rvn::callbacks::server_control_stream_callback);
    moqtServer->set_dataStreamCb(rvn::callbacks::server_data_stream_callback);

    QUIC_BUFFER AlpnBuffer = { sizeof("test1") - 1, (uint8_t*)"test1" };
    moqtServer->set_AlpnBuffers(&AlpnBuffer);
    moqtServer->set_AlpnBufferCount(1);

    const uint64_t IdleTimeoutMs = 0;
    QUIC_SETTINGS Settings;
    std::memset(&Settings, 0, sizeof(Settings));
    Settings.IdleTimeoutMs = IdleTimeoutMs;
    Settings.IsSet.IdleTimeoutMs = TRUE;
    Settings.PeerUnidiStreamCount = (std::numeric_limits<std::uint16_t>::max());
    Settings.IsSet.PeerUnidiStreamCount = TRUE;
    Settings.PeerBidiStreamCount = 1;
    Settings.IsSet.PeerBidiStreamCount = TRUE;
    Settings.IsSet.StreamMultiReceiveEnabled = TRUE;
    Settings.StreamMultiReceiveEnabled = TRUE;
    Settings.IsSet.SendBufferingEnabled = TRUE;
    Settings.SendBufferingEnabled = FALSE;
    Settings.IsSet.StreamRecvWindowDefault = TRUE;
    Settings.StreamRecvWindowDefault = std::bit_floor(
    std::numeric_limits<decltype(Settings.StreamRecvWindowDefault)>::max());
    Settings.IsSet.StreamRecvBufferDefault = TRUE;
    Settings.StreamRecvBufferDefault = std::bit_floor(
    std::numeric_limits<decltype(Settings.StreamRecvBufferDefault)>::max());
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

    std::cout << "Server started on port " << serverPort << std::endl;
    return moqtServer;
}

struct NetemRAII
{
    // Function to get the primary network interface
    static std::string get_network_interface()
    {
        return "lo";
    }

    NetemRAII(double lossPercentage, double kBitRate, double delayMs, double delayJitter)
    {
        // Get the correct network interface dynamically
        std::string iface = get_network_interface();
        if (iface.empty())
        {
            std::cerr << "Failed to detect network interface." << std::endl;
            abort();
        }

        std::string cmd = "sudo tc qdisc replace dev " + iface +
                          " root netem loss " + std::to_string(lossPercentage) +
                          "% rate " + std::to_string(kBitRate) + "kbit delay " +
                          std::to_string(delayMs) + "ms " +
                          std::to_string(delayJitter) + "ms distribution normal";

        if (std::system(cmd.c_str()) != 0)
        {
            std::cerr << "Failed to set network emulation." << std::endl;
            abort();
        }
    }

    ~NetemRAII()
    {
        std::string iface = get_network_interface();

        std::string cmd = "sudo tc qdisc del dev " + iface + " root netem";
        int ret = std::system(cmd.c_str());
        if (ret != 0)
        {
            std::cerr << "Failed to remove network emulation." << std::endl;
            abort();
        }
    }
};

static inline std::uint64_t get_current_ms_timestamp()
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
    .count();
}
