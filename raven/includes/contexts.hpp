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
struct StreamContext {
    class MOQT *moqtObject;
    HQUIC connection;
    StreamContext(MOQT *moqtObject_, HQUIC connection_)
        : moqtObject(moqtObject_), connection(connection_) {};
};

class StreamSendContext {
  public:
    // owns the QUIC_BUFFERS
    QUIC_BUFFER *buffers;
    std::uint32_t bufferCount;

    // non owning reference
    const StreamContext *streamContext;

    std::function<void(StreamSendContext *)> sendCompleteCallback =
        utils::NoOpVoid<StreamSendContext *>;

    StreamSendContext(QUIC_BUFFER *buffers_, const std::uint32_t bufferCount_,
                      const StreamContext *streamContext_,
                      std::function<void(StreamSendContext *)> sendCompleteCallback_ =
                          utils::NoOpVoid<StreamSendContext *>)
        : buffers(buffers_), bufferCount(bufferCount_), streamContext(streamContext_),
          sendCompleteCallback(sendCompleteCallback_) {}

    ~StreamSendContext() { destroy_buffers(); }

    std::tuple<QUIC_BUFFER *, std::uint32_t> get_buffers() { return {buffers, bufferCount}; }
    void destroy_buffers() {
        for (std::size_t i = 0; i < bufferCount; i++) {
            free(buffers[i].Buffer);
        }
        buffers = nullptr;
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
};

} // namespace rvn
