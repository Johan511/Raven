#include <timer_wheel_impl.hpp>
#include <utilities.hpp>

namespace rvn
{
// Timer wheel with 10ms jitter and 128 slots
using Timer = timer::Timer<std::chrono::steady_clock, 10, 128>;
DECLARE_SINGLETON(Timer)
} // namespace rvn
