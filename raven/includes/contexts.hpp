#pragma once
//////////////////////////////
#include <msquic.h>
//////////////////////////////
#include <boost/lockfree/queue.hpp>
#include <functional>
#include <memory>
#include <optional>
//////////////////////////////
#include <definitions.hpp>
#include <protobuf_messages.hpp>
#include <serialization.hpp>
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
    class MOQT* moqtObject;
    HQUIC connection;
    StreamContext(MOQT* moqtObject_, HQUIC connection_)
    : moqtObject(moqtObject_), connection(connection_) {};
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
    std::unique_ptr<StreamContext> streamContext{};
    std::stringstream messageSS;

    template <typename... Args> void set_stream_context(Args&&... args)
    {
        this->streamContext =
        std::make_unique<StreamContext>(std::forward<Args>(args)...);
    }

    void set_stream_context(std::unique_ptr<StreamContext>&& streamContext_)
    {
        this->streamContext = std::move(streamContext_);
    }
};


namespace bl = boost::lockfree;
struct ConnectionState
{
    // StreamManager //////////////////////////////////////////////////////////////
    // TODO: Inlined into the class because of some bug, please check (457239f)
    static constexpr std::size_t MAX_DATA_STREAMS = 8;

    std::queue<QUIC_BUFFER*> dataBuffersToSend;
    std::vector<StreamState> dataStreams; // they are sending/receiving data

    std::queue<QUIC_BUFFER*> controlBuffersToSend;
    std::optional<StreamState> controlStream{};


    void delete_data_stream(HQUIC streamHandle);

    void enqueue_data_buffer(QUIC_BUFFER* buffer);
    void enqueue_control_buffer(QUIC_BUFFER* buffer);

    StreamState& create_data_stream();

    void send_data_buffer();
    void send_control_buffer();
    /////////////////////////////////////////////////////////////////////////////


    HQUIC connection = nullptr;
    class MOQT* moqtObject = nullptr;

    std::string path;
    protobuf_messages::Role peerRole;
    bool expectControlStreamShutdown = true;
    StableContainer<MPMCQueue<std::string>>::iterator objectQueue;


    ConnectionState(HQUIC connection_, class MOQT* moqtObject_)
    : connection(connection_), moqtObject(moqtObject_)
    {
    }

    bool check_subscription(const protobuf_messages::SubscribeMessage& subscribeMessage);
    const std::vector<StreamState>& get_data_streams() const;
    std::vector<StreamState>& get_data_streams();

    std::optional<StreamState>& get_control_stream();
    const std::optional<StreamState>& get_control_stream() const;


    QUIC_STATUS accept_data_stream(HQUIC dataStreamHandle);

    QUIC_STATUS accept_control_stream(HQUIC controlStreamHandle);

    StreamState& reset_control_stream();

    void register_subscription(const protobuf_messages::SubscribeMessage& subscribeMessage,
                               std::string&& payload);

    void add_to_queue(const std::string& objectPayload)
    {
        objectQueue->enqueue(objectPayload);
    }
};

} // namespace rvn
