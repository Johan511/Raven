#pragma once
////////////////////////////////////////////
#include <msquic.h>
////////////////////////////////////////////
#include <cstdint>
#include <fstream>
#include <functional>
#include <ostream>
#include <sstream>
#include <unordered_map>
////////////////////////////////////////////
#include <contexts.hpp>
#include <moqt_utils.hpp>
#include <protobuf_messages.hpp>
#include <serialization.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>
////////////////////////////////////////////

#define DEFAULT_BUFFER_CAPACITY 1024

namespace rvn {

// context used when data is send on stream

class MOQT {

  public:
    using listener_cb_lamda_t = std::function<QUIC_STATUS(HQUIC, void *, QUIC_LISTENER_EVENT *)>;
    using connection_cb_lamda_t =
        std::function<QUIC_STATUS(HQUIC, void *, QUIC_CONNECTION_EVENT *)>;
    using stream_cb_lamda_t = std::function<QUIC_STATUS(HQUIC, void *, QUIC_STREAM_EVENT *)>;

    enum class SecondaryIndices {
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

    static constexpr std::uint64_t sec_index_to_val(SecondaryIndices idx) {
        auto intVal = rvn::utils::to_underlying(idx);

        return (1 << intVal);
    }

    std::uint32_t version;

    // PIMPL pattern for moqt related utilities
    MOQTUtilities *utils;

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // these functions will later be pushed into cgUtils
    // utils::MOQTComplexGetterUtils *cgUtils{this};

    StreamState *get_stream_state(HQUIC connectionHandle, HQUIC streamHandle) {
        ConnectionState &connectionState = connectionStateMap.at(connectionHandle);
        if (connectionState.controlStream.has_value() &&
            connectionState.controlStream.value().stream.get() == streamHandle) {
            return &connectionState.controlStream.value();
        }

        auto streamIter =
            std::find_if(connectionState.dataStreams.begin(), connectionState.dataStreams.end(),
                         [streamHandle](const StreamState &streamState) {
                             return streamState.stream.get() == streamHandle;
                         });

        if (streamIter != connectionState.dataStreams.end()) {
            return &(*streamIter);
        }

        return nullptr;
    }

