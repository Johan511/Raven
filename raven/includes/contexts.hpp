#pragma once
//////////////////////////////
#include <data_manager.hpp>
#include <msquic.h>
#include <serialization/messages.hpp>
#include <strong_types.hpp>
//////////////////////////////
#include <functional>
#include <memory>
#include <optional>
//////////////////////////////
#include <definitions.hpp>
#include <deserializer.hpp>
#include <message_handler.hpp>
#include <serialization/serialization.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>
//////////////////////////////

#define DEFAULT_BUFFER_CAPACITY 512

namespace rvn
{
enum class StreamType
{
    CONTROL,
    DATA
};

struct StreamContext
{
    std::atomic_bool streamHasBeenConstructed{};
    class MOQT& moqtObject_;
    /*  We can not have reference to StreamState
        StreamState constructor takes rvn::unique_stream which requires
        StreamContext to be constructed
        Constructors can not have cyclical dependency
     */
    class ConnectionState& connectionState_;
    /*
        We can not have operator= for Deserializer because it contains
        a non movable object (std::mutex)
        We can not construct it in constructor because it requires
        StreamState which requires rvn::unique_stream which requires StreamContext
    */
    std::optional<serialization::Deserializer<MessageHandler>> deserializer_;
    StreamContext(MOQT& moqtObject, ConnectionState& connectionState)
    : moqtObject_(moqtObject), connectionState_(connectionState) {};

    // deserializer can not be constructed in the constructor and has to be done seperately
    void construct_deserializer(StreamState& streamState);
};

class StreamSendContext
{
public:
    // owns the QUIC_BUFFERS
    QUIC_BUFFER* buffer;
    std::uint32_t bufferCount;

    // non owning reference
    const StreamContext* streamContext;

    std::function<void(StreamSendContext*)> sendCompleteCallback =
    utils::NoOpVoid<StreamSendContext*>;

    StreamSendContext(QUIC_BUFFER* buffer_,
                      const std::uint32_t bufferCount_,
                      const StreamContext* streamContext_,
                      std::function<void(StreamSendContext*)> sendCompleteCallback_ =
                      utils::NoOpVoid<StreamSendContext*>)
    : buffer(buffer_), bufferCount(bufferCount_), streamContext(streamContext_),
      sendCompleteCallback(sendCompleteCallback_)
    {
        if (bufferCount != 1)
            LOGE("StreamSendContext can only have one buffer");
    }

    ~StreamSendContext()
    {
        destroy_buffers();
    }

    std::tuple<QUIC_BUFFER*, std::uint32_t> get_buffers()
    {
        return { buffer, bufferCount };
    }
    void destroy_buffers()
    {
        //  bufferCount is always 1
        // the way we allocate buffers is  malloc(sizeof(QUIC_BUFFER)) and this
        // becomes our (QUIC_BUFFER *) buffers
        free(buffer);
        bufferCount = 0;
    }

    // callback called when the send is succsfull
    void send_complete_cb()
    {
        sendCompleteCallback(this);
    }
};

struct StreamState
{
    rvn::unique_stream stream;
    class ConnectionState& connectionState_;
    std::unique_ptr<StreamContext> streamContext{};

    StreamState(rvn::unique_stream&& stream_, class ConnectionState& connectionState_)
    : stream(std::move(stream_)), connectionState_(connectionState_)
    {
    }

    void set_stream_context(std::unique_ptr<StreamContext> streamContext_)
    {
        this->streamContext = std::move(streamContext_);
    }
};

struct ConnectionState : std::enable_shared_from_this<ConnectionState>
{
    // StreamManager //////////////////////////////////////////////////////////////
    // TODO: Inlined into the class because of some bug, please check (457239f)

    std::shared_mutex trackAliasMtx_;
    std::unordered_map<TrackIdentifier, TrackAlias, TrackIdentifier::Hash, TrackIdentifier::Equal> trackAliasMap_;
    std::unordered_map<std::uint64_t, TrackIdentifier> trackAliasRevMap_;

    void add_track_alias(TrackIdentifier trackIdentifier, TrackAlias trackAlias);

    // wtf is currGroup?
    std::shared_mutex currGroupMtx_;
    std::unordered_map<TrackIdentifier, GroupId, TrackIdentifier::Hash, TrackIdentifier::Equal> currGroupMap_;
    std::optional<GroupId> get_current_group(const TrackIdentifier& trackIdentifier);
    std::optional<GroupId> get_current_group(TrackAlias trackAlias);

    std::optional<TrackIdentifier> alias_to_identifier(TrackAlias trackAlias);
    std::optional<TrackAlias> identifier_to_alias(const TrackIdentifier& trackIdentifier);


    class DataStreamState : public StreamState,
                            public std::enable_shared_from_this<DataStreamState>
    {
        depracated::messages::StreamHeaderSubgroupMessage streamHeaderSubgroupMessage_;

    public:
        DataStreamState(rvn::unique_stream&& stream, class ConnectionState& connectionState)
        : StreamState(std::move(stream), connectionState)
        {
        }

        void set_header(depracated::messages::StreamHeaderSubgroupMessage streamHeaderSubgroupMessage)
        {
            streamHeaderSubgroupMessage_ = std::move(streamHeaderSubgroupMessage);
        }

        const auto& header()
        {
            return streamHeaderSubgroupMessage_;
        }

        bool can_send_object(const ObjectIdentifier& objectIdentifier) const
        {
            auto trackAliasOpt = connectionState_.identifier_to_alias(objectIdentifier);
            if (!trackAliasOpt.has_value())
                return false;

            bool trackBoolMatch =
            (streamHeaderSubgroupMessage_.groupId_ == objectIdentifier.groupId_) &&
            (trackAliasOpt.value() == streamHeaderSubgroupMessage_.trackAlias_);

            if (!trackBoolMatch)
                return false;

            // TODO: return true should be replaced with subgroupId match
            return true;
        }
    };

    StableContainer<DataStreamState> dataStreams;

    std::optional<StreamState> controlStream{};

    void delete_data_stream(HQUIC streamHandle);
    void enqueue_data_buffer(QUIC_BUFFER* buffer);

    StreamState& create_data_stream();

    QUIC_STATUS send_object(std::weak_ptr<DataStreamState> dataStream,
                            const ObjectIdentifier& objectIdentifier,
                            QUIC_BUFFER* buffer);
    QUIC_STATUS send_object(const ObjectIdentifier& objectIdentifier, QUIC_BUFFER* buffer);
    void send_control_buffer(QUIC_BUFFER* buffer, QUIC_SEND_FLAGS flags = QUIC_SEND_FLAG_NONE);
    /////////////////////////////////////////////////////////////////////////////

    unique_connection connection_;
    MOQT& moqtObject_;

    std::string path;
    // TODO: role

    // Only for Subscribers
    // TODO: We can have multiple subscriptions
    StableContainer<MPMCQueue<std::string>>::iterator objectQueue;


    ConnectionState(unique_connection&& connection, class MOQT& moqtObject)
    : connection_(std::move(connection)), moqtObject_(moqtObject)
    {
    }

    const StableContainer<DataStreamState>& get_data_streams() const;
    StableContainer<DataStreamState>& get_data_streams();
    std::optional<StreamState>& get_control_stream();
    const std::optional<StreamState>& get_control_stream() const;


    QUIC_STATUS accept_data_stream(HQUIC dataStreamHandle);

    QUIC_STATUS accept_control_stream(HQUIC controlStreamHandle);

    StreamState& establish_control_stream();

    void add_to_queue(const std::string& objectPayload)
    {
        objectQueue->enqueue(objectPayload);
    }
};

} // namespace rvn
