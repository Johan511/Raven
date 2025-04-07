/////////////////////////////////////////////////////////
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
#include "../test_utilities.hpp"
/////////////////////////////////////////////////////////

const std::string messagePayload("Hello World!");

using namespace rvn;

struct InterprocessSynchronizationData {
  boost::interprocess::interprocess_mutex mutex;
  bool serverSetup;
  bool clientDone;
};

namespace bip = boost::interprocess;

// TODO: use shared memory synchronization instead of `sleep_fir`
int main() {
  std::string sharedMemoryName = "simple_data_transfer_test_";
  sharedMemoryName += std::to_string(getpid());

  bip::shared_memory_object shmParent(
      bip::create_only, sharedMemoryName.c_str(), bip::read_write);
  shmParent.truncate(sizeof(InterprocessSynchronizationData));
  bip::mapped_region regionParent(shmParent, bip::read_write);
  InterprocessSynchronizationData *dataParent =
      new (regionParent.get_address()) InterprocessSynchronizationData();

  dataParent->serverSetup = false;
  dataParent->clientDone = false;

  if (fork()) {
    // parent process, server
    std::unique_ptr<MOQTServer> moqtServer = server_setup();

    auto dm = moqtServer->dataManager_;
    auto trackHandle = dm->add_track_identifier({}, "track");
    auto groupHandle =
        trackHandle.lock()->add_group(GroupId(0), PublisherPriority(0), {});
    auto subgroupHandle = groupHandle.lock()->add_subgroup(ObjectId(1));
    subgroupHandle.add_object(messagePayload);

    {
      std::unique_lock lock(dataParent->mutex);
      dataParent->serverSetup = true;
    }

    for (;;) {
      std::unique_lock lock(dataParent->mutex);
      if (dataParent->clientDone)
        break;
    }

    std::cout << "Server done" << std::endl;

    wait(NULL);
    exit(0);
  } else
  // child process
  {
    // Open shared memory
    bip::shared_memory_object shmChild(bip::open_only, sharedMemoryName.c_str(),
                                       bip::read_write);
    bip::mapped_region regionChildl(shmChild, bip::read_write);
    InterprocessSynchronizationData *dataChild =
        static_cast<InterprocessSynchronizationData *>(
            regionChildl.get_address());

    for (;;) {
      std::unique_lock lock(dataChild->mutex);
      if (dataChild->serverSetup)
        break;
    }

    std::unique_ptr<MOQTClient> moqtClient = client_setup();

    SubscriptionBuilder subscriptionBuilder;
    subscriptionBuilder.set_track_alias(TrackAlias(0));
    subscriptionBuilder.set_track_namespace({});
    subscriptionBuilder.set_track_name("track");
    subscriptionBuilder.set_data_range(
        SubscriptionBuilder::Filter::absoluteRange, {GroupId(0), ObjectId(0)},
        {GroupId(0), ObjectId(1)});

    subscriptionBuilder.set_subscriber_priority(0);
    subscriptionBuilder.set_group_order(0);

    auto subMessage = subscriptionBuilder.build();

    moqtClient->subscribe(std::move(subMessage));

    auto &dataStreams = moqtClient->dataStreamUserHandles_;
    auto dataStreamUserHandle = dataStreams.wait_dequeue_ret();

    auto &objectQueue = dataStreamUserHandle.objectQueue_;

    auto streamHeaderSubgroupObject = objectQueue->wait_dequeue_ret();

    try {
      utils::ASSERT_LOG_THROW(
          streamHeaderSubgroupObject.payload_ == messagePayload,
          "Payload mismatch", "Received: ", streamHeaderSubgroupObject.payload_,
          "Expected: ", messagePayload);
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
      exit(1);
    }

    dataChild->clientDone = true;
    std::cout << "Client done" << std::endl;
    exit(0);
  }
}
