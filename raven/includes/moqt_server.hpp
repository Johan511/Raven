#pragma once
////////////////////////////////////////////
#include <contexts.hpp>
#include <data_manager.hpp>
#include <moqt_base.hpp>
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

    std::shared_mutex connectionStateMapMtx;
    std::unordered_map<HQUIC, std::shared_ptr<ConnectionState>> connectionStateMap;

    MOQTServer(std::shared_ptr<DataManager> dataManager,
               std::tuple<QUIC_GLOBAL_EXECUTION_CONFIG*, std::uint64_t> execConfigTuple = {
               nullptr, 0 });

    void start_listener(QUIC_ADDR* LocalAddress);

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // these functions will later be pushed into cgUtils
    // utils::MOQTComplexGetterUtils *cgUtils{this};

    /*
        decltype(newConnectionInfo) is
        struct {
            const QUIC_NEW_CONNECTION_INFO* Info;
            HQUIC Connection;
        }
    */
    QUIC_STATUS accept_new_connection(auto newConnectionInfo)
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

        std::unique_lock l(connectionStateMapMtx);
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

    void cleanup_connection(HQUIC connection)
    {
        std::unique_lock l(connectionStateMapMtx);
        connectionStateMap.erase(connection);
    }
};
} // namespace rvn
