#pragma once

///////////////////////////////////c
#include <exceptions.hpp>
#include <msquic.h>
#include <utilities.hpp>
///////////////////////////////////
#include <google/protobuf/util/delimited_message_util.h>
#include <setup_messages.pb.h>
///////////////////////////////////
#include <algorithm>
///////////////////////////////////
#include <protobuf_messages.hpp>
///////////////////////////////////

namespace rvn::serialization
{
// do not use templates here, we are not preventing any duplication by using
// templatees and knowing type helps catch errors easily while compiling (do not
// have to wait for template to be instantiated)
using namespace protobuf_messages;

template <typename T, typename InputStream> T deserialize(InputStream& istream)
{
    T t;
    bool clean_eof;
    google::protobuf::util::ParseDelimitedFromZeroCopyStream(&t, &istream, &clean_eof);
    if (clean_eof)
        throw rvn::exception::parsing_exception();
    return t;
};


template <typename... Args> QUIC_BUFFER* serialize(Args&&... args)
{
    std::size_t requiredBufferSize = 0;
    std::ostringstream oss;
    (google::protobuf::util::SerializeDelimitedToOstream(args, &oss), ...);

    static constexpr std::uint32_t bufferCount = 1;
    std::string buffer = std::move(oss).str();

    void* sendBufferRaw = malloc(sizeof(QUIC_BUFFER) + buffer.size());
    utils::ASSERT_LOG_THROW(sendBufferRaw != nullptr,
                            "Could not allocate memory for buffer");


    QUIC_BUFFER* sendBuffer = (QUIC_BUFFER*)sendBufferRaw;
    sendBuffer->Buffer = (uint8_t*)sendBufferRaw + sizeof(QUIC_BUFFER);
    sendBuffer->Length = buffer.size();

    std::memcpy(sendBuffer->Buffer, buffer.c_str(), buffer.size());

    return sendBuffer;
}
} // namespace rvn::serialization
