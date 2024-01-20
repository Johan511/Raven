#include <msquic.h>

#include <functional>
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
          listener(std::move(listener_)) {}
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

    /*const*/ QUIC_CREDENTIAL_CONFIG* CredConfig;

    std::uint8_t secondaryCounter;

    enum class SecondaryIndices {
        regConfig,
        listenerCb,
        context,
        AlpnBuffers,
        AlpnBufferCount,
        LocalAddress,
        Settings,
        CredConfig,
        secondaryCounter
    };

    // ISSUE : listener cb needs tbl object
    std::function<QUIC_STATUS(HQUIC, void*,
                              QUIC_LISTENER_EVENT*)> const
        default_listener_cb = [&](HQUIC Listener, void* Context,
                                  QUIC_LISTENER_EVENT* Event) {
            QUIC_STATUS Status = QUIC_STATUS_NOT_SUPPORTED;
            switch (Event->Type) {
                case QUIC_LISTENER_EVENT_NEW_CONNECTION:
                    tbl->SetCallbackHandler(
                        Event->NEW_CONNECTION.Connection,
                        (void*)ServerConnectionCallback, NULL);
                    Status = tbl->ConnectionSetConfiguration(
                        Event->NEW_CONNECTION.Connection,
                        Configuration);
                    break;
                default:
                    break;
            }
            return Status;
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

    MOQTFactory& set_CredConfig(
        /*const*/ QUIC_CREDENTIAL_CONFIG* CredConfig_) {
        CredConfig = CredConfig_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::CredConfig);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    std::unique_ptr<MOQT> get() {
        return std::make_unique<MOQT>(
            std::move(tbl),
            rvn::unique_registration(tbl.get(), regConfig),
            rvn::unique_configuration(
                tbl.get(),
                {reg.get(), AlpnBuffers, AlpnBufferCount,
                 Settings, SettingsSize, context},
                {CredConfig}),
            rvn::unique_listener(
                tbl.get(), {reg.get(), listenerCb, context},
                {AlpnBuffers, AlpnBufferCount, LocalAddress}));
    }
};
