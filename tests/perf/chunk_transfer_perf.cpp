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
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
/////////////////////////////////////////////////////////
#include <callbacks.hpp>
#include <contexts.hpp>
#include <moqt.hpp>
#include <subscription_builder.hpp>
#include <utilities.hpp>
/////////////////////////////////////////////////////////
#include "../test_utilities.hpp"
#include "data_manager.hpp"
#include "moqt_client.hpp"
#include "serialization/messages.hpp"
#include "strong_types.hpp"
/////////////////////////////////////////////////////////
#include "./chunk_transfer_perf_lttng.h"
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
constexpr std::uint16_t numMsQuicWorkersPerServer = 6;
constexpr std::uint16_t numMsQuicWorkersPerClient = 3;
std::uint16_t numProcessors = std::thread::hardware_concurrency();
////////////////////////////////////////////////////////////////////
std::uint64_t numObjects;
std::uint64_t msBetweenObjects;
constexpr std::uint8_t numGroups = 5;
////////////////////////////////////////////////////////////////////


std::string generate_object(std::uint64_t groupId, std::uint64_t objectId)
{
    std::uint64_t currTime = get_current_ms_timestamp();
    std::string object((1 << 15) * (1 << groupId), 0);
    std::memcpy(object.data(), reinterpret_cast<const char*>(&currTime), sizeof(currTime));
    std::memcpy(object.data() + sizeof(std::uint64_t),
                reinterpret_cast<const char*>(&groupId), sizeof(groupId));
    std::memcpy(object.data() + 2 * sizeof(std::uint64_t),
                reinterpret_cast<const char*>(&objectId), sizeof(objectId));

    return object;
}


