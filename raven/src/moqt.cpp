#include <messages.hpp>
#include <moqt.hpp>
#include <serialization.hpp>
#include <utilities.hpp>
namespace rvn {
MOQT &MOQT::set_regConfig(QUIC_REGISTRATION_CONFIG *regConfig_) {
    regConfig = regConfig_;
    add_to_secondary_counter(SecondaryIndices::regConfig);
    return *this;
}

MOQT &MOQT::set_listenerCb(listener_cb_lamda_t listenerCb_) {
    listener_cb_lamda = listenerCb_;
    add_to_secondary_counter(SecondaryIndices::listenerCb);
    return *this;
}

MOQT &MOQT::set_connectionCb(connection_cb_lamda_t connectionCb_) {
    connection_cb_lamda = connectionCb_;
    add_to_secondary_counter(SecondaryIndices::connectionCb);
    return *this;
}

// TOOD : check const corectness here
MOQT &MOQT::set_AlpnBuffers(QUIC_BUFFER *AlpnBuffers_) {
    AlpnBuffers = AlpnBuffers_;
    add_to_secondary_counter(SecondaryIndices::AlpnBuffers);
    return *this;
}

MOQT &MOQT::set_AlpnBufferCount(uint32_t AlpnBufferCount_) {
    AlpnBufferCount = AlpnBufferCount_;
    add_to_secondary_counter(SecondaryIndices::AlpnBufferCount);
    return *this;
}

// sets settings and setting size
MOQT &MOQT::set_Settings(QUIC_SETTINGS *Settings_, uint32_t SettingsSize_) {
    utils->check_setting_assertions(Settings_, SettingsSize_);

    Settings = Settings_;
    SettingsSize = SettingsSize_;
    add_to_secondary_counter(SecondaryIndices::Settings);
    return *this;
}

MOQT &MOQT::set_CredConfig(QUIC_CREDENTIAL_CONFIG *CredConfig_) {
    CredConfig = CredConfig_;
    add_to_secondary_counter(SecondaryIndices::CredConfig);
    return *this;
}

MOQT &MOQT::set_controlStreamCb(stream_cb_lamda_t controlStreamCb_) {
    control_stream_cb_lamda = controlStreamCb_;
    add_to_secondary_counter(SecondaryIndices::controlStreamCb);
    return *this;
}

MOQT &MOQT::set_dataStreamCb(stream_cb_lamda_t dataStreamCb_) {
    data_stream_cb_lamda = dataStreamCb_;
    add_to_secondary_counter(SecondaryIndices::dataStreamCb);
    return *this;
}

MOQT::MOQT() : tbl(rvn::make_unique_quic_table()) { secondaryCounter = 0; }

const QUIC_API_TABLE *MOQT::get_tbl() { return tbl.get(); }

void MOQT::interpret_control_message(ConnectionState &connectionState,
                                     const auto *receiveInformation) {
    if (!connectionState.controlStream.has_value())
        LOGE("Trying to interpret control message without control stream");

    // we will assume 2 buffers =>
    // first buffer contains headers related to control message such as type
    // second buffer contains the control message itself

    QUIC_BUFFER firstBuffer = receiveInformation->Buffers[0];
    QUIC_BUFFER secondBuffer = receiveInformation->Buffers[1];

    // TODO : implement serialization
    ControlMessageHeader header = serialization::deserialize<ControlMessageHeader>(firstBuffer);

    /* TODO, split into client and server interpret functions helps reduce the number of branches
     * NOTE: This is the message received, which means that the client will interpret the server's
     * message and vice verse
     * CLIENT_SETUP is received by server and SERVER_SETUP is received by client
     */
    switch (header.messageType) {
    case MoQtMessageType::CLIENT_SETUP: {
        // CLIENT sends to SERVER

        ClientSetupMessage clientSetupMessage =
            serialization::deserialize<ClientSetupMessage>(secondBuffer);
        auto &supportedVersions = clientSetupMessage.supportedVersions;
        auto matchingVersionIter =
            std::find(supportedVersions.begin(), supportedVersions.end(), version);

        if (matchingVersionIter == supportedVersions.end()) {
            // destroy connection
            // connectionState.destroy_connection();
            return;
        }

        std::size_t iterIdx = std::distance(supportedVersions.begin(), matchingVersionIter);
        auto &params = clientSetupMessage.parameters[iterIdx];
        connectionState.path = std::move(params.path.path);
        connectionState.peerRole = params.role.role;
        break;
    }
    case MoQtMessageType::SERVER_SETUP: {
        // SERVER sends to CLIENT
        ServerSetupMessage serverSetupMessage =
            serialization::deserialize<ServerSetupMessage>(secondBuffer);
        utils::ASSERT_LOG_THROW(connectionState.path.size() == 0,
                                "Server must now use the path parameter");
        utils::ASSERT_LOG_THROW(serverSetupMessage.parameters.size() > 0,
                                "SERVER_SETUP sent no parameters, requires atleast role parameter");
        connectionState.peerRole = serverSetupMessage.parameters[0].role.role;

        break;
    }
    case MoQtMessageType::GOAWAY: {
        // SERVER sends to CLIENT
        /*
         * The server MUST terminate the session with a Protocol Violation (Section 3.5) if it
         * receives a GOAWAY message. The client MUST terminate the session with a Protocol Violation
         * (Section 3.5) if it receives multiple GOAWAY messages.
        */
        GoAwayMessage goAwayMessage = serialization::deserialize<GoAwayMessage>(secondBuffer);
        if (goAwayMessage.newSessionURI.size() > 0) {
            connectionState.path = std::move(goAwayMessage.newSessionURI);
        }

        // client should use the new session URI from now
    }
    default:
        LOGE("Unknown control message type");
    }
}

void MOQT::interpret_data_message(ConnectionState &connectionState, HQUIC dataStream,
                                  const auto *receiveInformation) {
    if (!connectionState.controlStream.has_value())
        LOGE("Trying to interpret control message without control stream");

    // we will assume 2 buffers =>
    // first buffer contains headers related to control message such as type
    // second buffer contains the control message itself

    QUIC_BUFFER firstBuffer = receiveInformation->Buffers[0];
    QUIC_BUFFER secondBuffer = receiveInformation->Buffers[1];

    // TODO : implement serialization
    // ControlMessageHeader header = serialize_message_header(firstBuffer);

    // switch (header.MessageType) {
    // case MoQtMessageType::CLIENT_SETUP: {

    //     break;
    // }
    // case MoQtMessageType::SERVER_SETUP: {
    //     break;
    // }
    // default:
    //     LOGE("Unknown control message type");
    // }
}

} // namespace rvn
