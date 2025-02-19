/////////////////////////////////////////////////////////
#include <execution>
#include <memory>
#include <sys/types.h>
#include <sys/wait.h>
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
#include "../test_utilities.hpp"
#include "moqt_client.hpp"
#include "strong_types.hpp"
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

        std::for_each(std::execution::par, groupHandles.begin(), groupHandles.end(),
                      [](std::weak_ptr<GroupHandle>& groupHandle)
                      {
                          std::uint64_t groupId =
                          groupHandle.lock()->groupIdentifier_.groupId_;

                          auto groupHandleSharedPtr = groupHandle.lock();
                          for (std::size_t idx = 0; idx < numObjects; ++idx)
                          {
                              std::string object = "Group_" + std::to_string(groupId) +
                                                   "_" + "ID_" + std::to_string(idx);
                              groupHandleSharedPtr->add_subgroup(1).add_object(
                              std::move(object));
                          }
                      });
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

        for (std::uint8_t i = 0; i < numGroups; i++)
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
