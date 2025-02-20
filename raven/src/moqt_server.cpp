#include "subscription_manager.hpp"
#include <contexts.hpp>
#include <moqt.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>

namespace rvn
{

MOQTServer::MOQTServer(std::shared_ptr<DataManager> dataManager)
: MOQT(HostType::SERVER), dataManager_(dataManager),
  subscriptionManager_(std::make_shared<SubscriptionManager>(*dataManager_)) {};

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
