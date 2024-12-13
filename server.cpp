#include <msquic.h>

#include <callbacks.hpp>
#include <contexts.hpp>
#include <cstring>
#include <iostream>
#include <memory>
#include <moqt.hpp>

#include <sanitizer/lsan_interface.h>
#include <signal.h>

#include <opencv2/opencv.hpp>
void handler(int signum)
{
    // __lsan_do_leak_check();
}

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
    // signal(SIGINT, handler);

    std::unique_ptr<MOQTServer> moqtServer = std::make_unique<MOQTServer>();

    QUIC_REGISTRATION_CONFIG RegConfig = { "quicsample", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
    moqtServer->set_regConfig(&RegConfig);

    moqtServer->set_listenerCb(callbacks::server_listener_callback);
    moqtServer->set_connectionCb(callbacks::server_connection_callback);
    moqtServer->set_controlStreamCb(callbacks::server_control_stream_callback);
    moqtServer->set_dataStreamCb(callbacks::server_data_stream_callback);

    QUIC_BUFFER AlpnBuffer = { sizeof("sample") - 1, (uint8_t*)"sample" };
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
    Settings.IsSet.StreamMultiReceiveEnabled = TRUE;
    Settings.StreamMultiReceiveEnabled = TRUE;

    moqtServer->set_Settings(&Settings, sizeof(Settings));
    moqtServer->set_CredConfig(get_cred_config());

    QUIC_ADDR Address;
    std::memset(&Address, 0, sizeof(Address));
    QuicAddrSetFamily(&Address, QUIC_ADDRESS_FAMILY_UNSPEC);
    const uint16_t UdpPort = 4567;
    QuicAddrSetPort(&Address, UdpPort);

    moqtServer->start_listener(&Address);

    std::thread th(
    [&]()
    {
        cv::VideoCapture cap(0);
        if (!cap.isOpened())
        {
            std::cerr << "Error: Could not open the camera." << std::endl;
            return;
        }

        int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        std::uint64_t i = 0;
        while (true)
        {
            cv::Mat frame;

            cap >> frame;

            if (frame.empty())
                continue;

            std::vector<uchar> buffer;
            cv::imencode(".jpg", frame, buffer);


            std::string image(buffer.begin(), buffer.end());
            moqtServer->register_object("default", "default", 0, i++, image);
        }
    });

    utils::thread_set_max_priority(th);
    utils::thread_set_affinity(th, 0);

    {
        char c;
        while (1)
            std::cin >> c;
    }
}
