#include <atomic>
#include <msquic.h>

#include <functional>
#include <moqt.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>

namespace rvn
{
MOQTClient::MOQTClient() : MOQT() {};

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

    connectionSetupFlag.store(true, std::memory_order_release);
    
    // enable critical section
    //            => no RAII because if emplace fails, we don't want connections to be accepted and fault elsewhere
    connection =
    rvn::unique_connection(tbl.get(), { reg.get(), MOQT::connection_cb_wrapper, this },
                           { configuration.get(), Family, ServerName, ServerPort });


    connectionStateMap.emplace(connection.get(), ConnectionState{ connection.get(), this });

    connectionSetupFlag.store(false, std::memory_order_release);
}

protobuf_messages::ClientSetupMessage MOQTClient::get_clientSetupMessage()
{
    protobuf_messages::ClientSetupMessage clientSetupMessage;
    clientSetupMessage.set_numsupportedversions(1);
    clientSetupMessage.add_supportedversions(version);
    clientSetupMessage.add_numberofparameters(1);
    auto* param1 = clientSetupMessage.add_parameters();
    param1->mutable_path()->set_path("path");
    param1->mutable_role()->set_role(protobuf_messages::Role::Subscriber);

    return clientSetupMessage;
}

} // namespace rvn
