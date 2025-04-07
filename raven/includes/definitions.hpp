#pragma once

#include <blockingconcurrentqueue.h>
#include <chrono>
#include <list>
#include <shared_mutex>
#include <utility>

namespace rvn
{
template <typename T> using StableContainer = std::list<T>;

// wrapper class for tsan suppressions
template <typename T> class MPMCQueue
{
    moodycamel::BlockingConcurrentQueue<T> mpmcQueue;

public:
    __attribute__((no_sanitize("thread"))) bool enqueue(const T& t)
    {
        return mpmcQueue.enqueue(t);
    }
    __attribute__((no_sanitize("thread"))) bool enqueue(T&& t)
    {
        return mpmcQueue.enqueue(std::move(t));
    }

    template <typename U>
    __attribute__((no_sanitize("thread"))) void wait_dequeue(U& u)
    {
        return mpmcQueue.wait_dequeue(u);
    }

    template <typename U>
    __attribute__((no_sanitize("thread"))) bool try_dequeue(U& u)
    {
        return mpmcQueue.try_dequeue(u);
    }

    __attribute__((no_sanitize("thread"))) T wait_dequeue_ret()
    {
        T t;
        mpmcQueue.wait_dequeue(t);
        return t;
    }

    __attribute__((no_sanitize("thread"))) size_t size_approx()
    {
        return mpmcQueue.size_approx();
    }
};

template <typename T> class RWProtected
{
    mutable std::shared_mutex mutex_;
    T data_;

public:
    template <typename... Args>
    RWProtected(Args&&... args) : data_(std::forward<Args>(args)...)
    {
    }

    template <typename F> decltype(auto) read(F&& f) const noexcept
    {
        std::shared_lock lock(mutex_);
        return f(data_);
    }

    template <typename F> decltype(auto) write(F&& f) noexcept
    {
        std::unique_lock lock(mutex_);
        return f(data_);
    }
};

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

} // namespace rvn
