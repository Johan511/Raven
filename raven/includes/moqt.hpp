#pragma once
////////////////////////////////////////////
#include <atomic>
#include <msquic.h>
////////////////////////////////////////////
#include <cstdint>
#include <fstream>
#include <functional>
#include <ostream>
#include <sstream>
#include <thread>
#include <unordered_map>
////////////////////////////////////////////
#include <contexts.hpp>
#include <message_handlers.hpp>
#include <moqt_utils.hpp>
#include <protobuf_messages.hpp>
#include <serialization.hpp>
#include <subscription_manager.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>
////////////////////////////////////////////

namespace rvn
{

// context used when data is send on stream

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

    std::uint32_t version;

    // PIMPL pattern for moqt related utilities
    MOQTUtilities* utils;

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // these functions will later be pushed into cgUtils
    // utils::MOQTComplexGetterUtils *cgUtils{this};

    StreamState* get_stream_state(HQUIC connectionHandle, HQUIC streamHandle)
    {
        ConnectionState& connectionState = connectionStateMap.at(connectionHandle);
        if (connectionState.get_control_stream().has_value() &&
            connectionState.get_control_stream().value().stream.get() == streamHandle)
        {
            return &connectionState.get_control_stream().value();
        }

        auto& dataStreams = connectionState.get_data_streams();

        auto streamIter =
        std::find_if(dataStreams.begin(), dataStreams.end(),
                     [streamHandle](const StreamState& streamState)
                     { return streamState.stream.get() == streamHandle; });

        if (streamIter != dataStreams.end())
        {
            return &(*streamIter);
        }

        return nullptr;
    }

