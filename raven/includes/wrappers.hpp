#pragma once

#include <msquic.h>

#include <memory>
#include <stdexcept>

namespace rvn::detail {

template <typename... Args>
QUIC_STATUS NoOpSuccess(Args...) {
    return QUIC_STATUS_SUCCESS;
}

template <typename... Args>
void NoOpVoid(Args...) {
    return;
};
template <typename Ctor, typename Dtor>
class unique_handler1 {
    // Pointer to QUIC_HANDLER owned by unique_handler1
    HQUIC handler;

    // non owning callable (function pointor)
    Ctor open;
    Dtor close;

    void destroy() {
        if (handler != NULL) close(handler);
    }

    void reset() {
        destroy();
        handler = NULL;
    }

   protected:
    unique_handler1(Ctor open_, Dtor close_) noexcept
        : handler(NULL) {
        open = open_;
        close = close_;
    }

    unique_handler1() : handler(NULL){};

    unique_handler1(const unique_handler1 &) = delete;
    unique_handler1 &operator=(const unique_handler1 &) = delete;

    unique_handler1(unique_handler1 &&rhs) {
        reset();
        // take ownership
        handler = rhs.handler;
        open = rhs.open;
        close = rhs.close;

        // RHS releases ownership
        rhs.handler = NULL;
    }

    unique_handler1 &operator=(unique_handler1 &&rhs) {
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
    template <typename... Args>
    QUIC_STATUS construct(Args... args) {
        return open(args..., &handler);
    }

   public:
    HQUIC get() const { return handler; }
    ~unique_handler1() noexcept { destroy(); }
};

// To be used when only one construct function exists
template <typename Open, typename Close, typename Start,
          typename Stop = decltype(&NoOpVoid<HQUIC>)>
class unique_handler2 {
    // Pointer to QUIC_HANDLER owned by unique_handler2
    HQUIC handler;

    // non owning callable (function pointor)
    Open open_func;
    Close close_func;

    Start start_func;
    Stop stop_func;

    void destroy() {
        if (handler != NULL) {
            stop_func(handler);
            close_func(handler);
        };
    }

    void reset() {
        destroy();
        handler = NULL;
    }

   protected:
    unique_handler2(Open open_, Close close_, Start start_,
                    Stop stop_ = &NoOpVoid<HQUIC>) noexcept
        : handler(NULL) {
        open_func = open_;
        close_func = close_;

        start_func = start_;
        stop_func = stop_;
    }
    unique_handler2() : handler(NULL){};

    unique_handler2(const unique_handler2 &) = delete;
    unique_handler2 &operator=(const unique_handler2 &) = delete;

    unique_handler2(unique_handler2 &&rhs) {
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

    unique_handler2 &operator=(unique_handler2 &&rhs) {
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
    template <typename... Args>
    QUIC_STATUS open_handler(Args... args) {
        return open_func(args..., &handler);
    }

    template <typename... Args>
    QUIC_STATUS start_handler(Args... args) {
        QUIC_STATUS status;
        status = start_func(handler, args...);
        if (QUIC_FAILED(status)) close_func(handler);
        return status;
    }

   public:
    HQUIC get() const { return handler; }
    ~unique_handler2() noexcept { destroy(); }
};

};  // namespace rvn::detail

namespace rvn {

/*-----------------QUIC_API_TABLE------------------------*/

static inline auto QUIC_API_TABLE_deleter =
    [](const QUIC_API_TABLE *tbl) { MsQuicClose(tbl); };

using QUIC_API_TABLE_uptr_t =
    std::unique_ptr<const QUIC_API_TABLE,
                    decltype(QUIC_API_TABLE_deleter)>;

class unique_QUIC_API_TABLE : public QUIC_API_TABLE_uptr_t {
   public:
    unique_QUIC_API_TABLE(const QUIC_API_TABLE *tbl);
};

/* rvn::make_unique should not be called on
   non specialised template */
static inline unique_QUIC_API_TABLE make_unique_quic_table() {
    const QUIC_API_TABLE *tbl;
    QUIC_STATUS status = MsQuicOpen2(&tbl);
    if (QUIC_FAILED(status))
        throw std::runtime_error("MsQuicOpenError");
    return unique_QUIC_API_TABLE(tbl);
}

/*----------------MsQuic->RegistrationOpen---------------*/
class unique_registration
    : public detail::unique_handler1<
          decltype(QUIC_API_TABLE::RegistrationOpen),
          decltype(QUIC_API_TABLE::RegistrationClose)> {
   public:
    unique_registration(
        const QUIC_API_TABLE *tbl_,
        const QUIC_REGISTRATION_CONFIG *RegConfig);
    unique_registration();
};

/*------------MsQuic->ListenerOpen and Start-------------*/
class unique_listener
    : public detail::unique_handler2<
          decltype(QUIC_API_TABLE::ListenerOpen),
          decltype(QUIC_API_TABLE::ListenerClose),
          decltype(QUIC_API_TABLE::ListenerStart),
          decltype(QUIC_API_TABLE::ListenerStop)> {
   public:
    struct ListenerOpenParams {
        HQUIC registration;
        QUIC_LISTENER_CALLBACK_HANDLER listenerCb;
        void *context = NULL;
    };
    struct ListenerStartParams {
        const QUIC_BUFFER *const AlpnBuffers;
        uint32_t AlpnBufferCount = 1;
        const QUIC_ADDR *LocalAddress;
    };

    unique_listener(const QUIC_API_TABLE *tbl_,
                    ListenerOpenParams openParams,
                    ListenerStartParams startParams);
    unique_listener();
};

/*------------MsQuic->ConnectionOpen and Start-------------*/
class unique_connection
    : public detail::unique_handler2<
          decltype(QUIC_API_TABLE::ConnectionOpen),
          decltype(QUIC_API_TABLE::ConnectionClose),
          decltype(QUIC_API_TABLE::ConnectionStart)
          /*There is no ConnectionStop*/> {
   public:
    struct ConnectionOpenParams {
        HQUIC registration;
        QUIC_CONNECTION_CALLBACK_HANDLER connectionCb;
        void *context = NULL;
    };
    struct ConnectionStartParams {
        HQUIC Configuration;
        QUIC_ADDRESS_FAMILY Family;
        const char *ServerName;
        uint16_t ServerPort;
    };

    unique_connection(const QUIC_API_TABLE *tbl_,
                      ConnectionOpenParams openParams,
                      ConnectionStartParams startParams);
    unique_connection();
};

/*-------------MsQuic->Config open and load--------------*/
class unique_configuration
    : public detail::unique_handler2<
          decltype(QUIC_API_TABLE::ConfigurationOpen),
          decltype(QUIC_API_TABLE::ConfigurationClose),
          decltype(QUIC_API_TABLE::ConfigurationLoadCredential)
          /*No need to unload configuration*/> {
   public:
    struct ConfigurationOpenParams {
        HQUIC registration;
        const QUIC_BUFFER *const AlpnBuffers;
        uint32_t AlpnBufferCount = 1;
        const QUIC_SETTINGS *Settings;
        uint32_t SettingsSize;
        void *Context;
    };

    struct ConfigurationStartParams {
        const QUIC_CREDENTIAL_CONFIG *CredConfig;
    };

    unique_configuration(const QUIC_API_TABLE *tbl_,
                         ConfigurationOpenParams openParams,
                         ConfigurationStartParams startParams);
    unique_configuration();
};

};  // namespace rvn
