#pragma once
////////////////////////////////////////////
#include <msquic.h>
////////////////////////////////////////////
#include <memory>
#include <stdexcept>
#include <type_traits>
////////////////////////////////////////////
#include <utilities.hpp>
////////////////////////////////////////////
namespace rvn::detail
{

template <typename Ctor, typename Dtor> class unique_handler1
{
    // Pointer to QUIC_HANDLER owned by unique_handler1
    HQUIC handler;

    // non owning callable (function pointor)
    Ctor open;
    Dtor close;

    void destroy()
    {
        if (handler != NULL)
            close(handler);
    }

    void reset()
    {
        destroy();
        handler = NULL;
    }

protected:
    unique_handler1(Ctor open_, Dtor close_) noexcept : handler(NULL)
    {
        open = open_;
        close = close_;
    }

    unique_handler1() : handler(NULL) {};

    unique_handler1(const unique_handler1&) = delete;
    unique_handler1& operator=(const unique_handler1&) = delete;

    unique_handler1(unique_handler1&& rhs)
    {
        if (this == &rhs)
            return;

        reset();
        // take ownership
        handler = rhs.handler;
        open = rhs.open;
        close = rhs.close;

        // RHS releases ownership
        rhs.handler = NULL;
    }

    unique_handler1& operator=(unique_handler1&& rhs)
    {
        if (this == &rhs)
            return *this;

        reset();
        // take ownership
        handler = rhs.handler;
        open = rhs.open;
        close = rhs.close;

        // RHS releases ownership
        rhs.handler = NULL;
        return *this;
    }

    /*Don't take universal reference as this is C.
      Everything is copied

      If return is QUIC_FAILED then throw exception
    */
    template <typename... Args> QUIC_STATUS construct(Args... args)
    {
        return open(args..., &handler);
    }

public:
    HQUIC
    get() const
    {
        return handler;
    }
    ~unique_handler1() noexcept
    {
        destroy();
    }
};

// To be used when only one construct function exists
template <typename Open, typename Close, typename Start, typename Stop = decltype(&utils::NoOpVoid<HQUIC>)>
class unique_handler2
{
    // Pointer to QUIC_HANDLER owned by unique_handler2
    HQUIC handler;

    // non owning callable (function pointor)
    Open open_func;
    Close close_func;

    Start start_func;
    Stop stop_func;

    void destroy()
    {
        if (handler != NULL)
        {
            stop_func(handler);
            close_func(handler);
        };
    }

    void reset()
    {
        destroy();
        handler = NULL;
    }

protected:
    unique_handler2(Open open_, Close close_, Start start_, Stop stop_ = &utils::NoOpVoid<HQUIC>) noexcept
    : handler(NULL)
    {
        open_func = open_;
        close_func = close_;

        start_func = start_;
        stop_func = stop_;
    }
    unique_handler2() : handler(NULL) {};

    unique_handler2(const unique_handler2&) = delete;
    unique_handler2& operator=(const unique_handler2&) = delete;

    unique_handler2(unique_handler2&& rhs)
    {
        if (this == &rhs)
            return;

        reset();
        // take ownership
        handler = rhs.handler;
        open_func = rhs.open_func;
        close_func = rhs.close_func;

        start_func = rhs.start_func;
        stop_func = rhs.stop_func;

        // RHS releases ownership
        rhs.handler = NULL;
    }

    unique_handler2& operator=(unique_handler2&& rhs)
    {
        if (this == &rhs)
            return *this;

        reset();
        // take ownership
        handler = rhs.handler;
        open_func = rhs.open_func;
        close_func = rhs.close_func;

        start_func = rhs.start_func;
        stop_func = rhs.stop_func;

        // RHS releases ownership
        rhs.handler = NULL;
        return *this;
    }

    /*Don't take universal reference as this is C interface.
      Everything is copied

      If return is QUIC_FAILED then throw exception
    */
    template <typename... Args> QUIC_STATUS open_handler(Args... args)
    {
        return open_func(args..., &handler);
    }

    template <typename... Args> QUIC_STATUS start_handler(Args... args)
    {
        QUIC_STATUS status;
        status = start_func(handler, args...);
        if (QUIC_FAILED(status))
            close_func(handler);
        return status;
    }

public:
    HQUIC
    get() const
    {
        return handler;
    }
    ~unique_handler2() noexcept
    {
        destroy();
    }
};

}; // namespace rvn::detail

