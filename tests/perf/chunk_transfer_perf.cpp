/////////////////////////////////////////////////////////
#include <atomic>
#include <dlfcn.h>
#include <memory>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
/////////////////////////////////////////////////////////
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
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

using namespace rvn;

struct InterprocessSynchronizationData
{
    boost::interprocess::interprocess_mutex mutex_;
    bool serverSetup_;
    bool clientDone_;
};

namespace bip = boost::interprocess;
namespace po = boost::program_options;

std::uint64_t numObjects = 1'000;
constexpr std::uint8_t numGroups = 4;

static long numMsQuicWorkers = 2;

long sysconf(int name)
{
    // Get the real sysconf
    static long (*real_sysconf)(int) = NULL;
    if (!real_sysconf)
    {
        real_sysconf =
        reinterpret_cast<long (*)(int)>(dlsym(RTLD_NEXT, "sysconf"));
    }

    if (name == _SC_NPROCESSORS_ONLN)
        return numMsQuicWorkers;
    // For everything else, call the real sysconf
    return real_sysconf(name);
}

std::uint64_t get_current_ms_timestamp()
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
    .count();
}


std::string generate_object(std::uint8_t groupId, std::uint64_t objectId)
{
    std::uint64_t currTime = get_current_ms_timestamp();
    std::string object(reinterpret_cast<const char*>(&currTime), sizeof(currTime));
    object.resize(4096);
    return object;
}


int main(int argc, char* argv[])
{
    po::options_description poptions("Program Options");

    // clang-format off
    poptions.add_options()
        ("help,h", "help")
        ("quic_threads,w", po::value<std::uint8_t>()->default_value(4), "Number of QUIC threads")
        ("objects,o", po::value<std::uint64_t>()->default_value(1'000), "Number of objects")
        ("loss_percentage,l", po::value<double>()->default_value(5), "Packet loss percentage")
        ("bit_rate,b", po::value<double>()->default_value(4096), "Bit rate in kbits per second")
        ("delay_ms,d", po::value<double>()->default_value(50), "Network delay in milliseconds")
        ("delay_jitter,j", po::value<double>()->default_value(10), "Network delay jitter in milliseconds");
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
    numMsQuicWorkers = vm["quic_threads"].as<std::uint8_t>();
    numObjects = vm["objects"].as<std::uint64_t>();
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

    if (fork())
    {
        // parent process, server
        std::unique_ptr<MOQTServer> moqtServer = server_setup();

        auto dm = moqtServer->dataManager_;
        auto trackHandle = dm->add_track_identifier({}, "track");

        std::array groupHandles = {
            trackHandle.lock()->add_group(GroupId(0), PublisherPriority(3)), // high priorty means would be sent first
            trackHandle.lock()->add_group(GroupId(1), PublisherPriority(2)),
            trackHandle.lock()->add_group(GroupId(2), PublisherPriority(1)),
            trackHandle.lock()->add_group(GroupId(3), PublisherPriority(0))
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

                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
        double lossPercentage = vm["loss_percentage"].as<double>();
        double bitRate = vm["bit_rate"].as<double>();
        double delayMs = vm["delay_ms"].as<double>();
        double delayJitter = vm["delay_jitter"].as<double>();

        NetemRAII netemRAII(lossPercentage, bitRate, delayMs, delayJitter);

        // Open shared memory
        bip::shared_memory_object shmChild(bip::open_only, sharedMemoryName.c_str(),
                                           bip::read_write);
        bip::mapped_region regionChildl(shmChild, bip::read_write);
        InterprocessSynchronizationData* dataChild =
        static_cast<InterprocessSynchronizationData*>(regionChildl.get_address());


        for (;;)
        {
            std::unique_lock lock(dataChild->mutex_);
            if (dataChild->serverSetup_)
                break;
        }


        std::unique_ptr<MOQTClient> moqtClient = client_setup();

        SubscriptionBuilder subscriptionBuilder;
        subscriptionBuilder.set_track_alias(TrackAlias(0));
        subscriptionBuilder.set_track_namespace({});
        subscriptionBuilder.set_track_name("track");
        subscriptionBuilder.set_data_range(SubscriptionBuilder::Filter::absoluteStart,
                                           { GroupId(0), ObjectId(0) });
        subscriptionBuilder.set_subscriber_priority(0);
        subscriptionBuilder.set_group_order(0);

        auto subMessage = subscriptionBuilder.build();

        moqtClient->subscribe(std::move(subMessage));

        auto& dataStreams = moqtClient->dataStreamUserHandles_;

        std::vector<std::thread> streamConsumerThreads;
        std::atomic_int8_t numEndObjectsReceived = 0;
        while (true)
        {
            if (numEndObjectsReceived == numGroups)
                break;

            rvn::MOQTClient::DataStreamUserHandle dataStreamUserHandle;
            if (!dataStreams.try_dequeue(dataStreamUserHandle))
                continue;

            streamConsumerThreads.emplace_back(
            [&numEndObjectsReceived](MOQTClient::DataStreamUserHandle&& dataStreamUserHandle)
            {
                auto& objectQueue = dataStreamUserHandle.objectQueue_;

                for (;;)
                {
                    auto streamHeaderSubgroupObject = objectQueue->wait_dequeue_ret();
                    std::uint64_t currTimestamp = get_current_ms_timestamp();
                    std::uint64_t* sentTimestamp = reinterpret_cast<std::uint64_t*>(
                    streamHeaderSubgroupObject.payload_.data());

                    std::cout << currTimestamp - *sentTimestamp << std::endl;

                    if (streamHeaderSubgroupObject.objectId_ == numObjects - 1)
                        numEndObjectsReceived++;

                    if (auto ptr = dataStreamUserHandle.streamLifeTimeFlag_.lock(); ptr == nullptr)
                    {
                        std::cout << ptr << std::endl;
                        break;
                    }
                }
            },
            std::move(dataStreamUserHandle));

            streamConsumerThreads.back().detach();
        }

        {
            std::unique_lock lock(dataChild->mutex_);
            dataChild->clientDone_ = true;
        }
        std::cout << "Client done" << std::endl;
        exit(0);
    }
}