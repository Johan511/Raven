#include <msquic.h>

#include <functional>
#include <utilities.hpp>
#include <wrappers.hpp>

struct StreamContext {
    MOQT* moqtObject;
    HQUIC connection;
    StreamContext(MOQT* moqtObject_, HQUIC connection_)
        : moqtObject(moqtObject_), connection(connection_){};
};

class MOQT {
    using listener_cb_lamda_t = std::function<QUIC_STATUS(
        HQUIC, void*, QUIC_LISTENER_EVENT*)>;
    using stream_cb_lamda_t = std::function<QUIC_STATUS(
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

    stream_cb_lamda_t control_stream_cb_lamda;

    stream_cb_lamda_t data_stream_cb_lamda;

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

    constexpr std::uint64_t full_sec_counter_value() {
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

    static QUIC_STATUS control_stream_cb_wrapper(
        HQUIC stream, void* context, QUIC_STREAM_EVENT* event) {
        MOQT* thisObject = static_cast<StreamContext*>(context);
        return thisObject->control_stream_cb_lamda(
            stream, context, event);
    }

    static QUIC_STATUS data_stream_cb_wrapper(
        HQUIC stream, void* context, QUIC_STREAM_EVENT* event) {
        MOQT* thisObject = static_cast<StreamContext*>(context);
        return thisObject->data_stream_cb_lamda(stream, context,
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

auto DataStreamCallBack = [](HQUIC Stream, void* Context,
                             QUIC_STREAM_EVENT* Event) {
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_SEND_COMPLETE:
            free(Event->SEND_COMPLETE.ClientContext);
            break;
        case QUIC_STREAM_EVENT_RECEIVE:
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
            MsQuic->StreamShutdown(
                Stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
            break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            MsQuic->StreamClose(Stream);
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
};

auto ControlStreamCallback = [](HQUIC Stream, void* Context,
                                QUIC_STREAM_EVENT* Event) {
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_SEND_COMPLETE:
            free(Event->SEND_COMPLETE.ClientContext);
            break;
        case QUIC_STREAM_EVENT_RECEIVE:
            // verify subscriber
            StreamContext* context =
                static_cast<StreamContext*>(Context);
            HQUIC dataStream = NULL;
            MsQuic->StreamOpen(
                context->connection,
                QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
                (void*)MOQT::data_stream_cb_wrapper, Context,
                &dataStream);

            // TODO : check flags
            MsQuic->StreamStart(dataStream,
                                QUIC_STREAM_START_FLAG_NONE);
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            ServerSend(Stream);
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
            MsQuic->StreamShutdown(
                Stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
            break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            /*Should also shutdown remaining streams
              because it releases ownership of Context
             */
            free(Context);
            MsQuic->StreamClose(Stream);
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
};

auto ServerConnectionCallback = [](HQUIC Connection,
                                   void* Context,
                                   QUIC_CONNECTION_EVENT*
                                       Event) {
    switch (Event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            MsQuic->ConnectionSendResumptionTicket(
                Connection, QUIC_SEND_RESUMPTION_FLAG_NONE, 0,
                NULL);
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
            if (Event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status ==
                QUIC_STATUS_CONNECTION_IDLE) {
            } else {
            }
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            MsQuic->ConnectionClose(Connection);
            break;
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
            /*
               Should receive bidirectional stream from user and
               then start transport of media
            */

            StreamContext* StreamContext = new StreamContext(
                static_cast<MOQT*>(Context), Connection);

            MsQuic->SetCallbackHandler(
                Event->PEER_STREAM_STARTED.Stream,
                (void*)MOQT::control_stream_cb_wrapper,
                StreamContext);
            break;
        case QUIC_CONNECTION_EVENT_RESUMED:
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
};
