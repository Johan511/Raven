#include "serialization/chunk.hpp"
#include <deserializer.hpp>
#include <initializer_list>
#include <memory>
#include <serialization/messages.hpp>
#include <serialization/serialization_impl.hpp>

using namespace rvn;
using namespace rvn::serialization;

std::unique_ptr<QUIC_BUFFER> construct_quic_buffer(std::uint64_t lenBuffer)
{
    QUIC_BUFFER* totalQuicBuffer =
    static_cast<QUIC_BUFFER*>(malloc(sizeof(QUIC_BUFFER) + lenBuffer));
    totalQuicBuffer->Length = lenBuffer;
    totalQuicBuffer->Buffer =
    reinterpret_cast<uint8_t*>(totalQuicBuffer) + sizeof(QUIC_BUFFER);

    return std::unique_ptr<QUIC_BUFFER>(totalQuicBuffer);
}

std::vector<std::shared_ptr<QUIC_BUFFER>>
generate_quic_buffers(std::initializer_list<ds::chunk> chunks)
{
    std::uint64_t totalSize = 0;
    for (const auto& chunk : chunks)
        totalSize += chunk.size();

    std::vector<std::shared_ptr<QUIC_BUFFER>> quicBuffers;
    quicBuffers.reserve(totalSize / 2);

    // To stress it we serialize 1 to 3 byte chunks
    const auto serialize_chunk_into_multiple_quic_buffers = [&quicBuffers](auto&& messageChunk)
    {
        for (size_t i = 0; i != messageChunk.size();)
        {
            std::uint64_t quicBufferSize = (i % 3) + 1; // deterministic number between [1, 3]
            quicBufferSize =
            std::min(quicBufferSize, static_cast<std::uint64_t>(messageChunk.size() - i));
            quicBuffers.emplace_back(construct_quic_buffer(quicBufferSize));
            memcpy(quicBuffers.back().get()->Buffer, messageChunk.data() + i, quicBufferSize);
            i += quicBufferSize;
        }
    };

    for (const auto& chunk : chunks)
        serialize_chunk_into_multiple_quic_buffers(chunk);

    return quicBuffers;
}

template <class... Ts> struct overloads : Ts...
{
    using Ts::operator()...;
};

void test1()
{
    // Client Setup Message
    depracated::messages::ClientSetupMessage clientSetupMessage;
    clientSetupMessage.supportedVersions_ = { 1, 2, 3 };
    ds::chunk clientSetupMessageChunk;
    detail::serialize(clientSetupMessageChunk, clientSetupMessage);

    // Server Setup Message
    depracated::messages::ServerSetupMessage serverSetupMessage;
    serverSetupMessage.selectedVersion_ = 1;
    ds::chunk serverSetupMessageChunk;
    detail::serialize(serverSetupMessageChunk, serverSetupMessage);

    auto quicBuffers =
    generate_quic_buffers({ clientSetupMessageChunk, serverSetupMessageChunk });

    const auto visitor =
    overloads{ [](...) { std::cout << "Unexpected Message\n"; },
               [](const depracated::messages::ClientSetupMessage& msg)
               { std::cout << "Received ClientSetupMessage\n"; },
               [](const depracated::messages::ServerSetupMessage& msg)
               { std::cout << "Received ServerSetupMessage\n"; } };

    Deserializer deserializer(visitor);
    for (const auto& quicBuffer : quicBuffers)
        deserializer.append_buffer(quicBuffer);

    return;
}


int main()
{
    test1();
    return 0;
}
