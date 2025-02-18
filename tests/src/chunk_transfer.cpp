/////////////////////////////////////////////////////////
#include <memory>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
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
#include "data_manager.hpp"
#include "moqt_client.hpp"
#include "strong_types.hpp"
#include "test_utilities.hpp"
/////////////////////////////////////////////////////////

using namespace rvn;

struct InterprocessSynchronizationData
{
    boost::interprocess::interprocess_mutex mutex;
    bool serverSetup;
    bool clientDone;
};

namespace bip = boost::interprocess;

// TODO: MsQuic issue for larget numObjects
// https://github.com/microsoft/msquic/discussions/4813
// We have solved the problem by making a copy of the buffer
static constexpr std::uint64_t numObjects = 10'000;

static constexpr std::uint8_t numGroups = 4;

int main()
{
    std::string sharedMemoryName = "chunk_transfer_test_";
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

        std::array groupHandles = {
            trackHandle.lock()->add_group(GroupId(0), PublisherPriority(3)), // high priorty means would be sent first
            trackHandle.lock()->add_group(GroupId(1), PublisherPriority(2)),
            trackHandle.lock()->add_group(GroupId(2), PublisherPriority(1)),
            trackHandle.lock()->add_group(GroupId(3), PublisherPriority(0))
        };

        {
            std::unique_lock lock(dataParent->mutex);
            dataParent->serverSetup = true;
        }

        std::array<std::jthread, numGroups> dataPublishers;
        for (std::uint8_t i = 0; i < numGroups; i++)
        {
            std::shared_ptr<GroupHandle> groupHandleSharedPtr = groupHandles[i].lock();
            dataPublishers[i] = std::jthread(
            [i, groupHandleSharedPtr]
            {
                std::optional<SubgroupHandle> subgroupHandleOpt =
                groupHandleSharedPtr->add_open_ended_subgroup();
                for (std::uint64_t j = 0; j < numObjects; j++)
                {
                    subgroupHandleOpt.value().add_object(
                    "Group" + std::to_string(i) + "_Object" + std::to_string(j));

                    subgroupHandleOpt.emplace(*subgroupHandleOpt->cap_and_next());
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });
        }

        for (;;)
        {
            std::unique_lock lock(dataParent->mutex);
            if (dataParent->clientDone)
                break;
        }

        std::cout << "Server done" << std::endl;
        wait(NULL);
        exit(0);
    }
    else
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

        NetemRAII netemRAII; // RAII

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

        while(true)
        {
            auto dataStreamUserHandle = dataStreams.wait_dequeue_ret();
            streamConsumerThreads.emplace_back(
            [](MOQTClient::DataStreamUserHandle&& dataStreamUserHandle)
            {
                auto& objectQueue = dataStreamUserHandle.objectQueue_;

                for (;;)
                {
                    auto streamHeaderSubgroupObject = objectQueue->wait_dequeue_ret();
                    if (streamHeaderSubgroupObject.objectId_ == ObjectId(numObjects - 1))
                        break;
                }
            },
            std::move(dataStreamUserHandle));
        }

        for (auto& thread : streamConsumerThreads)
            thread.join();

        {
            std::unique_lock lock(dataChild->mutex);
            dataChild->clientDone = true;
        }
        std::cout << "Client done" << std::endl;
    }
}
