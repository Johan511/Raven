#include "subscription_manager.hpp"
#include <contexts.hpp>
#include <moqt.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>

namespace rvn
{

MOQTServer::MOQTServer(std::shared_ptr<DataManager> dataManager)
: MOQT(HostType::SERVER), dataManager_(dataManager),
  subscriptionManager_(std::make_shared<SubscriptionManager>(*dataManager_))
{
    std::uint8_t rawExecConfig[QUIC_EXECUTION_CONFIG_MIN_SIZE + 6 * sizeof(uint16_t)] = { 0 };
    QUIC_EXECUTION_CONFIG* execConfig = (QUIC_EXECUTION_CONFIG*)rawExecConfig;
    execConfig->ProcessorCount = 6;
    execConfig->ProcessorList[0] = 0;
    execConfig->ProcessorList[1] = 1;
    execConfig->ProcessorList[2] = 2;
    execConfig->ProcessorList[3] = 3;
    execConfig->ProcessorList[4] = 4;
    execConfig->ProcessorList[5] = 5;

    // QUIC_STATUS status = tbl->SetParam(nullptr, QUIC_PARAM_GLOBAL_EXECUTION_CONFIG,
    //                                    sizeof(rawExecConfig), execConfig);
    // if (QUIC_FAILED(status))
    //     exit(1);
};

void MOQTServer::start_listener(QUIC_ADDR* LocalAddress)
{
    rvn::utils::ASSERT_LOG_THROW(secondaryCounter == full_sec_counter_value(),
                                 "secondaryCounter ", secondaryCounter,
                                 " full_sec_counter_value() ", full_sec_counter_value());

    reg = rvn::unique_registration(tbl.get(), regConfig);
    configuration = rvn::unique_configuration(tbl.get(),
                                              { reg.get(), AlpnBuffers, AlpnBufferCount,
                                                Settings, SettingsSize, this },
                                              { CredConfig });
    listener =
    rvn::unique_listener(tbl.get(), { reg.get(), MOQT::listener_cb_wrapper, this },
                         { AlpnBuffers, AlpnBufferCount, LocalAddress });
}

} // namespace rvn
