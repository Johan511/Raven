#include <wrappers.hpp>

namespace rvn {

/*-----------------QUIC_API_TABLE------------------------*/

// unique_QUIC_API_TABLE::unique_QUIC_API_TABLE(
//     const QUIC_API_TABLE *tbl)
//     : QUIC_API_TABLE_uptr_t(tbl, QUIC_API_TABLE_deleter) {}

/*----------------MsQuic->RegistrationOpen---------------*/

unique_registration::unique_registration(const QUIC_API_TABLE *tbl_,
                                         const QUIC_REGISTRATION_CONFIG *RegConfig)
    : unique_handler1(tbl_->RegistrationOpen, tbl_->RegistrationClose) {
    if (QUIC_FAILED(construct(RegConfig)))
        throw std::runtime_error("RegistrationHandlerConstructionFailure");
};

unique_registration::unique_registration() : unique_handler1() {}

/*------------MsQuic->ListenerOpen and Start-------------*/
unique_listener::unique_listener(const QUIC_API_TABLE *tbl_, ListenerOpenParams openParams,
                                 ListenerStartParams startParams)
    : unique_handler2(tbl_->ListenerOpen, tbl_->ListenerClose, tbl_->ListenerStart,
                      tbl_->ListenerStop) {
    if (QUIC_FAILED(
            open_handler(openParams.registration, openParams.listenerCb, openParams.context)))
        throw std::runtime_error("ListenerOpenFailure");

    if (QUIC_FAILED(start_handler(startParams.AlpnBuffers, startParams.AlpnBufferCount,
                                  startParams.LocalAddress)))
        throw std::runtime_error("ListenerStartFailure");
};

unique_listener::unique_listener() : unique_handler2() {}

/*------------MsQuic->ConnectionOpen and Start-------------*/
unique_connection::unique_connection(const QUIC_API_TABLE *tbl_, ConnectionOpenParams openParams,
                                     ConnectionStartParams startParams)
    : unique_handler2(tbl_->ConnectionOpen, tbl_->ConnectionClose, tbl_->ConnectionStart) {
    if (QUIC_FAILED(
            open_handler(openParams.registration, openParams.connectionCb, openParams.context)))
        throw std::runtime_error("ConnectionOpenFailure");

    if (QUIC_FAILED(start_handler(startParams.Configuration, startParams.Family,
                                  startParams.ServerName, startParams.ServerPort)))
        throw std::runtime_error("ConnectionStartFailure");
};

unique_connection::unique_connection() : unique_handler2() {}

/*-------------MsQuic->Config open and load--------------*/

unique_configuration::unique_configuration(const QUIC_API_TABLE *tbl_,
                                           ConfigurationOpenParams openParams,
                                           ConfigurationStartParams startParams)
    : unique_handler2(tbl_->ConfigurationOpen, tbl_->ConfigurationClose,
                      tbl_->ConfigurationLoadCredential) {
    if (QUIC_FAILED(open_handler(openParams.registration, openParams.AlpnBuffers,
                                 openParams.AlpnBufferCount, openParams.Settings,
                                 openParams.SettingsSize, openParams.Context)))
        throw std::runtime_error("ConfigurationOpenFailure");

    // Start handler loads credentials into configuration
    if (QUIC_FAILED(start_handler(startParams.CredConfig)))
        throw std::runtime_error("ConfigurationLoadCredentialsFailure");
};

unique_configuration::unique_configuration() : unique_handler2() {}

/*-------------MsQuic->Stream open and start--------------*/

unique_stream::unique_stream(const QUIC_API_TABLE *tbl_, StreamOpenParams openParams,
                             StreamStartParams startParams)
    : unique_handler2(tbl_->StreamOpen, tbl_->StreamClose, tbl_->StreamStart
                      /*tbl_->StreamShutdown*/) { // we do not call shutdown on stream, so causing possible issues
    if (QUIC_FAILED(open_handler(openParams.Connection, openParams.Flags, openParams.Handler,
                                 openParams.Context)))
        throw std::runtime_error("StreamOpenFailure");

    if (QUIC_FAILED(start_handler(startParams.Flags)))
        throw std::runtime_error("StreamStartFailure");
};

unique_stream::unique_stream() : unique_handler2() {}

/*-------------------------------------------------------*/
}; // namespace rvn
