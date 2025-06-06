/////////////////////////////////////////////////////////
#include <atomic>
#include <cstring>
#include <memory>
#include <msquic.h>
#include <mutex>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
/////////////////////////////////////////////////////////
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/program_options.hpp>
/////////////////////////////////////////////////////////
#include <callbacks.hpp>
#include <contexts.hpp>
#include <moqt.hpp>
#include <subscription_builder.hpp>
#include <utilities.hpp>
/////////////////////////////////////////////////////////
#include "../test_utilities.hpp"
#include "moqt_client.hpp"
#include "serialization/messages.hpp"
#include "strong_types.hpp"
/////////////////////////////////////////////////////////
#include "../object_generator_builder.hpp"
#include <chunk_transfer_perf_lttng.h>
/////////////////////////////////////////////////////////

using namespace rvn;

////////////////////////////////////////////////////////////////////
struct InterprocessSynchronizationData
{
    boost::interprocess::interprocess_mutex mutex_;
    bool serverSetup_;
    bool clientDone_;
    std::uint64_t processorIdBegin_;
};
////////////////////////////////////////////////////////////////////
namespace bip = boost::interprocess;
namespace po = boost::program_options;
////////////////////////////////////////////////////////////////////
constexpr std::uint16_t numMsQuicWorkersPerServer = 1;
constexpr std::uint16_t numMsQuicWorkersPerClient = 1;
std::uint16_t numProcessors = std::thread::hardware_concurrency();
////////////////////////////////////////////////////////////////////
std::uint64_t numObjects;
std::uint64_t msBetweenObjects;
constexpr std::uint8_t numLayers = 5;
constexpr ObjectGeneratorFactory::LayerGranularity layerGranularity =
ObjectGeneratorFactory::TrackGranularity;
////////////////////////////////////////////////////////////////////


