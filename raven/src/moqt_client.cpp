#include <atomic>
#include <contexts.hpp>
#include <moqt.hpp>
#include <serialization/messages.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>

namespace rvn
{
MOQTClient::MOQTClient() : MOQT(HostType::CLIENT)
{
    std::uint8_t rawExecConfig[QUIC_EXECUTION_CONFIG_MIN_SIZE + 2 * sizeof(uint16_t)] = { 0 };
    QUIC_EXECUTION_CONFIG* execConfig = (QUIC_EXECUTION_CONFIG*)rawExecConfig;
    execConfig->ProcessorCount = 2;
    execConfig->ProcessorList[0] = 2;
    execConfig->ProcessorList[1] = 3;

    QUIC_STATUS status = tbl->SetParam(nullptr, QUIC_PARAM_GLOBAL_EXECUTION_CONFIG,
                                       sizeof(rawExecConfig), execConfig);
    if (QUIC_FAILED(status))
        exit(1);
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
    //            => no RAII because if emplace fails, we don't want connections to be accepted and fault elsewhere
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