int main(int argc, char* argv[])
{
    po::options_description poptions("Program Options");

    // clang-format off
    poptions.add_options()
        ("help,h", "help")
        ("objects,o", po::value<std::uint64_t>()->default_value(1'000), "Number of objects")
        ("loss_percentage,l", po::value<double>()->default_value(5), "Packet loss percentage")
        ("bit_rate,b", po::value<double>()->default_value(4096), "Bit rate in kbits per second")
        ("delay_ms,d", po::value<double>()->default_value(50), "Network delay in milliseconds")
        ("delay_jitter,j", po::value<double>()->default_value(10), "Network delay jitter in milliseconds")
        ("sample_time,s", po::value<std::uint64_t>()->default_value(250), "Milliseconds between objects");
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
        std::uint8_t rawServerConfig[sizeof(QUIC_EXECUTION_CONFIG) +
                                     numMsQuicWorkersPerServer * sizeof(std::uint16_t)];
        QUIC_EXECUTION_CONFIG* serverConfig =
        reinterpret_cast<QUIC_EXECUTION_CONFIG*>(rawServerConfig);
        serverConfig->ProcessorCount = numMsQuicWorkersPerServer;
        serverConfig->Flags = QUIC_EXECUTION_CONFIG_FLAG_NONE;
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
        std::system(setNicenessString.c_str());

        std::unique_ptr<MOQTServer> moqtServer =
        server_setup(std::make_tuple(serverConfig, sizeof(rawServerConfig)));

        auto dm = moqtServer->dataManager_;
        auto trackHandle = dm->add_track_identifier({}, "track");

        std::array groupHandles = {
            trackHandle.lock()->add_group(GroupId(0), PublisherPriority(4)), // high priorty means would be sent first
            trackHandle.lock()->add_group(GroupId(1), PublisherPriority(3)),
            trackHandle.lock()->add_group(GroupId(2), PublisherPriority(2)),
            trackHandle.lock()->add_group(GroupId(3), PublisherPriority(1)),
            trackHandle.lock()->add_group(GroupId(4), PublisherPriority(0))
        };

        {
            std::unique_lock lock(dataParent->mutex_);
            dataParent->serverSetup_ = true;
        }

        std::array<std::jthread, numGroups> dataPublishers;
        for (std::uint8_t groupId = 0; groupId < numGroups; groupId++)
        {
            std::shared_ptr<GroupHandle> groupHandleSharedPtr =
            groupHandles[groupId].lock();
            dataPublishers[groupId] = std::jthread(
            [groupId, groupHandleSharedPtr]
            {
                std::optional<SubgroupHandle> subgroupHandleOpt =
                groupHandleSharedPtr->add_open_ended_subgroup();
                for (std::uint64_t objectId = 0; objectId < numObjects; objectId++)
                {
                    std::string object = generate_object(groupId, objectId);
                    subgroupHandleOpt.value().add_object(std::move(object));
                    subgroupHandleOpt.emplace(*subgroupHandleOpt->cap_and_next());
                    std::this_thread::sleep_for(std::chrono::milliseconds(msBetweenObjects));
                }
                subgroupHandleOpt->cap();
            });
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
        double bitRate = vm["bit_rate"].as<double>();
        double delayMs = vm["delay_ms"].as<double>();
        double delayJitter = vm["delay_jitter"].as<double>();

        NetemRAII netemRAII(lossPercentage, bitRate, delayMs, delayJitter);
        lttng_ust_tracepoint(chunk_transfer_perf_lttng, netem, lossPercentage,
                             bitRate, delayMs, delayJitter);
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // Open shared memory
        bip::shared_memory_object shmChild(bip::open_only, sharedMemoryName.c_str(),
                                           bip::read_write);
        bip::mapped_region regionChild(shmChild, bip::read_write);
        InterprocessSynchronizationData* dataChild =
        static_cast<InterprocessSynchronizationData*>(regionChild.get_address());
        //////////////////////////////////////////////////////////////////////////
        std::string setNicenessString = "renice -20 -p " + std::to_string(getpid());
        std::system(setNicenessString.c_str());

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
        sizeof(QUIC_EXECUTION_CONFIG) + numMsQuicWorkersPerClient * sizeof(std::uint16_t);

        std::uint8_t rawExecutionConfig[execConfigLen];
        QUIC_EXECUTION_CONFIG* executionConfig =
        reinterpret_cast<QUIC_EXECUTION_CONFIG*>(rawExecutionConfig);

        executionConfig->ProcessorCount = numMsQuicWorkersPerClient;
        executionConfig->Flags = QUIC_EXECUTION_CONFIG_FLAG_NONE;
        executionConfig->PollingIdleTimeoutUs = 50000;


        /*
            We want to map the client workers to the processors after the server workers
            so i = clientFirstProcessorId + j (j = 0, 1, 2, ..., numMsQuicWorkersPerClient - 1)
            is mapped to [numMsQuicWorkersPerServer, numCores)
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
        subscriptionBuilder.set_data_range(SubscriptionBuilder::Filter::latestPerGroupInTrack);
        subscriptionBuilder.set_subscriber_priority(0);
        subscriptionBuilder.set_group_order(0);

        auto subMessage = subscriptionBuilder.build();

        moqtClient->subscribe(std::move(subMessage));
        std::atomic_int8_t numEndObjectsReceived = 0;

        auto& receivedObjectsQueue = moqtClient->receivedObjects_;
        pid_t thisClientPid = getpid();
        while (true)
        {
            if (numEndObjectsReceived == numGroups)
                break;

            auto enrichedObject = receivedObjectsQueue.wait_dequeue_ret();

            if (enrichedObject.object_.objectId_ == numObjects - 1)
                numEndObjectsReceived++;

            std::uint64_t currTimestamp = get_current_ms_timestamp();
            std::uint64_t* sentTimestamp =
            reinterpret_cast<std::uint64_t*>(enrichedObject.object_.payload_.data());
            std::uint64_t* groupId = sentTimestamp + 1;
            std::uint64_t* objectId = groupId + 1;

            std::cout << currTimestamp - *sentTimestamp << " " << *groupId
                      << " " << *objectId << '\n';

            lttng_ust_tracepoint(chunk_transfer_perf_lttng, object_recv, thisClientPid,
                                 currTimestamp - *sentTimestamp, *groupId, *objectId,
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
