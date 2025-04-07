#pragma once
////////////////////////////////////////////
#include <msquic.h>

///////////////////////////////////////////
#include <csignal>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <optional>
#include <ostream>
#include <source_location>
#include <sstream>
#include <thread>
///////////////////////////////////////////
// NOTE: Utils can not use any raven header file

#define LOGE(...) \
    rvn::utils::LOG(std::source_location::current(), "LOGGING UNEXPECTED STATE: ", __VA_ARGS__)

#define DECLARE_SINGLETON(ClassName)        \
    class ClassName##Handle                 \
    {                                       \
        static ClassName* instance;         \
                                            \
    public:                                 \
        ClassName##Handle()                 \
        {                                   \
            if (instance == nullptr)        \
            {                               \
                instance = new ClassName(); \
            }                               \
        }                                   \
        ClassName* get_instance()           \
        {                                   \
            return instance;                \
        }                                   \
        ClassName& operator*()              \
        {                                   \
            return *instance;               \
        }                                   \
        const ClassName& operator*() const  \
        {                                   \
            return *instance;               \
        }                                   \
        ClassName* operator->()             \
        {                                   \
            return instance;                \
        }                                   \
        const ClassName* operator->() const \
        {                                   \
            return instance;                \
        }                                   \
    };

#ifdef _MSC_VER
#define WEAK_LINKAGE __declspec(selectany)
#else
#define WEAK_LINKAGE __attribute__((weak))
#endif

namespace rvn::utils
{
template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept
{
    return static_cast<typename std::underlying_type<E>::type>(e);
}

template <typename T> static void print(std::ostream& os, T value)
{
    os << value << std::endl;
}

template <typename T, typename... Args>
static void print(std::ostream& os, T value, Args... args)
{
    os << value << " ";
    print(os, args...);
}

template <typename... Args>
static void
ASSERT_LOG_THROW([[maybe_unused]] bool assertion, [[maybe_unused]] Args... args)
{
#ifdef RAVEN_ENABLE_ASSERTIONS
    if (!assertion)
    {
        print(std::cerr, args...);
        exit(1);
    }
#endif
}

template <typename... Args>
static void LOG(const std::source_location location, Args... args)
{
    std::clog << "file: " << location.file_name() << '(' << location.line() << ':'
              << location.column() << ") `" << location.function_name() << "`: ";
    print(std::cerr, args...);
}

template <typename... Args> QUIC_STATUS NoOpSuccess(Args...)
{
    return QUIC_STATUS_SUCCESS;
}

template <typename... Args> void NoOpVoid(Args...)
{
    return;
};

inline void LOG_EVENT_BASE(std::ostream& os, const std::string& eventMessage)
{
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);
    os << eventMessage << std::endl;
}

template <typename... Args> void LOG_EVENT(std::ostream& os, Args... args)
{
    std::ostringstream oss;
    print(oss, args...);
    LOG_EVENT_BASE(os, oss.str());
}

// double implication TT or FF
static inline bool xnor(bool a, bool b)
{
    return (a && b) || (!a && !b);
}

static inline std::uint64_t next_power_of_2(std::uint64_t n) noexcept
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;
    return n;
}

static inline void thread_set_affinity(std::thread& th, int core_id)
{
    // Get native thread handle
    pthread_t native_handle = th.native_handle();

    // Create a CPU set and set the desired core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    // Set affinity for the thread
    int result = pthread_setaffinity_np(native_handle, sizeof(cpu_set_t), &cpuset);

    if (result != 0)
    {
        std::cerr << "Error calling pthread_setaffinity_np: " << result << std::endl;
        std::exit(1);
    }
}

static inline void thread_set_max_priority(std::thread& th)
{
    sched_param sch;
    int policy;
    pthread_getschedparam(th.native_handle(), &policy, &sch);
    sch.sched_priority = sched_get_priority_max(policy);
    if (int result = pthread_setschedparam(th.native_handle(), policy, &sch))
    {
        std::cerr << "Failed to set Thread Priority, result= " << result << std::endl;
        exit(1);
    }
}

static inline void wait_for(const std::atomic_bool& flag)
{
    do
    {

    } while (!flag.load(std::memory_order_acquire));
}

template <typename T>
bool optional_equality(const std::optional<T>& lhs, const std::optional<T>& rhs)
{
    if (lhs.has_value() && rhs.has_value())
        return lhs.value() == rhs.value();

    else if (!lhs.has_value() && !rhs.has_value())
        return true;

    return false;
}

} // namespace rvn::utils

namespace rvn
{
struct MOQTUtilities
{
    static void check_setting_assertions(QUIC_SETTINGS* Settings_, uint32_t SettingsSize_)
    {
        utils::ASSERT_LOG_THROW(Settings_ != nullptr, "Settings_ is nullptr");
        utils::ASSERT_LOG_THROW(SettingsSize_ != 0, "SettingsSize_ is 0");
    }
};
} // namespace rvn
