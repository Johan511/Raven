#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <timer_wheel.hpp>
using namespace rvn::timer;

using SteadyClock = std::chrono::steady_clock;

constexpr std::size_t numThreads = 8;

int main()
{
    Timer<SteadyClock, 16, 512> timer;

    std::array<std::thread, numThreads> threadPool;
    // TODO: align with std::hardware_destructive_interference_size (Minimum
    // offset between two objects to avoid false sharing)
    double totalJitter = 0;
    std::atomic<std::uint64_t> numFinishedTimers = 0;

    constexpr std::uint64_t numTimersPerThread = 10'000;

    for (auto& thread : threadPool)
    {
        thread = std::thread(
        [&]()
        {
            for (std::uint64_t i = 0; i < numTimersPerThread; i++)
            {
                timer.add_timer(
                std::chrono::milliseconds(250),
                [currTime = SteadyClock::now(), &totalJitter, &numFinishedTimers](std::uint64_t)
                {
                    auto diff = SteadyClock::now() - currTime;
                    numFinishedTimers.fetch_add(1, std::memory_order_relaxed);
                    double jitter =
                    std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();

                    std::cout << "Jitter: " << jitter << '\n';

                    totalJitter += jitter;
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    for (auto& thread : threadPool)
        thread.join();

    while (numFinishedTimers.load(std::memory_order_relaxed) < numThreads * numTimersPerThread)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    std::cout << "Average jitter: " << totalJitter / (numTimersPerThread * numThreads)
              << std::endl;

    return 0;
}