namespace rvn
{

/*-----------------QUIC_API_TABLE------------------------*/

/*QUIC_API_TABLE_deleter has to be a function and not lambda due
 * to [-Wsubobject-linkage] warning. This is because the lambda
 * has a different type in each subunit anad hence the class
 * unique_QUIC_API_TABLE will also have different data type
 * members in each subunit. This is not allowed in C++ due to ODR
 * (https://www.reddit.com/r/cpp_questions/comments/8im3h4/comment/dyt5u68/)
 */
static inline void QUIC_API_TABLE_deleter(const QUIC_API_TABLE* tbl)
{
    MsQuicClose(tbl);
};

using QUIC_API_TABLE_uptr_t =
std::unique_ptr<const QUIC_API_TABLE, decltype(&QUIC_API_TABLE_deleter)>;

class unique_QUIC_API_TABLE : public QUIC_API_TABLE_uptr_t
{
public:
    unique_QUIC_API_TABLE(const QUIC_API_TABLE* tbl)
    : QUIC_API_TABLE_uptr_t(tbl, &QUIC_API_TABLE_deleter)
    {
    }
};

/* rvn::make_unique should not be called on
   non specialised template */
static inline unique_QUIC_API_TABLE make_unique_quic_table()
{
    const QUIC_API_TABLE* tbl;
    QUIC_STATUS status = MsQuicOpen2(&tbl);
    if (QUIC_FAILED(status))
        throw std::runtime_error("MsQuicOpenError");
    return unique_QUIC_API_TABLE(tbl);
}

/*----------------MsQuic->RegistrationOpen---------------*/
class unique_registration
: public detail::unique_handler1<decltype(QUIC_API_TABLE::RegistrationOpen), decltype(QUIC_API_TABLE::RegistrationClose)>
{
public:
    unique_registration(const QUIC_API_TABLE* tbl_, const QUIC_REGISTRATION_CONFIG* RegConfig);
    unique_registration();
};

/*------------MsQuic->ListenerOpen and Start-------------*/
class unique_listener
: public detail::unique_handler2<decltype(QUIC_API_TABLE::ListenerOpen),
                                 decltype(QUIC_API_TABLE::ListenerClose),
                                 decltype(QUIC_API_TABLE::ListenerStart),
                                 decltype(QUIC_API_TABLE::ListenerStop)>
{
public:
    struct ListenerOpenParams
    {
        HQUIC registration;
        QUIC_LISTENER_CALLBACK_HANDLER listenerCb;
        void* context = NULL;
    };
    struct ListenerStartParams
    {
        const QUIC_BUFFER* const AlpnBuffers;
        uint32_t AlpnBufferCount = 1;
        const QUIC_ADDR* LocalAddress;
    };

    unique_listener(const QUIC_API_TABLE* tbl_,
                    ListenerOpenParams openParams,
                    ListenerStartParams startParams);
    unique_listener();
};

/*------------MsQuic->ConnectionOpen and Start-------------*/
class unique_connection
: public detail::unique_handler2<decltype(QUIC_API_TABLE::ConnectionOpen), decltype(QUIC_API_TABLE::ConnectionClose), decltype(QUIC_API_TABLE::ConnectionStart)
                                 /*There is no ConnectionStop*/>
{
public:
    struct ConnectionOpenParams
    {
        HQUIC registration;
        QUIC_CONNECTION_CALLBACK_HANDLER connectionCb;
        void* context = NULL;
    };
    struct ConnectionStartParams
    {
        HQUIC Configuration;
        QUIC_ADDRESS_FAMILY Family;
        const char* ServerName;
        uint16_t ServerPort;
    };

    unique_connection(const QUIC_API_TABLE* tbl_,
                      ConnectionOpenParams openParams,
                      ConnectionStartParams startParams);
    unique_connection();
};

/*-------------MsQuic->Config open and load--------------*/
class unique_configuration
: public detail::unique_handler2<decltype(QUIC_API_TABLE::ConfigurationOpen),
                                 decltype(QUIC_API_TABLE::ConfigurationClose),
                                 decltype(QUIC_API_TABLE::ConfigurationLoadCredential)
                                 /*No need to unload configuration*/>
{
public:
    struct ConfigurationOpenParams
    {
        HQUIC registration;
        const QUIC_BUFFER* const AlpnBuffers;
        uint32_t AlpnBufferCount = 1;
        const QUIC_SETTINGS* Settings;
        uint32_t SettingsSize;
        void* Context;
    };

    struct ConfigurationStartParams
    {
        const QUIC_CREDENTIAL_CONFIG* CredConfig;
    };

    unique_configuration(const QUIC_API_TABLE* tbl_,
                         ConfigurationOpenParams openParams,
                         ConfigurationStartParams startParams);
    unique_configuration();
};

/*-------------MsQuic->Stream open and start--------------*/

class unique_stream
{
    HQUIC streamHandle = NULL;

