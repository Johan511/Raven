#pragma once
//////////////////////////////
#include <msquic.h>
//////////////////////////////
#include <functional>
#include <memory>
#include <optional>
//////////////////////////////
#include <messages.hpp>
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
    // owns the QUIC_BUFFERS
    QUIC_BUFFER *buffers;
    std::uint32_t bufferCount;
    std::function<void(StreamSendContext *)> sendCompleteCallback =
        utils::NoOpVoid<StreamSendContext *>;

  public:
    // non owning reference
    const StreamContext *streamContext;

    StreamSendContext(StreamContext *streamContext) : streamContext(streamContext) {};

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
    HQUIC stream;
    std::size_t bufferCapacity;
    std::unique_ptr<StreamContext> streamContext{};

    void set_stream_context(std::unique_ptr<StreamContext> &&streamContext) {
        this->streamContext = std::move(streamContext);
    }
};

struct ConnectionState {
    HQUIC connection = nullptr;
    std::optional<StreamState> controlStream{};
    std::vector<StreamState> dataStreams{};
    std::string path;
    Role peerRole;
};

} // namespace rvn
