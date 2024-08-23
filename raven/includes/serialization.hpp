#include "msquic.h"
/*
    State Machine or protobuff
*/
namespace rvn::serialization {
template <typename T> void serialize(const T &data, QUIC_BUFFER &buffer) {
    buffer.Buffer = reinterpret_cast<uint8_t *>(&data);
    buffer.Length = sizeof(data);
}

template <typename T> T deserialize(const QUIC_BUFFER &buffer) {
    T data = *reinterpret_cast<T *>(buffer.Buffer);
    return data;
}
} // namespace rvn::serialization