#include <msquic.h>

#include <callbacks.hpp>
#include <cstring>
#include <iostream>
#include <memory>
#include <moqt.hpp>
#include <thread>

#include <opencv2/opencv.hpp>


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

void set_thread_affinity(std::thread& th, int core_id)
{
    // Get native thread handle
    pthread_t native_handle = th.native_handle();

    // Create a CPU set and set the desired core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    // Set affinity for the thread
    int result = pthread_setaffinity_np(native_handle, sizeof(cpu_set_t), &cpuset);
    if (result != 0)
    {
        std::cerr << "Error setting thread affinity: " << result << std::endl;
    }
}

void set_max_prio(std::thread& th)
{
    sched_param sch;
    int policy;
    pthread_getschedparam(th.native_handle(), &policy, &sch);
    sch.sched_priority = sched_get_priority_max(policy);
    if (pthread_setschedparam(th.native_handle(), policy, &sch))
    {
        std::cerr << "Failed to set Thread Priority" << std::endl;
    }
}

int main()
{
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
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(5000ms);
        auto* queue = moqtClient->subscribe();
        while (true)
        {
            if (!queue->empty())
            {
                std::cout << "Queue Size: " << queue->size() << std::endl;
                std::cout << queue->size() << std::endl;
                std::string objectPayloadStr = queue->front();
                queue->pop();

                cv::Mat frame;
                std::vector<uchar> buffer(objectPayloadStr.begin(),
                                          objectPayloadStr.end());
                frame = cv::imdecode(buffer, cv::IMREAD_COLOR);
                cv::imshow("Live Video Feed", frame);
                if (cv::waitKey(30) == 'q')
                    break;
            }
        }
    });

    set_thread_affinity(th, 1);
    set_max_prio(th);

    {
        char c;
        while (1)
            std::cin >> c;
    }
}
