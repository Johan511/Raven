#include <moqt.hpp>

constexpr std::uint64_t MOQT::full_sec_counter_value() {
    std::uint64_t value = 0;

    value |= sec_index_to_val(SecondaryIndices::regConfig);
    value |= sec_index_to_val(SecondaryIndices::listenerCb);
    value |= sec_index_to_val(SecondaryIndices::AlpnBuffers);
    value |= sec_index_to_val(SecondaryIndices::AlpnBufferCount);
    value |= sec_index_to_val(SecondaryIndices::Settings);
    value |= sec_index_to_val(SecondaryIndices::CredConfig);

    return value;
}

MOQT& MOQT::set_regConfig(QUIC_REGISTRATION_CONFIG* regConfig_) {
    regConfig = regConfig_;

    auto idx = rvn::utilities::to_underlying(
        SecondaryIndices::regConfig);

    secondaryCounter |= (1 << idx);

    return *this;
}

MOQT& MOQT::set_listenerCb(listener_cb_lamda_t listenerCb_) {
    listener_cb_lamda = listenerCb_;

    auto idx = rvn::utilities::to_underlying(
        SecondaryIndices::listenerCb);

    secondaryCounter |= (1 << idx);

    return *this;
}

// check  corectness here
MOQT& MOQT::set_AlpnBuffers(QUIC_BUFFER* AlpnBuffers_) {
    AlpnBuffers = AlpnBuffers_;

    auto idx = rvn::utilities::to_underlying(
        SecondaryIndices::AlpnBuffers);

    secondaryCounter |= (1 << idx);

    return *this;
}

MOQT& MOQT::set_AlpnBufferCount(uint32_t AlpnBufferCount_) {
    AlpnBufferCount = AlpnBufferCount_;

    auto idx = rvn::utilities::to_underlying(
        SecondaryIndices::AlpnBufferCount);

    secondaryCounter |= (1 << idx);

    return *this;
}

// sets settings and setting size
MOQT& MOQT::set_Settings(QUIC_SETTINGS* Settings_,
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

MOQT::MOQT() : tbl(rvn::make_unique_quic_table()) {
    secondaryCounter = 0;
}
