#include <array>
#include <atomic>
#include <chrono>
#include <definitions.hpp>
#include <functional>
#include <thread>
#include <vector>

namespace rvn::timer
{

// jitter is the accepted error when calling the callback
// if callback is to be called after t, we will call it before t + jitter ( and after t - jitter (do we need t - jitter condition) )
template <typename SteadyClock, std::uint32_t Jitter, std::uint32_t NumSlots>
class Timer
{
    using TimerIndex = std::uint64_t;
    using InternalCallback = std::function<void(TimerIndex)>;

    struct TimerEvent
    {
        InternalCallback callback_;
        TimerIndex index_;

        TimerEvent(InternalCallback callback, TimerIndex index)
        : callback_(std::move(callback)), index_(index)
        {
        }
    };

    // clang-format off
    /*
        Let us say Jitter is 50ms and NumSlots is 10

        Events in 0-50, 50-100, 100-150, 150-200, 200-250, 250-300, 300-350, 350-400, 400-450, 450-500
        go in slots    0,      1,       2,       3,       4,       5,     6,       7,       8,       9

      and it cycles over that is 500-550 again go in slot 0 and so on
    */
    // clang-format on
    std::array<RWProtected<std::vector<TimerEvent>>, NumSlots> timers_;
    std::atomic<std::uint64_t> timerIndexCounter_;

    std::chrono::milliseconds lastProcessedTime_;

    bool threadJoinFlag_;
    std::jthread pollThread_;

    //////////////////////////////////////////////////////////////////////////

    void clear_all()
    {
        for (auto& slot : timers_)
        {
            slot.write(
            [](auto& slot)
            {
                for (auto& timerEvent : slot)
                    timerEvent.callback_(timerEvent.index_);
                slot.clear();
            });
        }
    }

    void poll()
    {
        // NOTE: make sure multiple threads are not calling poll at the same time

        std::chrono::milliseconds currMsTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(
        SteadyClock::now().time_since_epoch());

        if ((lastProcessedTime_ - currMsTime).count() >= Jitter * NumSlots)
            clear_all();
        else
        {
            std::uint64_t beginIndex = (lastProcessedTime_.count() / Jitter) % NumSlots;
            std::uint64_t endIndex = (currMsTime.count() / Jitter) % NumSlots;

            while (beginIndex != endIndex)
            {
                timers_[beginIndex].write(
                [&](std::vector<TimerEvent>& slot)
                {
                    for (auto& timerEvent : slot)
                    {
                        timerEvent.callback_(timerEvent.index_);
                    }

                    slot.clear();
                });

                beginIndex = (beginIndex + 1) % NumSlots;
            }
        }

        lastProcessedTime_ = currMsTime;
    }

public:
    template <typename Callback>
    // `callback` will be executed in about `duration` milliseconds
    TimerIndex add_timer(std::chrono::milliseconds duration, Callback&& callback)
    {
        if (threadJoinFlag_)
            return -1;

        std::uint64_t timerIndex =
        timerIndexCounter_.fetch_add(1, std::memory_order::relaxed);

        std::chrono::milliseconds currMsTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(
        SteadyClock::now().time_since_epoch());


        if ((lastProcessedTime_ - currMsTime).count() >= Jitter * NumSlots)
            // events have not been polled for a very long time, need to do something about it
            clear_all();

        std::uint64_t slotIndex =
        ((currMsTime.count() + duration.count()) / Jitter) % NumSlots;

        timers_[slotIndex].write(
        [&](auto& slot)
        { slot.emplace_back(std::forward<Callback>(callback), timerIndex); });

        return timerIndex;
    }

    Timer()
    : timerIndexCounter_(0), lastProcessedTime_(0), threadJoinFlag_(false)
    {
        // cannot be part of initializer list because we use poll function (cant use non static members before construction)
        pollThread_ = std::jthread(
        [this]()
        {
            while (true)
            {
                if (threadJoinFlag_) [[unlikely]]
                    break;
                poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(Jitter));
            }
        });
    }

    ~Timer()
    {
        threadJoinFlag_ = true;
    }
};

} // namespace rvn::timer
