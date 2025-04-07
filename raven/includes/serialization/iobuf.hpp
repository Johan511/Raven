#pragma once

#include <boost/container/small_vector.hpp>
#include <cstdint>
#include <cstring>
#include <memory>
#include <serialization/chunk.hpp>
#include <serialization/serialization.hpp>
#include <stdlib.h>

namespace rvn::ds
{

class IOBuf
{
    boost::container::small_vector<chunk, 5> chunks_;

public:
    IOBuf() = default;

    // TOOD: check if move should be defined explicitly as noexcept

    // allocated and used length is assumed to be len
    void append(std::unique_ptr<std::uint8_t[]>&& buffer, std::uint64_t len)
    {
        if (!chunks_.empty() && chunks_.back().can_append(len))
            chunks_.back().append(buffer.release(), len);
        else
            chunks_.emplace_back(std::move(buffer), len);
    }

    void append(const std::uint8_t* data, std::uint64_t len)
    {
        if (!chunks_.empty() && chunks_.back().can_append(len))
            chunks_.back().append(data, len);
        else
            chunks_.emplace_back(data, len);
    }

    void coallesce()
    {
        if (chunks_.size() <= 1)
            return;

        std::uint64_t requiredSize = 0;
        for (const auto& chunk : chunks_)
            requiredSize += chunk.size();

        chunk newChunk(requiredSize);
        for (const auto& chunk : chunks_)
            newChunk.append(chunk.data(), chunk.size());

        chunks_.clear();
        chunks_.emplace_back(std::move(newChunk));
    }
};
} // namespace rvn::ds
