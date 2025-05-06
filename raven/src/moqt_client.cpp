#include <atomic>
#include <contexts.hpp>
#include <data_manager.hpp>
#include <moqt.hpp>
#include <msquic.h>
#include <serialization/messages.hpp>
#include <stdexcept>
#include <utilities.hpp>
#include <wrappers.hpp>

namespace rvn
{
MOQTClient::MOQTClient(std::tuple<QUIC_EXECUTION_CONFIG*, std::uint64_t> execConfigTuple)
: MOQT(HostType::CLIENT)
{
    auto [execConfig, execConfigLen] = execConfigTuple;
    QUIC_STATUS status = tbl->SetParam(nullptr, QUIC_PARAM_GLOBAL_EXECUTION_CONFIG,
                                       execConfigLen, execConfig);
    if (QUIC_FAILED(status))
        throw std::runtime_error("Could not set execution config");
};

void MOQTClient::start_connection(QUIC_ADDRESS_FAMILY Family, const char* ServerName, uint16_t ServerPort)
{
    rvn::utils::ASSERT_LOG_THROW(secondaryCounter == full_sec_counter_value(),
                                 "secondaryCounter ", secondaryCounter,
                                 " full_sec_counter_value() ", full_sec_counter_value());

    reg = rvn::unique_registration(tbl.get(), regConfig);
    configuration = rvn::unique_configuration(tbl.get(),
                                              { reg.get(), AlpnBuffers, AlpnBufferCount,
                                                Settings, SettingsSize, this },
                                              { CredConfig });

    // enable critical section
    //            => no RAII because if emplace fails, we don't want connections
    //            to be accepted and fault elsewhere
    auto connection =
    rvn::unique_connection(tbl.get(), { reg.get(), MOQT::connection_cb_wrapper, this },
                           { configuration.get(), Family, ServerName, ServerPort });

    // connection state is optional
    connectionState = std::make_shared<ConnectionState>(std::move(connection), *this);

    quicConnectionStateSetupFlag_.store(true, std::memory_order_release);

    utils::wait_for(ravenConnectionSetupFlag_);
    utils::LOG_EVENT(std::cout, "Client QUIC setup complete");
}

ClientSetupMessage MOQTClient::get_clientSetupMessage()
{
    ClientSetupMessage clientSetupMessage;
    clientSetupMessage.supportedVersions_ = { 1 };

    // TODO: set role and path parameters
    return clientSetupMessage;
}

} // namespace rvn
