#include "serialization/chunk.hpp"
#include "strong_types.hpp"
#include "wrappers.hpp"
#include <deserializer.hpp>
#include <initializer_list>
#include <serialization/messages.hpp>
#include <serialization/serialization_impl.hpp>

using namespace rvn;
using namespace rvn::serialization;

UniqueQuicBuffer construct_quic_buffer(std::uint64_t lenBuffer) {
  QUIC_BUFFER *totalQuicBuffer =
      static_cast<QUIC_BUFFER *>(malloc(sizeof(QUIC_BUFFER) + lenBuffer));
  totalQuicBuffer->Length = lenBuffer;
  totalQuicBuffer->Buffer =
      reinterpret_cast<uint8_t *>(totalQuicBuffer) + sizeof(QUIC_BUFFER);

  return UniqueQuicBuffer(totalQuicBuffer,
                          QUIC_BUFFERDeleter(nullptr, nullptr));
}

std::vector<UniqueQuicBuffer>
generate_quic_buffers(std::vector<ds::chunk> chunks) {
  std::uint64_t totalSize = 0;
  for (const auto &chunk : chunks)
    totalSize += chunk.size();

  std::vector<UniqueQuicBuffer> quicBuffers;
  quicBuffers.reserve(totalSize / 2);

  // To stress it we serialize to small chunks
  const auto serialize_chunk_into_multiple_quic_buffers =
      [&quicBuffers](auto &&messageChunk) {
        for (size_t i = 0; i != messageChunk.size();) {
          std::uint64_t quicBufferSize =
              (i % 3) + 1; // serialized to 1 or 2 bytes
          quicBufferSize =
              std::min(quicBufferSize,
                       static_cast<std::uint64_t>(messageChunk.size() - i));
          quicBuffers.emplace_back(construct_quic_buffer(quicBufferSize));
          memcpy(quicBuffers.back().get()->Buffer, messageChunk.data() + i,
                 quicBufferSize);
          i += quicBufferSize;
        }
      };

  for (const auto &chunk : chunks)
    serialize_chunk_into_multiple_quic_buffers(chunk);

  return quicBuffers;
}

template <class... Ts> struct overloads : Ts... {
  using Ts::operator()...;
};

void test1() {
  // Client Setup Message
  ClientSetupMessage clientSetupMessage;
  clientSetupMessage.supportedVersions_ = {1, 2, 3};
  ds::chunk clientSetupMessageChunk;
  serialization::detail::serialize(clientSetupMessageChunk, clientSetupMessage);

  // Server Setup Message
  ServerSetupMessage serverSetupMessage;
  serverSetupMessage.selectedVersion_ = 1;
  ds::chunk serverSetupMessageChunk;
  serialization::detail::serialize(serverSetupMessageChunk, serverSetupMessage);

  auto quicBuffers =
      generate_quic_buffers({clientSetupMessageChunk, serverSetupMessageChunk});

  const auto visitor =
      overloads{[](...) { std::cout << "Unexpected Message\n"; },
                [](const ClientSetupMessage &) {
                  std::cout << "Received ClientSetupMessage\n";
                },
                [](const ServerSetupMessage &) {
                  std::cout << "Received ServerSetupMessage\n";
                }};

  Deserializer deserializer(true, visitor);
  for (auto &&quicBuffer : quicBuffers)
    deserializer.append_buffer(std::move(quicBuffer));

  return;
}

void test2() {
  std::vector<ds::chunk> chunks;
  chunks.resize(1);

  StreamHeaderSubgroupMessage streamHeaderSubgroupMessage;
  streamHeaderSubgroupMessage.subgroupId_ = SubGroupId(1);
  streamHeaderSubgroupMessage.groupId_ = GroupId(1);
  streamHeaderSubgroupMessage.trackAlias_ = TrackAlias(1);
  streamHeaderSubgroupMessage.publisherPriority_ = PublisherPriority(1);

  serialization::detail::serialize(chunks[0], streamHeaderSubgroupMessage);

  constexpr std::uint64_t NumObjects = 1000;
  std::uint64_t initSize = chunks.size();

  chunks.resize(chunks.size() + NumObjects);

  for (std::uint64_t i = 0; i < 1000; i++) {
    StreamHeaderSubgroupObject streamObjectMessage;
    streamObjectMessage.objectId_ = ObjectId(i);
    streamObjectMessage.payload_ = "Object Message: " + std::to_string(i);
    serialization::detail::serialize(chunks[i + initSize], streamObjectMessage);
  }

  auto quicBuffers = generate_quic_buffers(chunks);

  const auto visitor =
      overloads{[](...) { std::cout << "Unexpected Message\n"; },
                [](const StreamHeaderSubgroupMessage &h) {
                  std::cout << "Received Subgroup Header Message\n"
                            << h << "\n";
                },
                [](const StreamHeaderSubgroupObject &o) {
                  std::cout << "Received Subgroup Object\n" << o << "\n";
                }};

  Deserializer deserializer(false, visitor);
  for (auto &&quicBuffer : quicBuffers)
    deserializer.append_buffer(std::move(quicBuffer));

  return;
}

int main() {
  test1();
  test2();
  return 0;
}
