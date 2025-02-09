#pragma once
////////////////////////////////////////////
#include "data_manager.hpp"
#include <cstdint>
#include <fstream>
////////////////////////////////////////////
#include <contexts.hpp>
#include <moqt_base.hpp>
#include <msquic.h>
#include <serialization/messages.hpp>
#include <serialization/serialization.hpp>
#include <subscription_manager.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>
////////////////////////////////////////////


namespace rvn
{
class MOQTServer : public MOQT
{
    rvn::unique_listener listener;

public:
    std::shared_ptr<DataManager> dataManager_;
    std::shared_ptr<SubscriptionManager> subscriptionManager_;
    std::unordered_map<HQUIC, std::shared_ptr<ConnectionState>> connectionStateMap;

    // map from connection HQUIC to connection state
    auto& get_connectionStateMap()
    {
        return connectionStateMap;
    }

    MOQTServer(std::shared_ptr<DataManager> dataManager,
               std::shared_ptr<SubscriptionManager> subscriptionManager);

    void start_listener(QUIC_ADDR* LocalAddress);

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // these functions will later be pushed into cgUtils
    // utils::MOQTComplexGetterUtils *cgUtils{this};

    StreamState* get_stream_state(HQUIC connectionHandle, HQUIC streamHandle)
    {
        ConnectionState& connectionState = *connectionStateMap.at(connectionHandle);
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
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////


    /*
        decltype(newConnectionInfo) is
        struct {
            const QUIC_NEW_CONNECTION_INFO* Info;
            HQUIC Connection;
        }
    */
    QUIC_STATUS accept_new_connection(HQUIC listener, auto newConnectionInfo)
    {
        HQUIC connectionHandle = newConnectionInfo.Connection;
        get_tbl()->ConnectionSetConfiguration(connectionHandle, configuration.get());

        get_tbl()->SetCallbackHandler(newConnectionInfo.Connection,
                                      (void*)(this->connection_cb_wrapper),
                                      (void*)(this));

        utils::ASSERT_LOG_THROW(connectionStateMap.find(connectionHandle) ==
                                connectionStateMap.end(),
                                "Trying to accept connection which already "
                                "exists");

        unique_connection connection = unique_connection(tbl.get(), connectionHandle);

        connectionStateMap.emplace(connectionHandle,
                                   std::make_shared<ConnectionState>(std::move(connection),
                                                                     *this));

        return QUIC_STATUS_SUCCESS;
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
        ConnectionState& connectionState = *connectionStateMap.at(connection);

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
} // namespace rvn
