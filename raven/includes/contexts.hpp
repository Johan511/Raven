#pragma once
//////////////////////////////
#include "wrappers.hpp"
#include <msquic.h>
//////////////////////////////
#include <functional>
#include <memory>
#include <optional>
//////////////////////////////
#include <protobuf_messages.hpp>
#include <utilities.hpp>
//////////////////////////////

namespace rvn {
enum class StreamType { CONTROL, DATA };

struct StreamContext {
    class MOQT *moqtObject;
    HQUIC connection;
    StreamContext(MOQT *moqtObject_, HQUIC connection_)
        : moqtObject(moqtObject_), connection(connection_) {};
};

class StreamSendContext {
  public:
    // owns the QUIC_BUFFERS
    QUIC_BUFFER *buffer;
    std::uint32_t bufferCount;

    // non owning reference
    const StreamContext *streamContext;

    std::function<void(StreamSendContext *)> sendCompleteCallback =
        utils::NoOpVoid<StreamSendContext *>;

    StreamSendContext(QUIC_BUFFER *buffer_, const std::uint32_t bufferCount_,
                      const StreamContext *streamContext_,
                      std::function<void(StreamSendContext *)> sendCompleteCallback_ =
                          utils::NoOpVoid<StreamSendContext *>)
        : buffer(buffer_), bufferCount(bufferCount_), streamContext(streamContext_),
          sendCompleteCallback(sendCompleteCallback_) {
        if (bufferCount != 1)
            LOGE("StreamSendContext can only have one buffer");
    }

    ~StreamSendContext() { destroy_buffers(); }

    std::tuple<QUIC_BUFFER *, std::uint32_t> get_buffers() { return {buffer, bufferCount}; }
    void destroy_buffers() {
        //  bufferCount is always 1
        // the way we allocate buffers is  malloc(sizeof(QUIC_BUFFER)) and this becomes our
        // (QUIC_BUFFER *) buffers
        free(buffer);
        bufferCount = 0;
    }

    // callback called when the send is succsfull
    void send_complete_cb() { sendCompleteCallback(this); }
};

struct StreamState {
    rvn::unique_stream stream;
    std::size_t bufferCapacity;
    std::unique_ptr<StreamContext> streamContext{};

    template <typename... Args> void set_stream_context(Args &&...args) {
        this->streamContext = std::make_unique<StreamContext>(std::forward<Args>(args)...);
    }

    void set_stream_context(std::unique_ptr<StreamContext> &&streamContext_) {
        this->streamContext = std::move(streamContext_);
    }
};

struct ConnectionState {
    HQUIC connection = nullptr;
    std::optional<StreamState> controlStream{};
    std::vector<StreamState> dataStreams{};
    std::string path;
    protobuf_messages::Role peerRole;

    bool check_subscription(const protobuf_messages::SubscribeMessage &subscribeMessage) {
        // TODO: check if subscription data exists and if we are authenticated to get it
        return true;
    }
};

} // namespace rvn
