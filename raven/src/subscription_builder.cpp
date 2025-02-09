#include <atomic>
#include <subscription_builder.hpp>

namespace rvn
{

std::atomic_uint64_t SubscriptionBuilder::subscribeIDCounter_ = 0;

} // namespace rvn