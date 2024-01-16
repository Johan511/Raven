#include <msquic.h>

#include <memory>
#include <stdexcept>

namespace rvn {
/*-------------------------------------------------------*/

template <class T, class... Args>
std::unique_ptr<T> make_unique(Args &&...);

// To be used when only one construct function exists
template <typename Ctor, typename Dtor>
class unique_handler1 {
    // Pointer to QUIC_HANDLER owned by unique_handler1
    HQUIC handler;

    // non owning callable (function pointor)
    Ctor open;
    Dtor close;

   protected:
    unique_handler1(Ctor open_, Dtor close_) noexcept
        : handler(NULL) {
        open = open_;
        close = close_;
    }

    /*Don't take universal reference as this is C.
      Everything is copied

      If return is QUIC_FAILED then throw exception
    */
    template <typename... Args>
    QUIC_STATUS construct(Args... args) {
        return open(args..., &handler);
    }

    ~unique_handler1() noexcept {
        if (handler != NULL) close(handler);
    }

    HQUIC get() const { return handler; }
};

// To be used when only one construct function exists
template <typename Open, typename Close, typename Start,
          typename Stop>
class unique_handler2 {
    // Pointer to QUIC_HANDLER owned by unique_handler2
    HQUIC handler;

    // non owning callable (function pointor)
    Open open_func;
    Close close_func;

    Start start_func;
    Stop stop_func;

   protected:
    unique_handler2(Open open_, Close close_, Start start_,
                    Stop stop_) noexcept
        : handler(NULL) {
        open_func = open_;
        close_func = close_;

        start_func = start_;
        stop_func = stop_;
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

    ~unique_handler2() noexcept {
        stop_func(handler);
        close_func(handler);
    }

    HQUIC get() const { return handler; }
};

/*-----------------QUIC_API_TABLE------------------------*/
auto QUIC_API_TABLE_deleter = [](const QUIC_API_TABLE *tbl) {
    MsQuicClose(tbl);
};

using QUIC_API_TABLE_uptr_t =
    std::unique_ptr<const QUIC_API_TABLE,
                    decltype(QUIC_API_TABLE_deleter)>;

class unique_QUIC_API_TABLE : public QUIC_API_TABLE_uptr_t {
   public:
    unique_QUIC_API_TABLE(const QUIC_API_TABLE *tbl)
        : QUIC_API_TABLE_uptr_t(tbl, QUIC_API_TABLE_deleter) {}
};

unique_QUIC_API_TABLE make_unique() {
    const QUIC_API_TABLE *tbl;
    MsQuicOpen2(&tbl);
    return unique_QUIC_API_TABLE(tbl);
}

/*----------------MsQuic->RegistrationOpen---------------*/
class unique_registration
    : public unique_handler1<
          decltype(QUIC_API_TABLE::RegistrationOpen),
          decltype(QUIC_API_TABLE::RegistrationClose)> {
   public:
    unique_registration(
        const QUIC_API_TABLE *tbl_,
        const QUIC_REGISTRATION_CONFIG *RegConfig)
        : unique_handler1(tbl_->RegistrationOpen,
                          tbl_->RegistrationClose) {
        if (QUIC_FAILED(construct(RegConfig)))
            throw std::runtime_error(
                "RegistrationHandlerConstructionFailure");
    };
};
/*-------------------------------------------------------*/

/*------------MsQuic->ListenerOpen and Start-------------*/
class unique_listener
    : public unique_handler2<
          decltype(QUIC_API_TABLE::ListenerOpen),
          decltype(QUIC_API_TABLE::ListenerClose),
          decltype(QUIC_API_TABLE::ListenerStart),
          decltype(QUIC_API_TABLE::ListenerStop)> {
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

   public:
    unique_listener(const QUIC_API_TABLE *tbl_,
                    ListenerOpenParams openParams,
                    ListenerStartParams startParams)
        : unique_handler2(
              tbl_->ListenerOpen, tbl_->ListenerClose,
              tbl_->ListenerStart, tbl_->ListenerStop) {
        if (QUIC_FAILED(open_handler(openParams.registration,
                                     openParams.listenerCb,
                                     openParams.context)))
            throw std::runtime_error("ListenerOpenFailure");

        if (QUIC_FAILED(
                start_handler(startParams.AlpnBuffers,
                              startParams.AlpnBufferCount,
                              startParams.LocalAddress)))
            throw std::runtime_error("ListenerStartFailure");
    };
};
/*-------------------------------------------------------*/
};  // namespace rvn
