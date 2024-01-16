#include "../includes/wrappers.hpp"

namespace rvn {

/*-----------------QUIC_API_TABLE------------------------*/

unique_QUIC_API_TABLE::unique_QUIC_API_TABLE(
    const QUIC_API_TABLE *tbl)
    : QUIC_API_TABLE_uptr_t(tbl, QUIC_API_TABLE_deleter) {}

/*----------------MsQuic->RegistrationOpen---------------*/

unique_registration::unique_registration(
    const QUIC_API_TABLE *tbl_,
    const QUIC_REGISTRATION_CONFIG *RegConfig)
    : unique_handler1(tbl_->RegistrationOpen,
                      tbl_->RegistrationClose) {
    if (QUIC_FAILED(construct(RegConfig)))
        throw std::runtime_error(
            "RegistrationHandlerConstructionFailure");
};

/*------------MsQuic->ListenerOpen and Start-------------*/
unique_listener::unique_listener(const QUIC_API_TABLE *tbl_,
                                 ListenerOpenParams openParams,
                                 ListenerStartParams startParams)
    : unique_handler2(tbl_->ListenerOpen, tbl_->ListenerClose,
                      tbl_->ListenerStart, tbl_->ListenerStop) {
    if (QUIC_FAILED(open_handler(openParams.registration,
                                 openParams.listenerCb,
                                 openParams.context)))
        throw std::runtime_error("ListenerOpenFailure");

    if (QUIC_FAILED(start_handler(startParams.AlpnBuffers,
                                  startParams.AlpnBufferCount,
                                  startParams.LocalAddress)))
        throw std::runtime_error("ListenerStartFailure");
};

/*-------------MsQuic->Config open and load--------------*/

unique_configuration::unique_configuration(
    const QUIC_API_TABLE *tbl_,
    ConfigurationOpenParams openParams,
    ConfigurationStartParams startParams)
    : unique_handler2(tbl_->ConfigurationOpen,
                      tbl_->ConfigurationClose,
                      tbl_->ConfigurationLoadCredential) {
    if (QUIC_FAILED(open_handler(
            openParams.registration, openParams.AlpnBuffers,
            openParams.AlpnBufferCount, openParams.Settings,
            openParams.SettingsSize, openParams.Context)))
        throw std::runtime_error("ConfigurationOpenFailure");

    // Start handler loads credentials into configuration
    if (QUIC_FAILED(start_handler(startParams.CredConfig)))
        throw std::runtime_error(
            "ConfigurationLoadCredentialsFailure");
};
/*-------------------------------------------------------*/
};  // namespace rvn
