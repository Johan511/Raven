////////////////////////////////
#include "msquic.h"
#include "subscription_manager.hpp"
#include <memory>
#include <moqt_server.hpp>
#include <optional>
#include <stdexcept>
#include <strong_types.hpp>
////////////////////////////////
#include <contexts.hpp>
#include <data_manager.hpp>
#include <definitions.hpp>
#include <message_handler.hpp>
#include <moqt.hpp>
#include <utilities.hpp>
////////////////////////////////

namespace rvn
{


void ConnectionState::delete_data_stream(HQUIC streamHandle)
{
    auto streamIter =
    std::find_if(dataStreams.begin(), dataStreams.end(),
                 [streamHandle](const DataStreamState& streamState)
                 { return streamState.stream.get() == streamHandle; });

    utils::ASSERT_LOG_THROW(streamIter != dataStreams.end(),
                            "Attempting to delete stream which does not "
                            "exist in busy stream queue");

    dataStreams.erase(streamIter);
}

void ConnectionState::send_control_buffer(QUIC_BUFFER* buffer, QUIC_SEND_FLAGS flags)
{
    // control messages have higher priority
    flags |= QUIC_SEND_FLAG_PRIORITY_WORK;

    StreamState* streamState = &controlStream.value();
    HQUIC streamHandle = streamState->stream.get();

    StreamSendContext* streamSendContext =
    new StreamSendContext(buffer, 1, streamState->streamContext.get());

    QUIC_STATUS status =
    moqtObject_.get_tbl()->StreamSend(streamHandle, buffer, 1, flags, streamSendContext);
    if (QUIC_FAILED(status))
        throw std::runtime_error("Failed to send control message");
}

const StableContainer<ConnectionState::DataStreamState>&
ConnectionState::get_data_streams() const
{
    return dataStreams;
}

StableContainer<ConnectionState::DataStreamState>& ConnectionState::get_data_streams()
{
    return dataStreams;
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
    auto& dataStreams = get_data_streams();

    // register new data stream into connectionState object
    dataStreams.emplace_back(rvn::unique_stream(moqtObject_.get_tbl(), streamHandle), *this);

    // set stream context for stream
    DataStreamState& streamState = dataStreams.back();
    streamState.set_stream_context(std::make_unique<StreamContext>(moqtObject_, *this));

    // set callback handler fot the stream (MOQT sets internally)
    moqtObject_.get_tbl()->SetCallbackHandler(streamHandle, (void*)MOQT::data_stream_cb_wrapper,
                                              (void*)streamState.streamContext.get());

    // registering stream can not fail
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS ConnectionState::accept_control_stream(HQUIC controlStreamHandle)
{
    this->controlStream.emplace(rvn::unique_stream(moqtObject_.get_tbl(), controlStreamHandle),
                                *this);
    this->controlStream->set_stream_context(
    std::make_unique<StreamContext>(moqtObject_, *this));


    moqtObject_.get_tbl()->SetCallbackHandler(controlStreamHandle,
                                              (void*)MOQT::control_stream_cb_wrapper,
                                              (
                                              void*)controlStream->streamContext.get());

    this->controlStream->streamContext->construct_deserializer(*this->controlStream);

    this->controlStream->streamContext->streamHasBeenConstructed.store(true, std::memory_order_release);


    return QUIC_STATUS_SUCCESS;
}

StreamState& ConnectionState::establish_control_stream()
{
    static int numTimesFunctionExecuted = 0;
    if (++numTimesFunctionExecuted != 1)
        utils::ASSERT_LOG_THROW(false, "establish control stream should be "
                                       "called only once in a connection");
    StreamContext* streamContext = new StreamContext(moqtObject_, *this);
    this->controlStream.emplace(rvn::unique_stream(moqtObject_.get_tbl(),
                                                   { connection_.get(), QUIC_STREAM_OPEN_FLAG_0_RTT,
                                                     MOQT::control_stream_cb_wrapper, streamContext },
                                                   { QUIC_STREAM_START_FLAG_PRIORITY_WORK }),
                                *this);

    this->controlStream->set_stream_context(std::unique_ptr<StreamContext>(streamContext));
    this->controlStream->streamContext->construct_deserializer(*this->controlStream);

    this->controlStream->streamContext->streamHasBeenConstructed.store(true, std::memory_order_release);
    return this->controlStream.value();
}

QUIC_STATUS ConnectionState::send_object(std::weak_ptr<DataStreamState> dataStreamWeakPtr,
                                         const ObjectIdentifier& objectIdentifier,
                                         QUIC_BUFFER* objectPayload)
{
    auto dataStreamSharedPtr = dataStreamWeakPtr.lock();

    if (!dataStreamSharedPtr)
        return QUIC_STATUS_INVALID_STATE;

    auto& dataStream = *dataStreamSharedPtr;

    utils::ASSERT_LOG_THROW(dataStream.can_send_object(objectIdentifier),
                            "Sending inappropriate object over data stream",
                            "Data stream Header: ", dataStream.header(),
                            "Object Identifier: ", objectIdentifier);


    StreamSendContext* streamSendContext =
    new StreamSendContext(objectPayload, 1, dataStream.streamContext.get());

    return moqtObject_.get_tbl()->StreamSend(dataStream.stream.get(), objectPayload,
                                             1, QUIC_SEND_FLAG_NONE, streamSendContext);
}

QUIC_STATUS ConnectionState::send_object(const ObjectIdentifier& objectIdentifier,
                                         QUIC_BUFFER* objectPayload)
{
    for (auto& dataStream : dataStreams)
    {
        if (dataStream.can_send_object(objectIdentifier))
        {
            StreamSendContext* streamSendContext =
            new StreamSendContext(objectPayload, 1, dataStream.streamContext.get());

            return moqtObject_.get_tbl()->StreamSend(dataStream.stream.get(),
                                                     objectPayload, 1, QUIC_SEND_FLAG_NONE,
                                                     streamSendContext);
        }
    }

    // TODO: Create new data stream and send object
    return QUIC_STATUS_INVALID_STATE;
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

void StreamContext::construct_deserializer(StreamState& streamState)
{
    SubscriptionManager& subscriptionManager =
    *static_cast<MOQTServer&>(moqtObject_).subscriptionManager_;
    streamState.streamContext->deserializer_.emplace(
    MessageHandler(streamState, subscriptionManager));
    return;
}
} // namespace rvn
