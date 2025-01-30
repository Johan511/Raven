#include "serialization/messages.hpp"
#include <atomic>
#include <msquic.h>

#include <functional>
#include <moqt.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>

namespace rvn
{
MOQTClient::MOQTClient() : MOQT(HostType::CLIENT) {};

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
    //            => no RAII because if emplace fails, we don't want connections to be accepted and fault elsewhere
    auto connection =
    rvn::unique_connection(tbl.get(), { reg.get(), MOQT::connection_cb_wrapper, this },
                           { configuration.get(), Family, ServerName, ServerPort });


    // connection state is optional
    connectionState.emplace(std::move(connection), *this);

    quicConnectionStateSetupFlag_.store(true, std::memory_order_release);

    utils::wait_for(ravenConnectionSetupFlag_);
    utils::LOG_EVENT(std::cout, "Client QUIC setup complete");
}

depracated::messages::ClientSetupMessage MOQTClient::get_clientSetupMessage()
{
    depracated::messages::ClientSetupMessage clientSetupMessage;
    clientSetupMessage.supportedVersions_ = { 1 };

    // TODO: set role and path parameters
    return clientSetupMessage;
}

} // namespace rvn
