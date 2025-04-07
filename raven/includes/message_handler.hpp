#pragma once
#include <serialization/messages.hpp>
#include <subscription_manager.hpp>

namespace rvn {
class MessageHandler {
  struct StreamState &streamState_;
  class SubscriptionManager *subscriptionManager_;

public:
  MessageHandler(StreamState &streamState,
                 SubscriptionManager *subscriptionManager)
      : streamState_(streamState), subscriptionManager_(subscriptionManager) {};

  void operator()(ClientSetupMessage clientSetupMessage);
  void operator()(ServerSetupMessage serverSetupMessage);
  void operator()(SubscribeMessage subscribeMessage);
  void operator()(StreamHeaderSubgroupObject streamHeaderSubgroupObject);
  void operator()(StreamHeaderSubgroupMessage streamHeaderSubgroupMessage);
  void operator()(BatchSubscribeMessage batchSubscribeMessage);
};
} // namespace rvn
