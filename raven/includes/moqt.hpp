#pragma once

#include <msquic.h>

#include <cstdint>
#include <utilities.hpp>
#include <wrappers.hpp>

struct StreamContext {
    class MOQT* moqtObject;
    HQUIC connection;
    StreamContext(MOQT* moqtObject_, HQUIC connection_)
        : moqtObject(moqtObject_), connection(connection_){};
};

class MOQT {
   protected:
    /*stream and listener cb differ by last argument
     * (quic event type)*/
    using listener_cb_lamda_t = std::function<QUIC_STATUS(
        HQUIC, void*, QUIC_LISTENER_EVENT*)>;

    using stream_cb_lamda_t = std::function<QUIC_STATUS(
        HQUIC, void*, QUIC_STREAM_EVENT*)>;

    enum class SecondaryIndices {
        regConfig,
        listenerCb,
        AlpnBuffers,
        AlpnBufferCount,
        Settings,
        CredConfig,
    };

    static constexpr std::uint64_t sec_index_to_val(
        SecondaryIndices idx) {
        auto intVal = rvn::utils::to_underlying(idx);

        return (1 << intVal);
    }

    // need to be able to get function pointor of this
    // function hence can not be member function
    static QUIC_STATUS listener_cb_wrapper(
        HQUIC reg, void* context, QUIC_LISTENER_EVENT* event) {
        MOQT* thisObject = static_cast<MOQT*>(context);
        return thisObject->listener_cb_lamda(reg, context,
                                             event);
    }

    static QUIC_STATUS control_stream_cb_wrapper(
        HQUIC stream, void* context, QUIC_STREAM_EVENT* event) {
        StreamContext* thisObject =
            static_cast<StreamContext*>(context);
        return thisObject->moqtObject->control_stream_cb_lamda(
            stream, context, event);
    }

    static QUIC_STATUS data_stream_cb_wrapper(
        HQUIC stream, void* context, QUIC_STREAM_EVENT* event) {
        StreamContext* thisObject =
            static_cast<StreamContext*>(context);
        return thisObject->moqtObject->data_stream_cb_lamda(
            stream, context, event);
    }

    rvn::unique_QUIC_API_TABLE tbl;
    rvn::unique_registration reg;
    rvn::unique_configuration configuration;

    // secondary variables => build into primary
    QUIC_REGISTRATION_CONFIG* regConfig;

    // placeholder for connection_cb in case of Client
    listener_cb_lamda_t listener_cb_lamda;

    stream_cb_lamda_t control_stream_cb_lamda;

    stream_cb_lamda_t data_stream_cb_lamda;

    QUIC_BUFFER* AlpnBuffers;

    uint32_t AlpnBufferCount;

    QUIC_SETTINGS* Settings;
    uint32_t SettingsSize;  // set along with Settings

    QUIC_CREDENTIAL_CONFIG* CredConfig;

    std::uint64_t secondaryCounter;

    /* constexpr functions are inline and need to be defined in
     * header*/
    constexpr std::uint64_t full_sec_counter_value() {
        std::uint64_t value = 0;

        value |= sec_index_to_val(SecondaryIndices::regConfig);
        value |= sec_index_to_val(SecondaryIndices::listenerCb);
        value |= sec_index_to_val(SecondaryIndices::AlpnBuffers);
        value |=
            sec_index_to_val(SecondaryIndices::AlpnBufferCount);
        value |= sec_index_to_val(SecondaryIndices::Settings);
        value |= sec_index_to_val(SecondaryIndices::CredConfig);

        return value;
    }

    MOQT();

   public:
    MOQT& set_regConfig(QUIC_REGISTRATION_CONFIG* regConfig_);
    MOQT& set_listenerCb(listener_cb_lamda_t listenerCb_);

    // check  corectness here
    MOQT& set_AlpnBuffers(QUIC_BUFFER* AlpnBuffers_);

    MOQT& set_AlpnBufferCount(uint32_t AlpnBufferCount_);

    // sets settings and setting size
    MOQT& set_Settings(QUIC_SETTINGS* Settings_,
                       uint32_t SettingsSize_);

    MOQT& set_CredConfig(QUIC_CREDENTIAL_CONFIG* CredConfig_);
};

class MOQTServer : public MOQT {
    rvn::unique_listener listener;

   public:
    MOQTServer();

    void start_listener(QUIC_ADDR* LocalAddress);
};

class MOQTClient : public MOQT {
    rvn::unique_connection connection;

   public:
    MOQTClient();

    void start_connection(QUIC_ADDRESS_FAMILY Family,
                          const char* ServerName,
                          uint16_t ServerPort);
};
