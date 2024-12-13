#include <msquic.h>

#include <callbacks.hpp>
#include <cstring>
#include <iostream>
#include <memory>
#include <moqt.hpp>
#include <subscription_builder.hpp>
#include <thread>

#include <opencv2/opencv.hpp>

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


    std::thread th(
    [&]()
    {
        do
        {

        } while (!moqtClient->isConnected.load(std::memory_order_acquire));
        SubscriptionBuilder subBuilder;
        subBuilder.set_data_range<SubscriptionBuilder::Filter::LatestObject>(
        std::uint64_t(0));
        subBuilder.set_track_alias(0);
        subBuilder.set_track_namespace("default");
        subBuilder.set_track_name("default");
        subBuilder.set_subscriber_priority(0);
        subBuilder.set_group_order(0);
        auto subMessage = subBuilder.build();
        auto queueIter = moqtClient->subscribe(std::move(subMessage));
        while (true)
        {
            std::string objectPayloadStr;
            queueIter->wait_dequeue(objectPayloadStr);
            if (objectPayloadStr.empty())
                continue;
            cv::Mat frame;
            std::vector<uchar> buffer(objectPayloadStr.begin(), objectPayloadStr.end());
            frame = cv::imdecode(buffer, cv::IMREAD_COLOR);
            cv::imshow("Live Video Feed", frame);
            using namespace std::chrono_literals;
            if (cv::waitKey(30) == 'q')
                break;
        }
    });

    utils::thread_set_max_priority(th);
    utils::thread_set_affinity(th, 1);

    {
        char c;
        while (1)
            std::cin >> c;
    }
}
