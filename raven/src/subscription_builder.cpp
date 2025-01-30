#include <subscription_builder.hpp>

namespace rvn
{

thread_local std::uint64_t SubscriptionBuilder::subscribeIDCounter_ = 0;

} // namespace rvn