#pragma once
////////////////////////////////////////////
#include <msquic.h>
////////////////////////////////////////////
#include <cstdint>
#include <functional>
////////////////////////////////////////////
#include <contexts.hpp>
#include <protobuf_messages.hpp>
#include <serialization/serialization.hpp>
#include <subscription_manager.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>
////////////////////////////////////////////

namespace rvn
{

enum class HostType
{
    CLIENT,
    SERVER
};

class MOQT
{

public:
    using listener_cb_lamda_t =
    std::function<QUIC_STATUS(HQUIC, void*, QUIC_LISTENER_EVENT*)>;
    using connection_cb_lamda_t =
    std::function<QUIC_STATUS(HQUIC, void*, QUIC_CONNECTION_EVENT*)>;
    using stream_cb_lamda_t =
    std::function<QUIC_STATUS(HQUIC, void*, QUIC_STREAM_EVENT*)>;

    enum class SecondaryIndices
    {
        regConfig,
        listenerCb,
        connectionCb,
        AlpnBuffers,
        AlpnBufferCount,
        Settings,
        CredConfig,
        controlStreamCb,
        dataStreamCb
    };

    static constexpr std::uint64_t sec_index_to_val(SecondaryIndices idx)
    {
        auto intVal = rvn::utils::to_underlying(idx);

        return (1 << intVal);
    }

    HostType hostType_;

    std::uint32_t version;

    // PIMPL pattern for moqt related utilities
    MOQTUtilities* utils;

    // need to be able to get function pointor of this
    // function hence can not be member function

    rvn::unique_QUIC_API_TABLE tbl;
    rvn::unique_registration reg;
    rvn::unique_configuration configuration;

    // secondary variables => build into primary
    QUIC_REGISTRATION_CONFIG* regConfig;
    // Server will use listener and client will use connection
    listener_cb_lamda_t listener_cb_lamda;
    connection_cb_lamda_t connection_cb_lamda;
    stream_cb_lamda_t control_stream_cb_lamda;
    stream_cb_lamda_t data_stream_cb_lamda;
    QUIC_BUFFER* AlpnBuffers;
    uint32_t AlpnBufferCount;
    QUIC_SETTINGS* Settings;
    uint32_t SettingsSize; // set along with Settings
    QUIC_CREDENTIAL_CONFIG* CredConfig;

    std::uint64_t secondaryCounter;

    void add_to_secondary_counter(SecondaryIndices idx)
    {
        secondaryCounter |= sec_index_to_val(idx);
    }

    constexpr std::uint64_t full_sec_counter_value()
    {
        std::uint64_t value = 0;

        value |= sec_index_to_val(SecondaryIndices::regConfig);
        value |= sec_index_to_val(SecondaryIndices::listenerCb);
        value |= sec_index_to_val(SecondaryIndices::connectionCb);
        value |= sec_index_to_val(SecondaryIndices::AlpnBuffers);
        value |= sec_index_to_val(SecondaryIndices::AlpnBufferCount);
        value |= sec_index_to_val(SecondaryIndices::Settings);
        value |= sec_index_to_val(SecondaryIndices::CredConfig);
        value |= sec_index_to_val(SecondaryIndices::controlStreamCb);
        value |= sec_index_to_val(SecondaryIndices::dataStreamCb);

        return value;
    }

    // callback wrappers
    //////////////////////////////////////////////////////////////////////////
    static QUIC_STATUS
    listener_cb_wrapper(HQUIC reg, void* context, QUIC_LISTENER_EVENT* event)
    {
        MOQT* thisObject = static_cast<MOQT*>(context);
        return thisObject->listener_cb_lamda(reg, context, event);
    }

    static QUIC_STATUS
    connection_cb_wrapper(HQUIC reg, void* context, QUIC_CONNECTION_EVENT* event)
    {
        MOQT* thisObject = static_cast<MOQT*>(context);
        return thisObject->connection_cb_lamda(reg, context, event);
    }

    static QUIC_STATUS
    control_stream_cb_wrapper(HQUIC stream, void* context, QUIC_STREAM_EVENT* event)
    {
        StreamContext* thisObject = static_cast<StreamContext*>(context);
        return thisObject->moqtObject->control_stream_cb_lamda(stream, context, event);
    }

    static QUIC_STATUS
    data_stream_cb_wrapper(HQUIC stream, void* context, QUIC_STREAM_EVENT* event)
    {
        StreamContext* thisObject = static_cast<StreamContext*>(context);
        return thisObject->moqtObject->data_stream_cb_lamda(stream, context, event);
    }

    // Setters
    //////////////////////////////////////////////////////////////////////////
    MOQT& set_regConfig(QUIC_REGISTRATION_CONFIG* regConfig_);
    MOQT& set_listenerCb(listener_cb_lamda_t listenerCb_);
    MOQT& set_connectionCb(connection_cb_lamda_t connectionCb_);

    // check  corectness here
    MOQT& set_AlpnBuffers(QUIC_BUFFER* AlpnBuffers_);

    MOQT& set_AlpnBufferCount(uint32_t AlpnBufferCount_);

    // sets settings and setting size
    MOQT& set_Settings(QUIC_SETTINGS* Settings_, uint32_t SettingsSize_);

    MOQT& set_CredConfig(QUIC_CREDENTIAL_CONFIG* CredConfig_);

    MOQT& set_controlStreamCb(stream_cb_lamda_t controlStreamCb_);
    MOQT& set_dataStreamCb(stream_cb_lamda_t dataStreamCb_);
    //////////////////////////////////////////////////////////////////////////

    const QUIC_API_TABLE* get_tbl();

    ConnectionState* get_connectionState(HQUIC connectionHandle);
    StreamState* get_stream_state(HQUIC connectionHandle, HQUIC streamHandle);


    void handle_message(ConnectionState& connectionState, HQUIC streamHandle, const auto* receiveInformation)
    {
        utils::ASSERT_LOG_THROW(connectionState.get_control_stream().has_value(),
                                "Trying to interpret control message without "
                                "control stream");

        const QUIC_BUFFER* buffers = receiveInformation->Buffers;

        std::stringstream iStringStream(
        std::string(reinterpret_cast<const char*>(buffers[0].Buffer), buffers[0].Length));

        return handle_message(connectionState, streamHandle, iStringStream);
    }

    void handle_message(ConnectionState& connectionState,
                        HQUIC streamHandle,
                        std::stringstream& iStringStream);


public:
    // auto is used as parameter because there is no named type for receive
    // information it is an anonymous structure
    /*
        type of recieve information is
        struct {
            uint64_t AbsoluteOffset;
            uint64_t TotalBufferLength;
            _Field_size_(BufferCount)
            const QUIC_BUFFER* Buffers;
            _Field_range_(0, UINT32_MAX)
            uint32_t BufferCount;
            QUIC_RECEIVE_FLAGS Flags;
        }
    */

protected:
    MOQT(HostType hostType);

    struct create_stream_params
    {
        class OpenParams
        {
        public:
            HQUIC connectionHandle;
            QUIC_STREAM_OPEN_FLAGS openFlags;
            QUIC_STREAM_CALLBACK_HANDLER streamCb;
        };

        class StartParams
        {
        public:
            QUIC_STREAM_START_FLAGS startFlags;
        };
    };


public:
    ~MOQT()
    {
        google::protobuf::ShutdownProtobufLibrary();
    }
};

} // namespace rvn

#include <moqt_client.hpp>
#include <moqt_server.hpp>
