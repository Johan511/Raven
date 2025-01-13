#include <memory>
#include <moqt.hpp>
#include <protobuf_messages.hpp>
#include <serialization/serialization.hpp>
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

MOQT::MOQT(HostType hostType)
: hostType_(hostType), tbl(rvn::make_unique_quic_table())
{
    secondaryCounter = 0;
    version = 0;
}

const QUIC_API_TABLE* MOQT::get_tbl()
{
    return tbl.get();
}

ConnectionState* MOQT::get_connectionState(HQUIC connectionHandle)
{
    if (hostType_ == HostType::CLIENT)
    {
        MOQTClient* thisClient = static_cast<MOQTClient*>(this);
        utils::ASSERT_LOG_THROW(connectionHandle ==
                                thisClient->connectionState->connection_.get(),
                                "Connection handle does not match");

        return std::addressof(*thisClient->connectionState);
    }
    else
    {
        MOQTServer* thisServer = static_cast<MOQTServer*>(this);
        auto iter = thisServer->connectionStateMap.find(connectionHandle);
        if (iter == thisServer->connectionStateMap.end())
            return nullptr;
        return std::addressof(iter->second);
    }
}
StreamState* MOQT::get_stream_state(HQUIC connectionHandle, HQUIC streamHandle)
{
    if (hostType_ == HostType::CLIENT)
    {
        MOQTClient* thisClient = static_cast<MOQTClient*>(this);
        return thisClient->get_stream_state(connectionHandle, streamHandle);
    }
    else
    {
        MOQTServer* thisServer = static_cast<MOQTServer*>(this);
        return thisServer->get_stream_state(connectionHandle, streamHandle);
    }
}

void MOQT::handle_message(ConnectionState& connectionState,
                          HQUIC streamHandle,
                          std::stringstream& iStringStream)
{
    if (hostType_ == HostType::SERVER)
    {
        MOQTServer* thisServer = static_cast<MOQTServer*>(this);
        return thisServer->handle_message_internal(connectionState, streamHandle, iStringStream);
    }
    else
    {
        MOQTClient* thisClient = static_cast<MOQTClient*>(this);
        return thisClient->handle_message_internal(connectionState, streamHandle, iStringStream);
    }
}

} // namespace rvn
