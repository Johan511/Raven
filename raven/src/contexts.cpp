////////////////////////////////
#include "moqt_server.hpp"
#include <contexts.hpp>
#include <data_manager.hpp>
#include <definitions.hpp>
#include <message_handler.hpp>
#include <moqt.hpp>
#include <msquic.h>
#include <strong_types.hpp>
#include <subscription_manager.hpp>
#include <utilities.hpp>
#include <variant>
#include <wrappers.hpp>
////////////////////////////////
#include <memory>
#include <optional>
#include <stdexcept>
////////////////////////////////

namespace rvn
{

DataStreamState::DataStreamState(rvn::unique_stream&& stream, struct ConnectionState& connectionState)
: StreamState(std::move(stream), connectionState),
  lifeTimeFlag_(std::make_shared<std::monostate>()),
  objectQueue_(std::make_shared<MPMCQueue<StreamHeaderSubgroupObject>>())
{
}


bool DataStreamState::can_send_object(const ObjectIdentifier& objectIdentifier) const noexcept
{
    auto trackAliasOpt = connectionState_.identifier_to_alias(objectIdentifier);
    if (!trackAliasOpt.has_value())
        return false;

    bool trackBoolMatch =
    (streamHeaderSubgroupMessage_->groupId_ == objectIdentifier.groupId_) &&
    (trackAliasOpt.value() == streamHeaderSubgroupMessage_->trackAlias_);

    if (!trackBoolMatch)
        return false;

    // TODO: return true should be replaced with subgroupId match
    return true;
}

void DataStreamState::set_header(StreamHeaderSubgroupMessage streamHeaderSubgroupMessage)
{
    streamHeaderSubgroupMessage_ =
    std::make_shared<StreamHeaderSubgroupMessage>(std::move(streamHeaderSubgroupMessage));
}

std::weak_ptr<void> DataStreamState::get_life_time_flag() const noexcept
{
    return lifeTimeFlag_;
}


void ConnectionState::delete_data_stream(HQUIC streamHandle)
{
    dataStreams.write(
    [&streamHandle](StableContainer<DataStreamState>& dataStreams)
    {
        auto iter =
        std::find_if(dataStreams.begin(), dataStreams.end(),
                     [&streamHandle](const DataStreamState& streamState)
                     { return streamState.stream.get() == streamHandle; });

        dataStreams.erase(iter);
    });
}

void ConnectionState::send_control_buffer(QUIC_BUFFER* buffer, QUIC_SEND_FLAGS flags)
{
    // control messages have higher priority
    flags |= QUIC_SEND_FLAG_PRIORITY_WORK;

    StreamState* streamState = &controlStream.value();
    HQUIC streamHandle = streamState->stream.get();

    StreamSendContext* streamSendContext =
    new StreamSendContext(buffer, 1, streamState->streamContext_);

    QUIC_STATUS status =
    moqtObject_.get_tbl()->StreamSend(streamHandle, buffer, 1, flags, streamSendContext);
    if (QUIC_FAILED(status))
        throw std::runtime_error("Failed to send control message");
}

const std::optional<StreamState>& ConnectionState::get_control_stream() const
{
    return controlStream;
}

std::optional<StreamState>& ConnectionState::get_control_stream()
{
    return controlStream;
}


QUIC_STATUS ConnectionState::accept_data_stream(HQUIC streamHandle)
{
    // register new data stream into connectionState object
    return dataStreams.write(
    [&](StableContainer<DataStreamState>& dataStreams)
    {
        dataStreams.emplace_back(rvn::unique_stream(moqtObject_.get_tbl(), streamHandle),
                                 *this);

        // set stream context for stream
        DataStreamState& streamState = dataStreams.back();
        streamState.set_stream_context(new StreamContext(moqtObject_, *this));
        streamState.streamContext_->construct_deserializer(streamState, false);

        // set callback handler fot the stream (MOQT sets internally)
        moqtObject_.get_tbl()->SetCallbackHandler(streamHandle, (void*)MOQT::data_stream_cb_wrapper,
                                                  (void*)streamState.streamContext_);

        return QUIC_STATUS_SUCCESS;
    });
}

QUIC_STATUS ConnectionState::accept_control_stream(HQUIC controlStreamHandle)
{
    this->controlStream.emplace(rvn::unique_stream(moqtObject_.get_tbl(), controlStreamHandle),
                                *this);
    this->controlStream->set_stream_context(new StreamContext(moqtObject_, *this));


    moqtObject_.get_tbl()->SetCallbackHandler(controlStreamHandle,
                                              (void*)MOQT::control_stream_cb_wrapper,
                                              (void*)controlStream->streamContext_);

    controlStream->streamContext_->construct_deserializer(*this->controlStream, true);
    controlStream->streamContext_->streamHasBeenConstructed.store(true, std::memory_order_release);


    return QUIC_STATUS_SUCCESS;
}

StreamState& ConnectionState::establish_control_stream()
{
    static int numTimesFunctionExecuted = 0;
    if (++numTimesFunctionExecuted != 1)
        utils::ASSERT_LOG_THROW(false, "establish control stream should be "
                                       "called only once in a connection");
    StreamContext* streamContext = new StreamContext(moqtObject_, *this);
    controlStream.emplace(rvn::unique_stream(moqtObject_.get_tbl(),
                                             { connection_.get(), QUIC_STREAM_OPEN_FLAG_0_RTT,
                                               MOQT::control_stream_cb_wrapper, streamContext },
                                             { QUIC_STREAM_START_FLAG_PRIORITY_WORK }),
                          *this);

    controlStream->set_stream_context(streamContext);
    controlStream->streamContext_->construct_deserializer(controlStream.value(), true);
    controlStream->streamContext_->streamHasBeenConstructed.store(true, std::memory_order_release);

    return this->controlStream.value();
}


QUIC_STATUS ConnectionState::send_object(const ObjectIdentifier& objectIdentifier,
                                         QUIC_BUFFER* objectPayload)
{
    auto sendObjectLambda = [&](const StableContainer<DataStreamState>& dataStreams)
    {
        auto iter =
        std::find_if(dataStreams.begin(), dataStreams.end(),
                     [&objectIdentifier](const DataStreamState& streamState)
                     { return streamState.can_send_object(objectIdentifier); });

        // We return this to indicate that we have not found a stream to send the object
        // This is not an error, we just need to create a new stream to send the object
        // We never expect StreamSend to return `QUIC_STATUS_ALPN_NEG_FAILURE`, hence
        // if it was returned, the intent is clear
        if (iter == dataStreams.end())
            return QUIC_STATUS_ALPN_NEG_FAILURE;

        StreamSendContext* streamSendContext =
        new StreamSendContext(objectPayload, 1, iter->streamContext_);

        return moqtObject_.get_tbl()->StreamSend(iter->stream.get(), objectPayload,
                                                 1, QUIC_SEND_FLAG_PRIORITY_WORK,
                                                 streamSendContext);
        return QUIC_STATUS_SUCCESS;
    };

    QUIC_STATUS trySendStatus = dataStreams.read(sendObjectLambda);


    if (trySendStatus == QUIC_STATUS_ALPN_NEG_FAILURE)
    {
        // header message
        StreamHeaderSubgroupMessage objectHeader;
        // TODO: add error handling to this
        objectHeader.trackAlias_ = identifier_to_alias(objectIdentifier).value();
        objectHeader.groupId_ = objectIdentifier.groupId_;
        // TOOD: get subgroupId
        objectHeader.subgroupId_ = SubGroupId(0);

        // Get publisher priority from group
        MOQTServer& moqtServer = static_cast<MOQTServer&>(moqtObject_);
        objectHeader.publisherPriority_ =
        moqtServer.dataManager_->get_publisher_priority(objectIdentifier).value();

        QUIC_BUFFER* objectHeaderQuicBuffer = serialization::serialize(objectHeader);

        // Create a new stream and send the object
        StreamContext* streamContext = new StreamContext(moqtObject_, *this);

        // TODO: do error handling here
        auto stream =
        rvn::unique_stream(moqtObject_.get_tbl(),
                           { connection_.get(), QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
                             moqtObject_.data_stream_cb_wrapper, streamContext },
                           { QUIC_STREAM_START_FLAG_SHUTDOWN_ON_FAIL });

        QUIC_STATUS status = dataStreams.write(
        [&, streamIn = std::move(stream), this](StableContainer<DataStreamState>& dataStreams) mutable
        {
            dataStreams.emplace_back(std::move(streamIn), *this);
            DataStreamState& streamState = dataStreams.back();
            streamState.set_header(objectHeader);
            streamState.set_stream_context(streamContext);

            // no need deserializer because we don't expect to receive any messages on this stream

            StreamSendContext* streamSendContext =
            new StreamSendContext(objectHeaderQuicBuffer, 1, streamState.streamContext_);

            // Set priority of stream to indicate the priority of the group
            // MsQuic uses uint16_t stream priority, unlike moqt which uses 8 bit
            std::uint16_t streamPriority = objectHeader.publisherPriority_;
            moqtObject_.get_tbl()->SetParam(streamState.stream.get(), QUIC_PARAM_STREAM_PRIORITY,
                                            sizeof(std::uint16_t), &streamPriority);

            return moqtObject_.get_tbl()->StreamSend(streamState.stream.get(),
                                                     objectHeaderQuicBuffer, 1,
                                                     QUIC_SEND_FLAG_DELAY_SEND,
                                                     streamSendContext);
        });

        if (QUIC_FAILED(status))
            return status;

        return send_object(objectIdentifier, objectPayload);
    }

    return trySendStatus;
}

void ConnectionState::abort_if_sending(const ObjectIdentifier& oid)
{
    dataStreams.write(
    [&](StableContainer<DataStreamState>& dataStreams)
    {
        auto iter = std::find_if(dataStreams.begin(), dataStreams.end(),
                                 [&](DataStreamState& streamState)
                                 { return streamState.can_send_object(oid); });

        if (iter != dataStreams.end())
            dataStreams.erase(iter);
    });
}

std::optional<GroupId> ConnectionState::get_current_group(const TrackIdentifier& trackIdentifier)
{
    // reader lock
    std::shared_lock<std::shared_mutex> l(currGroupMtx_);

    auto iter = currGroupMap_.find(trackIdentifier);
    if (iter == currGroupMap_.end())
        return std::nullopt;

    return iter->second;
}

std::optional<GroupId> ConnectionState::get_current_group(TrackAlias trackAlias)
{
    std::optional<TrackIdentifier> trackIdentifier = alias_to_identifier(trackAlias);

    if (!trackIdentifier.has_value())
        return std::nullopt;

    return get_current_group(*trackIdentifier);
}

void ConnectionState::add_track_alias(TrackIdentifier trackIdentifier, TrackAlias trackAlias)
{
    // writer lock
    std::unique_lock<std::shared_mutex> l(trackAliasMtx_);

    trackAliasMap_.emplace(trackIdentifier, trackAlias);
    trackAliasRevMap_.emplace(trackAlias, std::move(trackIdentifier));
}

std::optional<TrackIdentifier> ConnectionState::alias_to_identifier(TrackAlias trackAlias)
{
    // reader lock
    std::shared_lock<std::shared_mutex> l(trackAliasMtx_);

    auto iter = trackAliasRevMap_.find(trackAlias);
    if (iter == trackAliasRevMap_.end())
        return std::nullopt;

    return iter->second;
}

std::optional<TrackAlias>
ConnectionState::identifier_to_alias(const TrackIdentifier& trackIdentifier)
{
    // reader lock
    std::shared_lock<std::shared_mutex> l(trackAliasMtx_);

    auto iter = trackAliasMap_.find(trackIdentifier);
    if (iter == trackAliasMap_.end())
        return std::nullopt;

    return iter->second;
}

void StreamContext::construct_deserializer(StreamState& streamState, bool isControlStream)
{
    if (moqtObject_.hostType_ == HostType::SERVER)
    {
        SubscriptionManager* subscriptionManager =
        static_cast<MOQTServer&>(moqtObject_).subscriptionManager_.get();
        streamState.streamContext_->deserializer_.emplace(isControlStream,
                                                          MessageHandler(streamState, subscriptionManager));
    }
    else
        streamState.streamContext_->deserializer_.emplace(isControlStream,
                                                          MessageHandler(streamState, nullptr));
    return;
}
} // namespace rvn
