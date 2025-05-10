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
#include "data_manager.hpp"
#include "moqt_client.hpp"
#include "serialization/messages.hpp"
#include "strong_types.hpp"
/////////////////////////////////////////////////////////
#include "../object_generator_builder.hpp"
#include <chunk_transfer_perf_lttng.h>
/////////////////////////////////////////////////////////

using namespace rvn;
std::uint8_t numNodes;
constexpr std::uint16_t numMsQuicWorkersPerNode = 1;
constexpr std::uint8_t numLayers = 5;
constexpr std::uint16_t numObjects = 1'000;
// publisher runs on port 5000, relay 1 on 5001, relay 2 on 5002...
constexpr std::uint16_t basePort = 5000;
std::uint16_t numProcessors = std::thread::hardware_concurrency();
constexpr ObjectGeneratorFactory::LayerGranularity layerGranularity =
ObjectGeneratorFactory::TrackGranularity;
constexpr std::uint64_t msBetweenObjects = 250;
////////////////////////////////////////////////////////////////////
struct InterprocessSynchronizationData
{
    boost::interprocess::interprocess_mutex mutex_{};
    // we do not care if client is setup
    std::vector<int> nodeSetup_{};
    bool clientDone_{};
    std::uint64_t processorIdBegin_{};

    InterprocessSynchronizationData(std::uint8_t numNodes)
    : nodeSetup_(numNodes, false)
    {
    }
};
////////////////////////////////////////////////////////////////////
namespace bip = boost::interprocess;
namespace po = boost::program_options;
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
        ("sample_time,s", po::value<std::uint64_t>()->default_value(250), "Milliseconds between objects")
        ("num_nodes,s", po::value<std::uint8_t>()->default_value(2), "Number of nodes");
    // clang-format on

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, poptions), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << poptions << std::endl;
        exit(0);
    }

    ////////////////////////////////////////////////////////////
    // assigning the program options
    numNodes = vm["num_nodes"].as<std::uint8_t>();
    ////////////////////////////////////////////////////////////

    std::string sharedMemoryName = "chunk_transfer_test_";
    sharedMemoryName += std::to_string(getpid());

    bip::shared_memory_object shmParent(bip::create_only,
                                        sharedMemoryName.c_str(), bip::read_write);
    shmParent.truncate(sizeof(InterprocessSynchronizationData));
    bip::mapped_region regionParent(shmParent, bip::read_write);
    InterprocessSynchronizationData* dataParent =
    new (regionParent.get_address()) InterprocessSynchronizationData(numNodes);

    ////////////////////////////////////////////////////////////////////////////////
    // Apply NetEm rules
    // reset any existing rules
    std::system("tc qdisc del dev lo root 2> /dev/null || true");

    // create qdisc (queueing discipline) with different queue for each node
    std::system(std::format("tc qdisc add dev lo root handle 1: prio bands {}", numNodes - 1)
                .c_str());

    // netem rule for relay nodes
    {
        std::uint16_t nodeId = 0;
        for (; nodeId < numNodes - 1; nodeId++)
        {
            if (nodeId == numNodes - 2)
                // emulation for the last link (last relay and the subscriber)
                std::system(
                std::format("tc qdisc add dev lo parent 1:{} handle "
                            "{}: netem loss {}% delay {}ms {}ms rate {}kbit",
                            nodeId, nodeId, 2 /* loss percentage*/, 20, 2, // delay jitter
                            16 /* bandwidth in kbit */)
                .c_str());
            else
                std::system(
                std::format("tc qdisc add dev lo parent 1:{} handle "
                            "{}: netem loss {}% delay {}ms {}ms",
                            nodeId, nodeId, 0 /* loss percentage*/, 50 /* delay in ms */,
                            5 /* delay jitter */) // No bandwidth emulation
                .c_str());

            std::system(std::format("tc filter add dev lo parent 1: protocol "
                                    "ip prio {} handle {} fw classid 1:{}",
                                    nodeId, nodeId, nodeId)
                        .c_str());
        }


        /*
            We have setup different queuing disciplines for each tag,
            now we need to use iptables to tag each packet
        */

        // https://chatgpt.com/c/681bd070-602c-8013-93dd-86c4dbc4169e
    }
    ////////////////////////////////////////////////////////////////////////////////

    if (fork())
    {
        // parent process, data publisher
        // parent process, server
        std::uint8_t rawServerConfig[sizeof(QUIC_EXECUTION_CONFIG) +
                                     numMsQuicWorkersPerNode * sizeof(std::uint16_t)];
        QUIC_EXECUTION_CONFIG* serverConfig =
        reinterpret_cast<QUIC_EXECUTION_CONFIG*>(rawServerConfig);
        serverConfig->ProcessorCount = numMsQuicWorkersPerNode;
        serverConfig->Flags = QUIC_EXECUTION_CONFIG_FLAG_NONE;
        serverConfig->PollingIdleTimeoutUs = 50000;

        if (numProcessors < numMsQuicWorkersPerNode)
        {
            std::cerr
            << "Number of processors is less than the number of server workers"
            << std::endl;
            exit(1);
        }

        for (std::uint16_t i = 0; i < numMsQuicWorkersPerNode; i++)
            serverConfig->ProcessorList[i] = i;

        std::unique_ptr<MOQTServer> moqtServer =
        server_setup(std::make_tuple(serverConfig, sizeof(rawServerConfig)), basePort);

        auto dm = moqtServer->dataManager_;

        ObjectGeneratorFactory objectGeneratorFactory(*dm);
        auto dataPublishers =
        objectGeneratorFactory.create(layerGranularity, numLayers, numObjects,
                                      std::chrono::milliseconds(msBetweenObjects),
                                      vm["base_bit_rate"].as<double>() * 1024.);

        {
            std::unique_lock lock(dataParent->mutex_);
            dataParent->nodeSetup_[0] = true;
            std::cout << "Node Idx: 0 setup" << std::endl;
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
        // child 1 process
        for (int nodeIdx = 1; nodeIdx < numNodes - 1; nodeIdx++)
        {
            if (fork())
            {
                // child 1 process
                // continues on its merry way and finally runs subscriber after all relays are up
            }
            else
            {
                // child nodeIdx process
                // runs the relay node

                //////////////////////////////////////////////////////////////////////////
                // Open shared memory
                bip::shared_memory_object shmChild(bip::open_only,
                                                   sharedMemoryName.c_str(), bip::read_write);
                bip::mapped_region regionChild(shmChild, bip::read_write);
                InterprocessSynchronizationData* dataChild =
                static_cast<InterprocessSynchronizationData*>(regionChild.get_address());
                //////////////////////////////////////////////////////////////////////////

                for (;;)
                {
                    std::unique_lock lock(dataChild->mutex_);
                    // wait for previous node to be setup
                    if (dataChild->nodeSetup_[nodeIdx - 1])
                        break;
                }

                // setup subscriber to be able to read data from previous node

                std::uint16_t clientFirstProcessorId;
                {
                    std::unique_lock lock(dataParent->mutex_);
                    clientFirstProcessorId = dataParent->processorIdBegin_;
                    dataParent->processorIdBegin_ += numMsQuicWorkersPerNode;
                }

                constexpr std::uint64_t execConfigLen =
                sizeof(QUIC_EXECUTION_CONFIG) +
                numMsQuicWorkersPerNode * sizeof(std::uint16_t);

                std::uint8_t rawRelaySubscriberExecutionConfig[execConfigLen];
                QUIC_EXECUTION_CONFIG* relaySubscriberExecutionConfig =
                reinterpret_cast<QUIC_EXECUTION_CONFIG*>(rawRelaySubscriberExecutionConfig);

                relaySubscriberExecutionConfig->ProcessorCount = numMsQuicWorkersPerNode;
                relaySubscriberExecutionConfig->Flags = QUIC_EXECUTION_CONFIG_FLAG_NONE;
                relaySubscriberExecutionConfig->PollingIdleTimeoutUs = 50000;

                for (std::uint16_t workerId = 0; workerId < numMsQuicWorkersPerNode; workerId++)
                {
                    std::uint16_t processorId = clientFirstProcessorId + workerId;
                    // processorId should be mapped to [numMsQuicWorkersPerNode, numCores)
                    processorId %= (numProcessors - numMsQuicWorkersPerNode);
                    processorId += numMsQuicWorkersPerNode;

                    relaySubscriberExecutionConfig->ProcessorList[workerId] = processorId;
                }

                std::unique_ptr<MOQTClient> moqtClient =
                client_setup(std::make_tuple(relaySubscriberExecutionConfig, execConfigLen),
                             "127.0.0.1",
                             basePort + nodeIdx - 1 /* we are requesting the previous node  */);

                // TODO: change to fetch
                SubscriptionBuilder subscriptionBuilder;
                subscriptionBuilder.set_track_alias(TrackAlias(0));
                subscriptionBuilder.set_track_namespace({});
                subscriptionBuilder.set_track_name("track");
                subscriptionBuilder.set_data_range(SubscriptionBuilder::Filter::latestObject);
                subscriptionBuilder.set_subscriber_priority(0);
                subscriptionBuilder.set_group_order(0);

                auto subMessage = subscriptionBuilder.build();

                BatchSubscribeMessage batchSubscribeMessage;
                batchSubscribeMessage.trackNamespacePrefix_ = { "namespace1", "namespace2",
                                                                "namespace3" };

                for (std::uint64_t layerId = 0; layerId < numLayers; ++layerId)
                {
                    subMessage.trackName_ = std::to_string(layerId);
                    subMessage.trackAlias_ = TrackAlias(layerId);
                    batchSubscribeMessage.subscriptions_.push_back(std::move(subMessage));
                }

                moqtClient->subscribe(std::move(batchSubscribeMessage));
                std::atomic_int8_t numEndObjectsReceived = 0;

                auto& receivedObjectsQueue = moqtClient->receivedObjects_;
                pid_t thisClientPid = getpid();

                // relay-publisher setup

                std::uint8_t rawRelayPublisherConfig[sizeof(QUIC_EXECUTION_CONFIG) +
                                                     numMsQuicWorkersPerNode * sizeof(std::uint16_t)];
                QUIC_EXECUTION_CONFIG* relayPublisherConfig =
                reinterpret_cast<QUIC_EXECUTION_CONFIG*>(rawRelayPublisherConfig);
                relayPublisherConfig->ProcessorCount = numMsQuicWorkersPerNode;
                relayPublisherConfig->Flags = QUIC_EXECUTION_CONFIG_FLAG_NONE;
                relayPublisherConfig->PollingIdleTimeoutUs = 50000;

                if (numProcessors < numMsQuicWorkersPerNode)
                {
                    std::cerr << "Number of processors is less than the number "
                                 "of server workers"
                              << std::endl;
                    exit(1);
                }

                for (std::uint16_t i = 0; i < numMsQuicWorkersPerNode; i++)
                    relayPublisherConfig->ProcessorList[i] = i;

                std::unique_ptr<MOQTServer> moqtServer =
                server_setup(std::make_tuple(relayPublisherConfig, sizeof(rawRelayPublisherConfig)),
                             basePort + nodeIdx /* we are the relay publisher node */);

                {
                    std::unique_lock lock(dataParent->mutex_);
                    dataParent->nodeSetup_[nodeIdx] = true;
                    std::cout << "Node Idx: " << nodeIdx << " setup" << std::endl;
                }

                auto dm = moqtServer->dataManager_;
                std::array trackHandles = {
                    // clang-format off
                dm->add_track_identifier({ "namespace1", "namespace2", "namespace3" }, "0", PublisherPriority(-1), std::nullopt).lock(),
                dm->add_track_identifier({ "namespace1", "namespace2", "namespace3" }, "1", PublisherPriority(3), std::nullopt).lock(),
                dm->add_track_identifier({ "namespace1", "namespace2", "namespace3" }, "2", PublisherPriority(2), std::nullopt).lock(),
                dm->add_track_identifier({ "namespace1", "namespace2", "namespace3" }, "3", PublisherPriority(1), std::nullopt).lock(),
                dm->add_track_identifier({ "namespace1", "namespace2", "namespace3" }, "4", PublisherPriority(0), std::nullopt).lock()
                    // clang-format on
                };

                const auto objectReceiver = [&]()
                {
                    while (true)
                    {
                        if (numEndObjectsReceived == numLayers)
                            break;

                        auto enrichedObject = receivedObjectsQueue.wait_dequeue_ret();

                        if (enrichedObject.object_.objectId_ == numObjects - 1)
                            numEndObjectsReceived++;

                        std::uint64_t currTimestamp = get_current_ms_timestamp();
                        std::uint64_t* sentTimestamp = reinterpret_cast<std::uint64_t*>(
                        enrichedObject.object_.payload_.data());
                        std::uint64_t groupId = enrichedObject.header_->groupId_;
                        std::uint64_t objectId = enrichedObject.object_.objectId_;
                        TrackAlias trackAlias = enrichedObject.header_->trackAlias_;

                        std::cout
                        << getpid() << " " << currTimestamp - *sentTimestamp << " "
                        << "Track Alias: " << enrichedObject.header_->trackAlias_
                        << " " << "Group Id: " << groupId << " "
                        << "Object Id: " << objectId << '\n';

                        ObjectIdentifier oid{ moqtClient->connectionState
                                              ->alias_to_identifier(
                                              enrichedObject.header_->trackAlias_)
                                              .value(),
                                              GroupId(groupId), ObjectId(objectId) };

                        lttng_ust_tracepoint(chunk_transfer_perf_lttng, object_recv,
                                             thisClientPid, currTimestamp - *sentTimestamp,
                                             groupId, objectId,
                                             enrichedObject.object_.payload_.size());

                        trackHandles[trackAlias]
                        ->add_object(GroupId(groupId), ObjectId(objectId),
                                     enrichedObject.object_.payload_);
                    }
                };

                std::thread objectReceiverThread(objectReceiver);

                objectReceiverThread.join();

                for (;;)
                {
                    std::unique_lock lock(dataParent->mutex_);
                    // wait for subscriber to finish
                    if (dataParent->nodeSetup_[numNodes - 1])
                        break;
                }

                std::cout << "Node Idx: " << nodeIdx << " done" << std::endl;
                wait(NULL);
                exit(0);
            }
        }

        // child 1 process, running subscriber
        // TODO: netem parameters with ip tables

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
            // we are node: numLayers - 1, check if -2 is setup
            if (dataChild->nodeSetup_[numNodes - 2])
                break;
        }

        std::uint16_t clientFirstProcessorId;
        {
            std::unique_lock lock(dataParent->mutex_);
            clientFirstProcessorId = dataParent->processorIdBegin_;
            dataParent->processorIdBegin_ += numMsQuicWorkersPerNode;
        }

        constexpr std::uint64_t execConfigLen =
        sizeof(QUIC_EXECUTION_CONFIG) + numMsQuicWorkersPerNode * sizeof(std::uint16_t);

        std::uint8_t rawExecutionConfig[execConfigLen];
        QUIC_EXECUTION_CONFIG* executionConfig =
        reinterpret_cast<QUIC_EXECUTION_CONFIG*>(rawExecutionConfig);

        executionConfig->ProcessorCount = numMsQuicWorkersPerNode;
        executionConfig->Flags = QUIC_EXECUTION_CONFIG_FLAG_NONE;
        executionConfig->PollingIdleTimeoutUs = 50000;

        /*
            We want to map the client workers to the processors after the server
           workers so i = clientFirstProcessorId + j (j = 0, 1, 2, ...,
           numMsQuicWorkersPerNode - 1) is mapped to
           [numMsQuicWorkersPerNode, numCores)
        */
        for (std::uint16_t workerId = 0; workerId < numMsQuicWorkersPerNode; workerId++)
        {
            std::uint16_t processorId = clientFirstProcessorId + workerId;
            // processorId should be mapped to [numMsQuicWorkersPerNode, numCores)
            processorId %= (numProcessors - numMsQuicWorkersPerNode);
            processorId += numMsQuicWorkersPerNode;

            executionConfig->ProcessorList[workerId] = processorId;
        }

        std::unique_ptr<MOQTClient> moqtClient =
        client_setup(std::make_tuple(executionConfig, execConfigLen), "127.0.0.1",
                     basePort + numNodes - 2 /* we are the last node */);

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

            std::cout << currTimestamp - *sentTimestamp << " "
                      << "Track Alias: " << enrichedObject.header_->trackAlias_
                      << " " << "Group Id: " << groupId << " "
                      << "Object Id: " << objectId << " "
                      << enrichedObject.object_.payload_.size() << '\n';

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