    StreamState *get_stream_state(HQUIC streamHandle) {
        for (const auto &connectionStatePair : connectionStateMap) {
            HQUIC connectionHandle = connectionStatePair.first;
            ConnectionState &connectionState = connectionStateMap.at(connectionHandle);
            if (connectionState.controlStream.has_value() &&
                connectionState.controlStream.value().stream.get() == streamHandle) {
                return &connectionState.controlStream.value();
            }

            auto streamIter =
                std::find_if(connectionState.dataStreams.begin(), connectionState.dataStreams.end(),
                             [streamHandle](const StreamState &streamState) {
                                 return streamState.stream.get() == streamHandle;
                             });

            if (streamIter != connectionState.dataStreams.end()) {
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
    QUIC_REGISTRATION_CONFIG *regConfig;
    // Server will use listener and client will use connection
    listener_cb_lamda_t listener_cb_lamda;
    connection_cb_lamda_t connection_cb_lamda;
    stream_cb_lamda_t control_stream_cb_lamda;
    stream_cb_lamda_t data_stream_cb_lamda;
    QUIC_BUFFER *AlpnBuffers;
    uint32_t AlpnBufferCount;
    QUIC_SETTINGS *Settings;
    uint32_t SettingsSize; // set along with Settings
    QUIC_CREDENTIAL_CONFIG *CredConfig;

    std::uint64_t secondaryCounter;

    void add_to_secondary_counter(SecondaryIndices idx) {
        secondaryCounter |= sec_index_to_val(idx);
    }

    constexpr std::uint64_t full_sec_counter_value() {
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
    static QUIC_STATUS listener_cb_wrapper(HQUIC reg, void *context, QUIC_LISTENER_EVENT *event) {
        MOQT *thisObject = static_cast<MOQT *>(context);
        return thisObject->listener_cb_lamda(reg, context, event);
    }

    static QUIC_STATUS connection_cb_wrapper(HQUIC reg, void *context,
                                             QUIC_CONNECTION_EVENT *event) {
        MOQT *thisObject = static_cast<MOQT *>(context);
        return thisObject->connection_cb_lamda(reg, context, event);
    }

    static QUIC_STATUS control_stream_cb_wrapper(HQUIC stream, void *context,
                                                 QUIC_STREAM_EVENT *event) {
        StreamContext *thisObject = static_cast<StreamContext *>(context);
        return thisObject->moqtObject->control_stream_cb_lamda(stream, context, event);
    }

    static QUIC_STATUS data_stream_cb_wrapper(HQUIC stream, void *context,
                                              QUIC_STREAM_EVENT *event) {
        StreamContext *thisObject = static_cast<StreamContext *>(context);
        return thisObject->moqtObject->data_stream_cb_lamda(stream, context, event);
    }

    // Setters
    //////////////////////////////////////////////////////////////////////////
    MOQT &set_regConfig(QUIC_REGISTRATION_CONFIG *regConfig_);
    MOQT &set_listenerCb(listener_cb_lamda_t listenerCb_);
    MOQT &set_connectionCb(connection_cb_lamda_t connectionCb_);

    // check  corectness here
    MOQT &set_AlpnBuffers(QUIC_BUFFER *AlpnBuffers_);

    MOQT &set_AlpnBufferCount(uint32_t AlpnBufferCount_);

    // sets settings and setting size
    MOQT &set_Settings(QUIC_SETTINGS *Settings_, uint32_t SettingsSize_);

    MOQT &set_CredConfig(QUIC_CREDENTIAL_CONFIG *CredConfig_);

    MOQT &set_controlStreamCb(stream_cb_lamda_t controlStreamCb_);
    MOQT &set_dataStreamCb(stream_cb_lamda_t dataStreamCb_);
    //////////////////////////////////////////////////////////////////////////

    const QUIC_API_TABLE *get_tbl();

    // map from connection HQUIC to connection state
    std::unordered_map<HQUIC, ConnectionState>
        connectionStateMap; // should have size 1 in case of client
    std::unordered_map<HQUIC, ConnectionState> &get_connectionStateMap() {
        return connectionStateMap;
    }
    struct GetPayloadStreamError {
        std::uint8_t errc = 0;
    };

    auto get_payload_stream(ConnectionState &connectionState,
                            const protobuf_messages::SubscribeMessage &subscribeMessage) {

        std::ifstream *payloadStream = new std::ifstream();
        payloadStream->open(DUMMY_PAYLOAD_FILE_PATH, std::ios::in);

        GetPayloadStreamError errorInfo;

        return std::make_pair(payloadStream, errorInfo);
    }

    void register_subscription(StreamState &streamState,
                               const protobuf_messages::SubscribeMessage &subscribeMessage,
                               std::istream *payloadStream) {
        protobuf_messages::MessageHeader header;
        header.set_messagetype(protobuf_messages::MoQtMessageType::OBJECT_STREAM);

        protobuf_messages::ObjectStreamMessage objectStreamMessage;
        // <DummyValues>
        objectStreamMessage.set_subscribeid(1);
        objectStreamMessage.set_trackalias(1);
        objectStreamMessage.set_groupid(1);
        objectStreamMessage.set_objectid(1);
        objectStreamMessage.set_publisherpriority(1);
        objectStreamMessage.set_objectstatus(1);
        // </DummyValues>
        std::stringstream objectStreamMessageStream;
        objectStreamMessageStream << payloadStream->rdbuf();
        objectStreamMessage.set_objectpayload(objectStreamMessageStream.str());

        stream_send(streamState, header, objectStreamMessage);
    }
    void send_subscription_ok(ConnectionState &connectionState,
                              const protobuf_messages::SubscribeMessage &subscribeMessage) {}

  public:
    // always sends only one buffer
    template <typename... Args>
    QUIC_STATUS stream_send(const StreamState &streamState, Args &&...args) {
        utils::LOG_EVENT(std::cout, args.DebugString()...);

        std::size_t requiredBufferSize = 0;

        // calculate required buffer size
        std::ostringstream oss;
        (google::protobuf::util::SerializeDelimitedToOstream(args, &oss), ...);

        static constexpr std::uint32_t bufferCount = 1;

        std::string buffer = oss.str();
        void *sendBufferRaw = malloc(sizeof(QUIC_BUFFER) + buffer.size());
        utils::ASSERT_LOG_THROW(sendBufferRaw != nullptr, "Could not allocate memory for buffer");

        QUIC_BUFFER *sendBuffer = (QUIC_BUFFER *)sendBufferRaw;
        sendBuffer->Buffer = (uint8_t *)sendBufferRaw + sizeof(QUIC_BUFFER);
        sendBuffer->Length = buffer.size();

        std::memcpy(sendBuffer->Buffer, buffer.c_str(), buffer.size());
        HQUIC streamHandle = streamState.stream.get();

        StreamSendContext *streamSendContext =
            new StreamSendContext(sendBuffer, bufferCount, streamState.streamContext.get());

        QUIC_STATUS status = get_tbl()->StreamSend(streamHandle, sendBuffer, 1, QUIC_SEND_FLAG_FIN,
                                                   streamSendContext);

        return status;
    }

    // auto is used as parameter because there is no named type for receive information
    // it is an anonymous structure
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
    void interpret_message(ConnectionState &connectionState, HQUIC streamHandle,
                           const auto *receiveInformation) {
        utils::ASSERT_LOG_THROW(connectionState.controlStream.has_value(),
                                "Trying to interpret control message without control stream");

        const QUIC_BUFFER *buffers = receiveInformation->Buffers;

        std::istringstream iStringStream(
            std::string(reinterpret_cast<const char *>(buffers[0].Buffer), buffers[0].Length));

        google::protobuf::io::IstreamInputStream istream(&iStringStream);

        protobuf_messages::MessageHeader header =
            serialization::deserialize<protobuf_messages::MessageHeader>(istream);

        /* TODO, split into client and server interpret functions helps reduce the number of
         * branches NOTE: This is the message received, which means that the client will interpret
         * the server's message and vice verse CLIENT_SETUP is received by server and SERVER_SETUP
         * is received by client
         */

        StreamState &streamState = *get_stream_state(connectionState.connection, streamHandle);
        switch (header.messagetype()) {
        case protobuf_messages::MoQtMessageType::CLIENT_SETUP: {
            // CLIENT sends to SERVER

            protobuf_messages::ClientSetupMessage clientSetupMessage =
                serialization::deserialize<protobuf_messages::ClientSetupMessage>(istream);

            auto &supportedversions = clientSetupMessage.supportedversions();
            auto matchingVersionIter =
                std::find(supportedversions.begin(), supportedversions.end(), version);

            if (matchingVersionIter == supportedversions.end()) {
                // TODO
                // destroy connection
                // connectionState.destroy_connection();
                return;
            }

            std::size_t iterIdx = std::distance(supportedversions.begin(), matchingVersionIter);
            auto &params = clientSetupMessage.parameters()[iterIdx];
            connectionState.path = std::move(params.path().path());
            connectionState.peerRole = params.role().role();

            utils::LOG_EVENT(std::cout, "Client Setup Message received: \n",
                             clientSetupMessage.DebugString());

            // send SERVER_SETUP message
            protobuf_messages::MessageHeader serverSetupHeader;
            serverSetupHeader.set_messagetype(protobuf_messages::MoQtMessageType::SERVER_SETUP);

            protobuf_messages::ServerSetupMessage serverSetupMessage;
            serverSetupMessage.add_parameters()->mutable_role()->set_role(
                protobuf_messages::Role::Publisher);
            stream_send(streamState, serverSetupHeader, serverSetupMessage);

            break;
        }
        case protobuf_messages::MoQtMessageType::SERVER_SETUP: {
            // SERVER sends to CLIENT
            protobuf_messages::ServerSetupMessage serverSetupMessage =
                serialization::deserialize<protobuf_messages::ServerSetupMessage>(istream);

            utils::ASSERT_LOG_THROW(connectionState.path.size() == 0,
                                    "Server must now use the path parameter");
            utils::ASSERT_LOG_THROW(
                serverSetupMessage.parameters().size() > 0,
                "SERVER_SETUP sent no parameters, requires atleast role parameter");

            utils::LOG_EVENT(std::cout,
                             "Server Setup Message received: ", serverSetupMessage.DebugString());

            connectionState.peerRole = serverSetupMessage.parameters()[0].role().role();

            break;
        }
        case protobuf_messages::MoQtMessageType::SUBSCRIBE: {
            // Subscriber sends to Publisher

            protobuf_messages::SubscribeMessage subscribeMessage;
            // serialization::deserialize<protobuf_messages::SubscribeMessage>(istream);

            // utils::ASSERT_LOG_THROW(connectionState.peerRole ==
            // protobuf_messages::Role::Publisher,
            //                         "Client can only subscribe to Publisher");

            // utils::ASSERT_LOG_THROW(
            //     subscribeMessage.has_startgroup() ^ subscribeMessage.has_startobject(),
            //     "StartGroup and StartObject must both be sent or both should not be sent");

            // utils::ASSERT_LOG_THROW(
            //     subscribeMessage.has_startgroup() &&
            //         (subscribeMessage.filtertype() == protobuf_messages::AbsoluteStart ||
            //          subscribeMessage.filtertype() == protobuf_messages::AbsoluteRange),
            //     "Start group Id is only present for AbsoluteStart and AbsoluteRange filter
            //     types.");

            // utils::ASSERT_LOG_THROW(
            //     !subscribeMessage.has_startobject() &&
            //         (subscribeMessage.filtertype() == protobuf_messages::AbsoluteStart ||
            //          subscribeMessage.filtertype() == protobuf_messages::AbsoluteRange),
            //     "Start object Id is only present for AbsoluteStart and AbsoluteRange filter "
            //     "types.");

            // utils::ASSERT_LOG_THROW(
            //     subscribeMessage.has_endgroup() ^ subscribeMessage.has_endobject(),
            //     "EndGroup and EndObject must both be sent or both should not be sent");

            // utils::ASSERT_LOG_THROW(
            //     subscribeMessage.has_endgroup() &&
            //         (subscribeMessage.filtertype() == protobuf_messages::AbsoluteRange),
            //     "End group Id is only present for AbsoluteRange filter types.");

            // // EndObject: The end Object ID, plus 1. A value of 0 means the entire group is
            // // requested.
            // utils::ASSERT_LOG_THROW(
            //     !subscribeMessage.has_endobject() &&
            //         (subscribeMessage.filtertype() == protobuf_messages::AbsoluteRange),
            //     "End object Id is only present for AbsoluteRange filter "
            //     "types.");

            auto [payloadStream, errorInfo] = get_payload_stream(connectionState, subscribeMessage);
            if (errorInfo.errc != 0) {
                // TODO: error handling and sending SUBSCRIBE_ERROR message
                // send_subscribe_error(connectionState, subscribeMessage, errorInfo.value());
                break;
            }

            // TODO: ordering between subscribe ok and register subscription (which manages sending
            // of data)
            register_subscription(streamState, subscribeMessage, payloadStream);
            send_subscription_ok(connectionState, subscribeMessage);

            utils::LOG_EVENT(std::cout, "Subscribe Message received: \n",
                             subscribeMessage.DebugString());

            break;
        }
        case protobuf_messages::MoQtMessageType::OBJECT_STREAM: {
            // Publisher sends to Subscriber
            /*
            OBJECT_STREAM Message {
              Subscribe ID (i),
              Track Alias (i),
              Group ID (i),
              Object ID (i),
              Publisher Priority (8),
              Object Status (i),
              Object Payload (..),
            }
            */

            protobuf_messages::ObjectStreamMessage objectStreamMessage =
                serialization::deserialize<protobuf_messages::ObjectStreamMessage>(istream);

            std::cout << "ObjectPayload: \n" << objectStreamMessage.objectpayload() << std::endl;

            break;
        }
        default:
            LOGE("Unknown control message type", header.messagetype());
        }
    }

  protected:
    MOQT();

    struct open_and_start_new_stream_params {
        class OpenParams {
          public:
            HQUIC connectionHandle;
            QUIC_STREAM_OPEN_FLAGS openFlags;
            QUIC_STREAM_CALLBACK_HANDLER streamCb;
        };

        class StartParams {
          public:
            QUIC_STREAM_START_FLAGS startFlags;
        };
    };

    const StreamState &
    open_and_start_new_stream(open_and_start_new_stream_params::OpenParams openParams,
                              open_and_start_new_stream_params::StartParams startParams,
                              StreamType streamType) {
        HQUIC connectionHandle = openParams.connectionHandle;
        StreamContext *streamContext = new StreamContext(this, openParams.connectionHandle);

        auto stream = rvn::unique_stream(
            get_tbl(),
            {openParams.connectionHandle, openParams.openFlags, openParams.streamCb, streamContext},
            {startParams.startFlags});

        auto &connectionState = this->connectionStateMap[connectionHandle];
        StreamState *streamState = nullptr;

        switch (streamType) {
        case StreamType::CONTROL:
            connectionState.controlStream = StreamState{std::move(stream), DEFAULT_BUFFER_CAPACITY};
            streamState = &connectionState.controlStream.value();
            break;
        case StreamType::DATA:
            connectionState.dataStreams.emplace_back(std::move(stream), DEFAULT_BUFFER_CAPACITY);
            streamState = &connectionState.dataStreams.back();
            break;
        }

        streamState->set_stream_context(std::unique_ptr<StreamContext>(streamContext));

        return *streamState;
    }

  public:
    ~MOQT() { google::protobuf::ShutdownProtobufLibrary(); }
};

class MOQTServer : public MOQT {
    rvn::unique_listener listener;

  public:
    MOQTServer();

    void start_listener(QUIC_ADDR *LocalAddress);

    const StreamState &
    open_and_start_new_stream(open_and_start_new_stream_params::OpenParams openParams,
                              open_and_start_new_stream_params::StartParams startParams) {
        // Server can only start data stream
        return MOQT::open_and_start_new_stream(openParams, startParams, StreamType::DATA);
    }

    /*
        decltype(newConnectionInfo) is
        struct {
            const QUIC_NEW_CONNECTION_INFO* Info;
            HQUIC Connection;
        }
    */
    QUIC_STATUS register_new_connection(HQUIC listener, auto newConnectionInfo) {
        QUIC_STATUS status = QUIC_STATUS_NOT_SUPPORTED;
        HQUIC connection = newConnectionInfo.Connection;
        status = get_tbl()->ConnectionSetConfiguration(connection, configuration.get());

        if (QUIC_FAILED(status)) {
            return status;
        }

        get_tbl()->SetCallbackHandler(newConnectionInfo.Connection,
                                      (void *)(this->connection_cb_wrapper), (void *)(this));

        connectionStateMap[connection] = ConnectionState{connection};

        return status;
    }

    /*
        decltype(newStreamInfo) is
        struct {
            HQUIC Stream;
            QUIC_STREAM_OPEN_FLAGS Flags;
        }
    */
    QUIC_STATUS register_control_stream(HQUIC connection, auto newStreamInfo) {
        ConnectionState &connectionState = connectionStateMap.at(connection);
        utils::ASSERT_LOG_THROW(!connectionState.controlStream.has_value(),
                                "Control stream already registered by connection: ", connection);
        connectionState.controlStream = StreamState{
            rvn::unique_stream(get_tbl(), newStreamInfo.Stream), DEFAULT_BUFFER_CAPACITY};

        StreamState &streamState = connectionState.controlStream.value();
        streamState.set_stream_context(std::make_unique<StreamContext>(this, connection));
        this->get_tbl()->SetCallbackHandler(newStreamInfo.Stream,
                                            (void *)MOQT::control_stream_cb_wrapper,
                                            (void *)streamState.streamContext.get());

        // registering stream can not fail
        return QUIC_STATUS_SUCCESS;
    }

    void send_stream_object(HQUIC connectionHandle, std::istream &istream) {
        ConnectionState &connectionState = connectionStateMap.at(connectionHandle);
        const StreamState &streamState = open_and_start_new_stream(
            {connectionHandle, QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL, MOQT::data_stream_cb_wrapper},
            {QUIC_STREAM_START_FLAG_FAIL_BLOCKED | QUIC_STREAM_START_FLAG_SHUTDOWN_ON_FAIL});
    }
};

class MOQTClient : public MOQT {
    rvn::unique_connection connection;

  public:
    MOQTClient();

    void start_connection(QUIC_ADDRESS_FAMILY Family, const char *ServerName, uint16_t ServerPort);

    protobuf_messages::ClientSetupMessage get_clientSetupMessage() {
        protobuf_messages::ClientSetupMessage clientSetupMessage;
        clientSetupMessage.set_numsupportedversions(1);
        clientSetupMessage.add_supportedversions(version);
        clientSetupMessage.add_numberofparameters(1);
        auto *param1 = clientSetupMessage.add_parameters();
        param1->mutable_path()->set_path("path");
        param1->mutable_role()->set_role(protobuf_messages::Role::Subscriber);

        return clientSetupMessage;
    }

    QUIC_STATUS register_data_stream(HQUIC connection, auto newStreamInfo) {
        ConnectionState &connectionState = connectionStateMap.at(connection);
        connectionState.dataStreams.emplace_back(
            rvn::unique_stream(get_tbl(), newStreamInfo.Stream), DEFAULT_BUFFER_CAPACITY);

        StreamState &streamState = connectionState.dataStreams.back();
        streamState.set_stream_context(std::make_unique<StreamContext>(this, connection));
        this->get_tbl()->SetCallbackHandler(newStreamInfo.Stream,
                                            (void *)MOQT::control_stream_cb_wrapper,
                                            (void *)streamState.streamContext.get());

        // registering stream can not fail
        return QUIC_STATUS_SUCCESS;
    }

    const StreamState &
    open_and_start_new_stream(open_and_start_new_stream_params::OpenParams openParams,
                              open_and_start_new_stream_params::StartParams startParams) {
        // Client can only start control stream
        return MOQT::open_and_start_new_stream(openParams, startParams, StreamType::CONTROL);
    }

    std::ostream *subscribe() {
        ConnectionState &connectionState = connectionStateMap.at(connection.get());
        const StreamState &streamState = open_and_start_new_stream(
            {connectionState.connection, QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
             MOQT::data_stream_cb_wrapper},
            {QUIC_STREAM_START_FLAG_FAIL_BLOCKED | QUIC_STREAM_START_FLAG_SHUTDOWN_ON_FAIL});

        protobuf_messages::MessageHeader header;
        header.set_messagetype(protobuf_messages::MoQtMessageType::SUBSCRIBE);

        stream_send(streamState, header);

        return nullptr;
    }
};
} // namespace rvn
