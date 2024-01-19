#include <msquic.h>

#include <utilities.hpp>
#include <wrappers.hpp>

class MOQT {
    rvn::unique_QUIC_API_TABLE tbl;
    rvn::unique_registration reg;
    rvn::unique_configuration configuration;
    rvn::unique_listener listener;

   public:
    MOQT(rvn::unique_QUIC_API_TABLE&& tbl_,
         rvn::unique_registration&& reg_,
         rvn::unique_configuration&& configuration_,
         rvn::unique_listener&& listener_)
        : tbl(std::move(tbl_)),
          reg(std::move(reg_)),
          configuration(std::move(configuration_)),
          listener(std::move(listener_)) {
        return;
    }
};

class MOQTFactory {
    // primary variables => build into MOQT object
    rvn::unique_QUIC_API_TABLE tbl;
    rvn::unique_registration reg;
    rvn::unique_configuration configuration;
    rvn::unique_listener listener;

    std::uint8_t primaryCounter;

    // secondary variables => build into primary
    /*const*/ QUIC_REGISTRATION_CONFIG* regConfig;

    QUIC_LISTENER_CALLBACK_HANDLER listenerCb;

    void* context;

    /*const*/ QUIC_BUFFER* /*const*/ AlpnBuffers;

    uint32_t AlpnBufferCount;

    /*const*/ QUIC_ADDR* LocalAddress;

    /*const*/ QUIC_SETTINGS* Settings;
    uint32_t SettingsSize;  // set along with Settings

    /*const*/ QUIC_CREDENTIAL_CONFIG* configCred;

    std::uint8_t secondaryCounter;

    enum class SecondaryIndices {
        regConfig,
        listenerCb,
        context,
        AlpnBuffers,
        AlpnBufferCount,
        LocalAddress,
        Settings,
        configCred,
        secondaryCounter
    };

   public:
    MOQTFactory() : tbl(rvn::make_unique_quic_table()) {
        primaryCounter = 0;
        secondaryCounter = 0;
    }

    MOQTFactory& set_regConfig(
        /*const*/ QUIC_REGISTRATION_CONFIG* regConfig_) {
        regConfig = regConfig_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::regConfig);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    MOQTFactory& set_listenerCb(
        QUIC_LISTENER_CALLBACK_HANDLER listenerCb_) {
        listenerCb = listenerCb_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::listenerCb);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    MOQTFactory& set_context(void* context_) {
        context = context_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::context);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    // check /*const*/ corectness here
    MOQTFactory& set_AlpnBuffers(
        /*const*/ QUIC_BUFFER* /*const*/ AlpnBuffers_) {
        AlpnBuffers = AlpnBuffers_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::AlpnBuffers);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    MOQTFactory& set_AlpnBufferCount(uint32_t AlpnBufferCount_) {
        AlpnBufferCount = AlpnBufferCount_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::AlpnBufferCount);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    MOQTFactory& set_LocalAddress(QUIC_ADDR* LocalAddress_) {
        LocalAddress = LocalAddress_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::LocalAddress);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    // sets settings and setting size
    MOQTFactory& set_Settings(/*const*/ QUIC_SETTINGS* Settings_,
                              uint32_t SettingsSize_) {
        Settings = Settings_;
        SettingsSize = SettingsSize_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::Settings);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    MOQTFactory& set_configCred(
        /*const*/ QUIC_CREDENTIAL_CONFIG* configCred_) {
        configCred = configCred_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::configCred);

        secondaryCounter |= (1 << idx);

        return *this;
    }
};
