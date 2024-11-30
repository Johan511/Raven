#pragma once

#include <blockingconcurrentqueue.h>
#include <list>
#include <utility>

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
};
