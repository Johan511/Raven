#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <moqt.hpp>
#include <protobuf_messages.hpp>
#include <serialization.hpp>
#include <utilities.hpp>

namespace rvn
{
MOQT& MOQT::set_regConfig(QUIC_REGISTRATION_CONFIG* regConfig_)
{
    regConfig = regConfig_;
    add_to_secondary_counter(SecondaryIndices::regConfig);
    return *this;
}

MOQT& MOQT::set_listenerCb(listener_cb_lamda_t listenerCb_)
{
    listener_cb_lamda = listenerCb_;
    add_to_secondary_counter(SecondaryIndices::listenerCb);
    return *this;
}

MOQT& MOQT::set_connectionCb(connection_cb_lamda_t connectionCb_)
{
    connection_cb_lamda = connectionCb_;
    add_to_secondary_counter(SecondaryIndices::connectionCb);
    return *this;
}

// TOOD : check const corectness here
MOQT& MOQT::set_AlpnBuffers(QUIC_BUFFER* AlpnBuffers_)
{
    AlpnBuffers = AlpnBuffers_;
    add_to_secondary_counter(SecondaryIndices::AlpnBuffers);
    return *this;
}

MOQT& MOQT::set_AlpnBufferCount(uint32_t AlpnBufferCount_)
{
    AlpnBufferCount = AlpnBufferCount_;
    add_to_secondary_counter(SecondaryIndices::AlpnBufferCount);
    return *this;
}

// sets settings and setting size
MOQT& MOQT::set_Settings(QUIC_SETTINGS* Settings_, uint32_t SettingsSize_)
{
    utils->check_setting_assertions(Settings_, SettingsSize_);

    Settings = Settings_;
    SettingsSize = SettingsSize_;
    add_to_secondary_counter(SecondaryIndices::Settings);
    return *this;
}

MOQT& MOQT::set_CredConfig(QUIC_CREDENTIAL_CONFIG* CredConfig_)
{
    CredConfig = CredConfig_;
    add_to_secondary_counter(SecondaryIndices::CredConfig);
    return *this;
}

MOQT& MOQT::set_controlStreamCb(stream_cb_lamda_t controlStreamCb_)
{
    control_stream_cb_lamda = controlStreamCb_;
    add_to_secondary_counter(SecondaryIndices::controlStreamCb);
    return *this;
}

MOQT& MOQT::set_dataStreamCb(stream_cb_lamda_t dataStreamCb_)
{
    data_stream_cb_lamda = dataStreamCb_;
    add_to_secondary_counter(SecondaryIndices::dataStreamCb);
    return *this;
}

MOQT::MOQT() : tbl(rvn::make_unique_quic_table())
{
    secondaryCounter = 0;
    version = 0;
}

const QUIC_API_TABLE* MOQT::get_tbl()
{
    return tbl.get();
}

} // namespace rvn