    StreamState* get_stream_state(HQUIC streamHandle)
    {
        for (const auto& connectionStatePair : connectionStateMap)
        {
            HQUIC connectionHandle = connectionStatePair.first;
            ConnectionState& connectionState = connectionStateMap.at(connectionHandle);
            if (connectionState.get_control_stream().has_value() &&
                connectionState.get_control_stream().value().stream.get() == streamHandle)
            {
                return &connectionState.get_control_stream().value();
            }

            auto& dataStreams = connectionState.get_data_streams();

            auto streamIter =
            std::find_if(dataStreams.begin(), dataStreams.end(),
                         [streamHandle](const StreamState& streamState)
                         { return streamState.stream.get() == streamHandle; });

            if (streamIter != dataStreams.end())
            {
                return &(*streamIter);
            }
        }

        return nullptr;
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

    // map from connection HQUIC to connection state
    std::unordered_map<HQUIC, ConnectionState> connectionStateMap; // should
                                                                   // have size
                                                                   // 1 in case
                                                                   // of client
    std::unordered_map<HQUIC, ConnectionState>& get_connectionStateMap()
    {
        return connectionStateMap;
    }


public:
    StableContainer<MPMCQueue<std::string>> objectQueues;

    auto subscribe(protobuf_messages::SubscribeMessage&& subscribeMessage)
    {
        // WAIT_FOR_SETUP_COMPLETE()

        ConnectionState& connectionState = connectionStateMap.begin()->second;
        objectQueues.emplace_back();
        connectionState.objectQueue = --(objectQueues.end());

        protobuf_messages::MessageHeader subscribeHeader;
        subscribeHeader.set_messagetype(protobuf_messages::MoQtMessageType::SUBSCRIBE);


        QUIC_BUFFER* quicBuffer =
        serialization::serialize(subscribeHeader, subscribeMessage);

        connectionState.enqueue_control_buffer(quicBuffer);

        return connectionState.objectQueue;
    }


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
    std::atomic_bool isConnected{};

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

    void handle_message(ConnectionState& connectionState, HQUIC streamHandle, std::stringstream& iStringStream)
    {
        google::protobuf::io::IstreamInputStream istream(&iStringStream);

        protobuf_messages::MessageHeader header =
        serialization::deserialize<protobuf_messages::MessageHeader>(istream);

        /* TODO, split into client and server interpret functions helps reduce
         * the number of branches NOTE: This is the message received, which
         * means that the client will interpret the server's message and vice
         * verse CLIENT_SETUP is received by server and SERVER_SETUP is received
         * by client
         */

        StreamState& streamState =
        *get_stream_state(connectionState.connection, streamHandle);

        MessageHandler messageHandler(*this, connectionState);

        switch (header.messagetype())
        {
            case protobuf_messages::MoQtMessageType::CLIENT_SETUP:
            {
                // CLIENT sends to SERVER
                messageHandler.template handle_message<protobuf_messages::ClientSetupMessage>(connectionState, istream);
                break;
            }
            case protobuf_messages::MoQtMessageType::SERVER_SETUP:
            {
                // SERVER sends to CLIENT
                messageHandler.template handle_message<protobuf_messages::ServerSetupMessage>(connectionState, istream);
                break;
            }
            case protobuf_messages::MoQtMessageType::SUBSCRIBE:
            {
                // Client to Servevr
                messageHandler.template handle_message<protobuf_messages::SubscribeMessage>(connectionState, istream);

                break;
            }
            case protobuf_messages::MoQtMessageType::OBJECT_STREAM:
            {

                messageHandler.template handle_message<protobuf_messages::ObjectStreamMessage>(connectionState, istream);

                break;
            }
            default: LOGE("Unknown control message type", header.messagetype());
        }
        isConnected.store(true, std::memory_order_release);
    }


    RegisterSubscriptionErr
    try_register_subscription(ConnectionState& connectionState,
                              protobuf_messages::SubscribeMessage&& subscribeMessage)
    {
        RegisterSubscriptionErr errorInfo;

        SubscriptionManagerHandle
        {
        } -> try_register_subscription(connectionState, std::move(subscribeMessage));
        return errorInfo;
    }


protected:
    MOQT();

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

class MOQTServer : public MOQT
{
    rvn::unique_listener listener;

public:
    MOQTServer();

    void start_listener(QUIC_ADDR* LocalAddress);


    /*
        decltype(newConnectionInfo) is
        struct {
            const QUIC_NEW_CONNECTION_INFO* Info;
            HQUIC Connection;
        }
    */
    QUIC_STATUS accept_new_connection(HQUIC listener, auto newConnectionInfo)
    {
        QUIC_STATUS status = QUIC_STATUS_NOT_SUPPORTED;
        HQUIC connectionHandle = newConnectionInfo.Connection;
        status =
        get_tbl()->ConnectionSetConfiguration(connectionHandle, configuration.get());

        if (QUIC_FAILED(status))
        {
            return status;
        }

        get_tbl()->SetCallbackHandler(newConnectionInfo.Connection,
                                      (void*)(this->connection_cb_wrapper),
                                      (void*)(this));

        utils::ASSERT_LOG_THROW(connectionStateMap.find(connectionHandle) ==
                                connectionStateMap.end(),
                                "Trying to accept connection which already "
                                "exists");

        connectionStateMap.emplace(connectionHandle,
                                   ConnectionState{ connectionHandle, this });

        return status;
    }

    /*
        decltype(newStreamInfo) is
        struct {
            HQUIC Stream;
            QUIC_STREAM_OPEN_FLAGS Flags;
        }
    */
    QUIC_STATUS accept_control_stream(HQUIC connection, auto newStreamInfo)
    {
        // get connection state object
        ConnectionState& connectionState = connectionStateMap.at(connection);

        return connectionState.accept_control_stream(newStreamInfo.Stream);
    }

    void register_object(std::string trackNamesapce,
                         std::string trackName,
                         std::uint64_t groupId,
                         std::uint64_t objectId,
                         std::string objectPayload)
    {
        std::string path = DATA_DIRECTORY + trackNamesapce + "/" + trackName + "/" +
                           std::to_string(groupId) + "/" + std::to_string(objectId);

        std::ofstream file(path);
        file << objectPayload;
        file.close();
    }
};

class MOQTClient : public MOQT
{
    rvn::unique_connection connection;

public:
    // atomic flags for multi thread synchronization
    // make sure no connections are accepted until whole setup required is completed
    std::atomic_bool connectionSetupFlag;

    MOQTClient();

    void start_connection(QUIC_ADDRESS_FAMILY Family, const char* ServerName, uint16_t ServerPort);

    protobuf_messages::ClientSetupMessage get_clientSetupMessage();

    QUIC_STATUS accept_data_stream(HQUIC connection, HQUIC streamHandle)
    {
        ConnectionState& connectionState = connectionStateMap.at(connection);

        return connectionState.accept_data_stream(streamHandle);
    }
};
} // namespace rvn
