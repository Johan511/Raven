#include <msquic.h>

#include <functional>
#include <utilities.hpp>
#include <wrappers.hpp>

class MOQT {
    using listener_cb_lamda_t = std::function<QUIC_STATUS(
        HQUIC, void*, QUIC_LISTENER_EVENT*)>;

    // primary variables => build into MOQT object
    rvn::unique_QUIC_API_TABLE tbl;
    rvn::unique_registration reg;
    rvn::unique_configuration configuration;
    rvn::unique_listener listener;

    std::uint8_t primaryCounter;

    // secondary variables => build into primary
    QUIC_REGISTRATION_CONFIG* regConfig;

    listener_cb_lamda_t listener_cb_lamda;

    QUIC_BUFFER* AlpnBuffers;

    uint32_t AlpnBufferCount;

    QUIC_ADDR* LocalAddress;

    QUIC_SETTINGS* Settings;
    uint32_t SettingsSize;  // set along with Settings

    QUIC_CREDENTIAL_CONFIG* CredConfig;

    std::uint64_t secondaryCounter;

    enum class SecondaryIndices {
        regConfig,
        listenerCb,
        AlpnBuffers,
        AlpnBufferCount,
        LocalAddress,
        Settings,
        CredConfig,
    };

    static std::uint64_t sec_index_to_val(SecondaryIndices idx) {
        auto intVal = rvn::utilities::to_underlying(idx);

        return (1 << intVal);
    }

    std::uint64_t full_sec_counter_value() {
        std::uint64_t value = 0;

        value |= sec_index_to_val(SecondaryIndices::regConfig);
        value |= sec_index_to_val(SecondaryIndices::listenerCb);
        value |= sec_index_to_val(SecondaryIndices::AlpnBuffers);
        value |=
            sec_index_to_val(SecondaryIndices::AlpnBufferCount);
        value |=
            sec_index_to_val(SecondaryIndices::LocalAddress);
        value |= sec_index_to_val(SecondaryIndices::Settings);
        value |= sec_index_to_val(SecondaryIndices::CredConfig);

        return value;
    }

    // need to be able to get function pointor of this
    // function hence can not be member function
    static QUIC_STATUS listener_cb_wrapper(
        HQUIC reg, void* context, QUIC_LISTENER_EVENT* event) {
        MOQT* thisObject = static_cast<MOQT*>(context);
        return thisObject->listener_cb_lamda(reg, context,
                                             event);
    }

   public:
    MOQT() : tbl(rvn::make_unique_quic_table()) {
        primaryCounter = 0;
        secondaryCounter = 0;
    }

    MOQT& set_regConfig(QUIC_REGISTRATION_CONFIG* regConfig_) {
        regConfig = regConfig_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::regConfig);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    MOQT& set_listenerCb(listener_cb_lamda_t listenerCb_) {
        listener_cb_lamda = listenerCb_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::listenerCb);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    // check  corectness here
    MOQT& set_AlpnBuffers(QUIC_BUFFER* AlpnBuffers_) {
        AlpnBuffers = AlpnBuffers_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::AlpnBuffers);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    MOQT& set_AlpnBufferCount(uint32_t AlpnBufferCount_) {
        AlpnBufferCount = AlpnBufferCount_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::AlpnBufferCount);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    MOQT& set_LocalAddress(QUIC_ADDR* LocalAddress_) {
        LocalAddress = LocalAddress_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::LocalAddress);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    // sets settings and setting size
    MOQT& set_Settings(QUIC_SETTINGS* Settings_,
                       uint32_t SettingsSize_) {
        Settings = Settings_;
        SettingsSize = SettingsSize_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::Settings);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    MOQT& set_CredConfig(QUIC_CREDENTIAL_CONFIG* CredConfig_) {
        CredConfig = CredConfig_;

        auto idx = rvn::utilities::to_underlying(
            SecondaryIndices::CredConfig);

        secondaryCounter |= (1 << idx);

        return *this;
    }

    void start_listener() {
        assert(secondaryCounter == full_sec_counter_value());
        reg = rvn::unique_registration(tbl.get(), regConfig);
        configuration = rvn::unique_configuration(
            tbl.get(),
            {reg.get(), AlpnBuffers, AlpnBufferCount, Settings,
             SettingsSize, this},
            {CredConfig});
        listener = rvn::unique_listener(
            tbl.get(),
            {reg.get(), MOQT::listener_cb_wrapper, this},
            {AlpnBuffers, AlpnBufferCount, LocalAddress});
    }
};