int main(int argc, char* argv[])
{
    po::options_description poptions("Program Options");

    // clang-format off
    poptions.add_options()
        ("help,h", "help")
        ("objects,o", po::value<std::uint64_t>()->default_value(1'000), "Number of objects")
        ("loss_percentage,l", po::value<double>()->default_value(5), "Packet loss percentage")
        ("tc_bandwidth,t", po::value<double>()->default_value(8192), "NetEm bandwidth limitation")
        ("base_bit_rate,b", po::value<double>()->default_value(1024), "Bit rate in kbits per second of Base layer")
        ("delay_ms,d", po::value<double>()->default_value(50), "Network delay in milliseconds")
        ("delay_jitter,j", po::value<double>()->default_value(10), "Network delay jitter in milliseconds")
        ("sample_time,s", po::value<std::uint64_t>()->default_value(100), "Milliseconds between objects");
    // clang-format on

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, poptions), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << poptions << std::endl;
        exit(0);
    }

    //////////////////////////////////////////////////////////////////////////
    // Setting Command Link arguments
    numObjects = vm["objects"].as<std::uint64_t>();
    msBetweenObjects = vm["sample_time"].as<std::uint64_t>();
    //////////////////////////////////////////////////////////////////////////

    std::string sharedMemoryName = "chunk_transfer_test_";
    sharedMemoryName += std::to_string(getpid());

    bip::shared_memory_object shmParent(bip::create_only,
                                        sharedMemoryName.c_str(), bip::read_write);
    shmParent.truncate(sizeof(InterprocessSynchronizationData));
    bip::mapped_region regionParent(shmParent, bip::read_write);
    InterprocessSynchronizationData* dataParent =
    new (regionParent.get_address()) InterprocessSynchronizationData();

    dataParent->serverSetup_ = false;
    dataParent->clientDone_ = false;
    dataParent->processorIdBegin_ = numMsQuicWorkersPerServer;

    if (fork())
    {
        // parent process, server
        std::uint8_t rawServerConfig[sizeof(QUIC_GLOBAL_EXECUTION_CONFIG) +
                                     numMsQuicWorkersPerServer * sizeof(std::uint16_t)];
        QUIC_GLOBAL_EXECUTION_CONFIG* serverConfig =
        reinterpret_cast<QUIC_GLOBAL_EXECUTION_CONFIG*>(rawServerConfig);
        serverConfig->ProcessorCount = numMsQuicWorkersPerServer;
        serverConfig->Flags = QUIC_GLOBAL_EXECUTION_CONFIG_FLAG_NONE;
        serverConfig->PollingIdleTimeoutUs = 50000;

        if (numProcessors < numMsQuicWorkersPerServer)
        {
            std::cerr
            << "Number of processors is less than the number of server workers"
            << std::endl;
            exit(1);
        }

        for (std::uint16_t i = 0; i < numMsQuicWorkersPerServer; i++)
            serverConfig->ProcessorList[i] = i;

        std::string setNicenessString = "renice -20 -p " + std::to_string(getpid());
        // std::system(setNicenessString.c_str());

        std::unique_ptr<MOQTServer> moqtServer =
        server_setup(std::make_tuple(serverConfig, sizeof(rawServerConfig)));

        auto dm = moqtServer->dataManager_;

        ObjectGeneratorFactory objectGeneratorFactory(*dm);
        auto dataPublishers =
        objectGeneratorFactory.create(layerGranularity, numLayers, numObjects,
                                      std::chrono::milliseconds(msBetweenObjects),
                                      vm["base_bit_rate"].as<double>() * 1024.);

        {
            std::unique_lock lock(dataParent->mutex_);
            dataParent->serverSetup_ = true;
        }
        for (auto& dataPublisher : dataPublishers)
            dataPublisher.join();

        for (;;)
        {
            std::unique_lock lock(dataParent->mutex_);
            if (dataParent->clientDone_)
                break;
        }

        std::cout << "Server done" << std::endl;
        wait(NULL);
        exit(0);
    }
    else
    {
        //////////////////////////////////////////////////////////////////////////
        // Setting up NetEm parameters
        double lossPercentage = vm["loss_percentage"].as<double>();
        double tcBandwidth = vm["tc_bandwidth"].as<double>();
        double delayMs = vm["delay_ms"].as<double>();
        double delayJitter = vm["delay_jitter"].as<double>();

        // NetemRAII netemRAII(lossPercentage, tcBandwidth, delayMs, delayJitter);
        lttng_ust_tracepoint(chunk_transfer_perf_lttng, netem, lossPercentage,
                             tcBandwidth, delayMs, delayJitter);
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // Open shared memory
        bip::shared_memory_object shmChild(bip::open_only, sharedMemoryName.c_str(),
                                           bip::read_write);
        bip::mapped_region regionChild(shmChild, bip::read_write);
        InterprocessSynchronizationData* dataChild =
        static_cast<InterprocessSynchronizationData*>(regionChild.get_address());
        //////////////////////////////////////////////////////////////////////////

        for (;;)
        {
            std::unique_lock lock(dataChild->mutex_);
            if (dataChild->serverSetup_)
                break;
        }

        std::uint16_t clientFirstProcessorId;
        {
            std::unique_lock lock(dataParent->mutex_);
            clientFirstProcessorId = dataParent->processorIdBegin_;
            dataParent->processorIdBegin_ += numMsQuicWorkersPerClient;
        }

        constexpr std::uint64_t execConfigLen =
        sizeof(QUIC_GLOBAL_EXECUTION_CONFIG) +
        numMsQuicWorkersPerClient * sizeof(std::uint16_t);

        std::uint8_t rawExecutionConfig[execConfigLen];
        QUIC_GLOBAL_EXECUTION_CONFIG* executionConfig =
        reinterpret_cast<QUIC_GLOBAL_EXECUTION_CONFIG*>(rawExecutionConfig);

        executionConfig->ProcessorCount = numMsQuicWorkersPerClient;
        executionConfig->Flags = QUIC_GLOBAL_EXECUTION_CONFIG_FLAG_NONE;
        executionConfig->PollingIdleTimeoutUs = 50000;

        /*
            We want to map the client workers to the processors after the server
           workers so i = clientFirstProcessorId + j (j = 0, 1, 2, ...,
           numMsQuicWorkersPerClient - 1) is mapped to
           [numMsQuicWorkersPerServer, numCores)
        */
        for (std::uint16_t workerId = 0; workerId < numMsQuicWorkersPerClient; workerId++)
        {
            std::uint16_t processorId = clientFirstProcessorId + workerId;
            // processorId should be mapped to [numMsQuicWorkersPerServer, numCores)
            processorId %= (numProcessors - numMsQuicWorkersPerServer);
            processorId += numMsQuicWorkersPerServer;

            executionConfig->ProcessorList[workerId] = processorId;
        }

        std::unique_ptr<MOQTClient> moqtClient =
        client_setup(std::make_tuple(executionConfig, execConfigLen));

        SubscriptionBuilder subscriptionBuilder;
        subscriptionBuilder.set_track_alias(TrackAlias(0));
        subscriptionBuilder.set_track_namespace({});
        subscriptionBuilder.set_track_name("track");
        subscriptionBuilder.set_data_range(SubscriptionBuilder::Filter::latestObject);
        subscriptionBuilder.set_subscriber_priority(0);
        subscriptionBuilder.set_group_order(0);
        auto subMessage = subscriptionBuilder.build();

        BatchSubscribeMessage batchSubscribeMessage;
        batchSubscribeMessage.trackNamespacePrefix_ = { "namespace1", "namespace2", "namespace3" };

        for (std::uint64_t layerId = 0; layerId < numLayers; ++layerId)
        {
            subMessage.trackName_ = std::to_string(layerId);
            subMessage.trackAlias_ = TrackAlias(layerId);
            if (layerId != 0)
                subMessage.parameters_.emplace_back(DeliveryTimeoutParameter{
                std::chrono::milliseconds{ msBetweenObjects } });
            batchSubscribeMessage.subscriptions_.push_back(std::move(subMessage));
        }

        moqtClient->subscribe(std::move(batchSubscribeMessage));
        std::atomic_int8_t numEndObjectsReceived = 0;

        auto& receivedObjectsQueue = moqtClient->receivedObjects_;
        pid_t thisClientPid = getpid();
        while (true)
        {
            if (numEndObjectsReceived == numLayers)
                break;

            auto enrichedObject = receivedObjectsQueue.wait_dequeue_ret();

            if (enrichedObject.object_.objectId_ == numObjects - 1)
                numEndObjectsReceived++;

            std::uint64_t currTimestamp = get_current_ms_timestamp();
            std::uint64_t* sentTimestamp =
            reinterpret_cast<std::uint64_t*>(enrichedObject.object_.payload_.data());
            std::uint64_t groupId = enrichedObject.header_->groupId_;
            std::uint64_t objectId = enrichedObject.object_.objectId_;

            std::cerr << currTimestamp - *sentTimestamp << " "
                      << "Track Alias: " << enrichedObject.header_->trackAlias_
                      << " " << "Group Id: " << groupId << " "
                      << "Object Id: " << objectId << '\n';

            lttng_ust_tracepoint(chunk_transfer_perf_lttng, object_recv, thisClientPid,
                                 currTimestamp - *sentTimestamp, groupId, objectId,
                                 enrichedObject.object_.payload_.size());
        }

        {
            std::unique_lock lock(dataChild->mutex_);
            dataChild->clientDone_ = true;
        }
        std::cout << "Client done" << std::endl;
        exit(0);
    }
}
