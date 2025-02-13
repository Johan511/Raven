/////////////////////////////////////////////////////////
#include <cstdint>
#include <limits>
#include <memory>
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


const char* Target = "127.0.0.1";
const std::uint16_t serverPort = 4567;
const char* messagePayload = "Hello World!";

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

struct InterprocessSynchronizationData
{
    boost::interprocess::interprocess_mutex mutex;
    bool serverSetup;
    bool clientDone;
};

namespace bip = boost::interprocess;

// TODO: use shared memory synchronization instead of `sleep_fir`
int main()
{
    std::string sharedMemoryName = "simple_data_transfer_test_";
    sharedMemoryName += std::to_string(getpid());


    bip::shared_memory_object shmParent(bip::create_only,
                                        sharedMemoryName.c_str(), bip::read_write);
    shmParent.truncate(sizeof(InterprocessSynchronizationData));
    bip::mapped_region regionParent(shmParent, bip::read_write);
    InterprocessSynchronizationData* dataParent =
    new (regionParent.get_address()) InterprocessSynchronizationData();

    dataParent->serverSetup = false;
    dataParent->clientDone = false;

    if (fork())
    {
        // parent process, server
        std::unique_ptr<MOQTServer> moqtServer = server_setup();

        auto dm = moqtServer->dataManager_;
        auto trackHandle = dm->add_track_identifier({}, "track");
        auto groupHandle = trackHandle.lock()->add_group(GroupId(0));
        auto subgroupHandle = groupHandle.lock()->add_subgroup(ObjectId(1));
        subgroupHandle.add_object(messagePayload);

        {
            std::unique_lock lock(dataParent->mutex);
            dataParent->serverSetup = true;
        }

        for (;;)
        {
            std::unique_lock lock(dataParent->mutex);
            if (dataParent->clientDone)
                break;
        }

        std::cout << "Server done" << std::endl;

        // TODO: goes into infinite loop because destructor subscription calls join on threads which run an infinite while loop
        wait(NULL);
        exit(0);
    }
    else
    // child process
    {
        // Open shared memory
        bip::shared_memory_object shmChild(bip::open_only, sharedMemoryName.c_str(),
                                           bip::read_write);
        bip::mapped_region regionChildl(shmChild, bip::read_write);
        InterprocessSynchronizationData* dataChild =
        static_cast<InterprocessSynchronizationData*>(regionChildl.get_address());

        for (;;)
        {
            std::unique_lock lock(dataChild->mutex);
            if (dataChild->serverSetup)
                break;
        }


        std::unique_ptr<MOQTClient> moqtClient = client_setup();

        SubscriptionBuilder subscriptionBuilder;
        subscriptionBuilder.set_track_alias(TrackAlias(0));
        subscriptionBuilder.set_track_namespace({});
        subscriptionBuilder.set_track_name("track");
        subscriptionBuilder.set_data_range(SubscriptionBuilder::Filter::absoluteRange,
                                           { GroupId(0), ObjectId(0) },
                                           { GroupId(0), ObjectId(1) });

        subscriptionBuilder.set_subscriber_priority(0);
        subscriptionBuilder.set_group_order(0);

        auto subMessage = subscriptionBuilder.build();

        moqtClient->subscribe(std::move(subMessage));

        auto& dataStreams = moqtClient->dataStreamUserHandles_;
        auto dataStreamUserHandle = dataStreams.wait_dequeue_ret();

        auto& objectQueue = dataStreamUserHandle.objectQueue_;

        auto streamHeaderSubgroupObject = objectQueue->wait_dequeue_ret();

        try
        {
            utils::ASSERT_LOG_THROW(streamHeaderSubgroupObject.payload_ == messagePayload,
                                    "Payload mismatch",
                                    "Received: ", streamHeaderSubgroupObject.payload_,
                                    "Expected: ", messagePayload);
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            exit(1);
        }

        dataChild->clientDone = true;
        std::cout << "Client done" << std::endl;
        exit(0);
    }
}
