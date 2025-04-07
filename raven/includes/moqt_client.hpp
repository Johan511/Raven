#pragma once
////////////////////////////////////////////
#include "definitions.hpp"
#include <moqt_base.hpp>
#include <msquic.h>
#include <serialization/messages.hpp>
////////////////////////////////////////////
#include <atomic>
#include <cstdint>
////////////////////////////////////////////
#include <contexts.hpp>
#include <serialization/serialization.hpp>
#include <subscription_manager.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>
////////////////////////////////////////////

namespace rvn {
class MOQTClient : public MOQT {
public:
  // ConnectionState is not default constructable
  // so we need to have a optional wrapper
  std::shared_ptr<ConnectionState> connectionState;

  struct DataStreamUserHandle {
    std::weak_ptr<void> streamLifeTimeFlag_;
    std::shared_ptr<StreamHeaderSubgroupMessage> streamHeaderSubgroupMessage_;
    std::shared_ptr<MPMCQueue<StreamHeaderSubgroupObject>> objectQueue_;
  };

  MPMCQueue<DataStreamUserHandle> dataStreamUserHandles_;

  // Alternative deliever method where we enqueue all received objects into a
  // single queue
  struct EnrichedObjectMessage {
    std::shared_ptr<StreamHeaderSubgroupMessage> header_;
    StreamHeaderSubgroupObject object_;
  };
  MPMCQueue<EnrichedObjectMessage> receivedObjects_;

  void subscribe(SubscribeMessage &&subscribeMessage) {
    QUIC_BUFFER *quicBuffer = serialization::serialize(subscribeMessage);
    connectionState->send_control_buffer(quicBuffer);
  }

  void subscribe(BatchSubscribeMessage &&batchSubscribeMessage) {
    QUIC_BUFFER *quicBuffer = serialization::serialize(batchSubscribeMessage);
    connectionState->send_control_buffer(quicBuffer);
  }

  MOQTClient(
      std::tuple<QUIC_EXECUTION_CONFIG *, std::uint64_t> execConfigTuple = {
          nullptr, 0});

  void start_connection(QUIC_ADDRESS_FAMILY Family, const char *ServerName,
                        uint16_t ServerPort);

  ClientSetupMessage get_clientSetupMessage();

  QUIC_STATUS accept_data_stream(HQUIC streamHandle) {
    return connectionState->accept_data_stream(streamHandle);
  }

  // atomic flags for multi thread synchronization
  // make sure no connections are accepted until whole setup required is
  // completed
  std::atomic_bool quicConnectionStateSetupFlag_{};

  // make sure no communication untill setup messages are exchanged
  std::atomic_bool ravenConnectionSetupFlag_{};
};
} // namespace rvn