    QUIC_STREAM_OPEN_FN open_func;
    QUIC_STREAM_CLOSE_FN close_func;
    QUIC_STREAM_START_FN start_func;
    QUIC_STREAM_SHUTDOWN_FN shutdown_func;

    void destroy()
    {
        if (streamHandle != NULL)
        {
            // TODO: shutdown function always uses with defaults flag and
            // error code
            shutdown_func(streamHandle, QUIC_STREAM_SHUTDOWN_FLAG_GRACEFUL, 0);
            close_func(streamHandle);
        };
    }

    void reset()
    {
        destroy();
        streamHandle = NULL;
    }

    template <typename... Args> QUIC_STATUS open_handler(Args... args)
    {
        return open_func(args..., &streamHandle);
    }

    template <typename... Args> QUIC_STATUS start_handler(Args... args)
    {
        QUIC_STATUS status;
        status = start_func(streamHandle, args...);
        if (QUIC_FAILED(status))
            close_func(streamHandle);
        return status;
    }

    struct StreamOpenParams
    {
        HQUIC Connection;
        QUIC_STREAM_OPEN_FLAGS Flags;
        QUIC_STREAM_CALLBACK_HANDLER Handler;
        void* Context;
    };

    struct StreamStartParams
    {
        QUIC_STREAM_START_FLAGS Flags;
    };

public:
    unique_stream(const QUIC_API_TABLE* tbl_, StreamOpenParams openParams, StreamStartParams startParams)
    {
        open_func = tbl_->StreamOpen;
        close_func = tbl_->StreamClose;
        start_func = tbl_->StreamStart;
        shutdown_func = tbl_->StreamShutdown;

        if (QUIC_FAILED(open_handler(openParams.Connection, openParams.Flags,
                                     openParams.Handler, openParams.Context)))
            throw std::runtime_error("StreamOpenFailure");

        if (QUIC_FAILED(start_handler(startParams.Flags)))
            throw std::runtime_error("StreamStartFailure");
    }

    unique_stream()
    {
        open_func = nullptr;
        close_func = nullptr;
        start_func = nullptr;
        shutdown_func = nullptr;
    }

    ~unique_stream() noexcept
    {
        destroy();
    }

    unique_stream(const QUIC_API_TABLE* tbl_, HQUIC streamHandle_)
    : streamHandle(streamHandle_)
    {
        open_func = nullptr;
        start_func = nullptr;

        close_func = tbl_->StreamClose;
        shutdown_func = tbl_->StreamShutdown;
    }

    unique_stream(const unique_stream&) = delete;
    unique_stream& operator=(const unique_stream&) = delete;

    unique_stream(unique_stream&& rhs)
    {
        if (this == &rhs)
            return;
        reset();
        // take ownership
        streamHandle = rhs.streamHandle;
        open_func = rhs.open_func;
        close_func = rhs.close_func;

        start_func = rhs.start_func;
        shutdown_func = rhs.shutdown_func;

        // RHS releases ownership
        rhs.streamHandle = NULL;
    }

    unique_stream& operator=(unique_stream&& rhs)
    {
        if (this == &rhs)
            return *this;
        reset();
        // take ownership
        streamHandle = rhs.streamHandle;
        open_func = rhs.open_func;
        close_func = rhs.close_func;

        start_func = rhs.start_func;
        shutdown_func = rhs.shutdown_func;

        // RHS releases ownership
        rhs.streamHandle = NULL;
        return *this;
    }

    HQUIC get() const
    {
        return streamHandle;
    }
};

}; // namespace rvn
