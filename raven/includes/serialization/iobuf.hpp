#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <stdlib.h>
#include <vector>

namespace rvn::ds
{
namespace detail
{
    static inline std::uint64_t next_power_of_2(std::uint64_t n) noexcept
    {
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        n++;
        return n;
    }
} // namespace detail

class chunk
{
    std::uint64_t currSize_; // number of bytes written
    std::uint64_t maxSize_;  // length of data_ allocated
    std::uint8_t* data_;

    void swap(chunk& other) noexcept
    {
        std::swap(currSize_, other.currSize_);
        std::swap(maxSize_, other.maxSize_);
        std::swap(data_, other.data_);
    }

public:
    chunk() : currSize_{ 0 }, maxSize_{ 0 }, data_{ nullptr }
    {
    }

    // takes ownership of buffer
    chunk(std::unique_ptr<std::uint8_t[]>&& buffer, std::uint64_t len)
    : currSize_(len), maxSize_(len), data_(buffer.release())
    {
    }

    // copies data
    chunk(const std::uint8_t* data, std::uint64_t len) : chunk()
    {
        append(data, len);
    }

    chunk(std::uint64_t maxSize)
    : currSize_{ 0 }, maxSize_{ maxSize },
      // We use malloc and realloc instead of new
      data_{ static_cast<std::uint8_t*>(malloc(sizeof(std::uint8_t) * maxSize)) }
    {
        if (data_ == nullptr)
        {
            currSize_ = 0;
            maxSize_ = 0;
            throw std::bad_alloc();
        }
    }

    chunk& operator=(const chunk& other)
    {
        if (this == std::addressof(other))
            return *this;

        clear();
        append(other.data_, other.currSize_);

        return *this;
    }

    chunk(const chunk& other)
    : chunk() // without default constructor, the values would be garbage
    {
        *this = other;
    }

    chunk& operator=(chunk&& other) noexcept
    {
        if (this == std::addressof(other))
            return *this;

        free(data_);

        // steal values
        currSize_ = other.currSize_;
        maxSize_ = other.maxSize_;
        data_ = other.data_;

        // invalidate other
        other.currSize_ = 0;
        other.maxSize_ = 0;
        other.data_ = nullptr;

        return *this;
    }

    chunk(chunk&& other) noexcept
    : chunk() // without default constructor, the values would be garbage
    {
        *this = std::move(other);
    }


    ~chunk() noexcept
    {
        free(data_);
    }

    std::uint64_t size() const noexcept
    {
        return currSize_;
    }

    std::uint64_t capacity() const noexcept
    {
        return maxSize_;
    }

    std::uint8_t* data() const noexcept
    {
        return data_;
    }

    void clear() noexcept
    {
        currSize_ = 0;
    }

    bool can_append(std::uint64_t size) const noexcept
    {
        return currSize_ + size <= maxSize_;
    }

    void append(const void* src, std::uint64_t size)
    {
        if (!can_append(size))
        {
            std::uint64_t requiredSizeMin = currSize_ + size;
            // we allocate next power of 2 to required size min
            std::uint64_t allocatedSize = detail::next_power_of_2(requiredSizeMin);

            void* reallocedData = realloc(data_, allocatedSize);
            if (reallocedData == nullptr)
                throw std::bad_alloc();

            data_ = static_cast<std::uint8_t*>(reallocedData);
            maxSize_ = allocatedSize;
            currSize_ = requiredSizeMin;
        }
        else
        {
            std::memcpy(data_ + currSize_, src, size);
            currSize_ += size;
        }
    }

    void reserve(std::uint64_t size)
    {
        if (size <= maxSize_)
            return;
        void* reallocedData = realloc(data_, size);
        if (reallocedData == nullptr)
            throw std::bad_alloc();

        data_ = static_cast<std::uint8_t*>(reallocedData);
        maxSize_ = size;
    }

    std::tuple<std::unique_ptr<std::uint8_t[]>, std::uint64_t> release() noexcept
    {
        std::uint64_t currSizeCopy_ = currSize_;
        currSize_ = 0;
        maxSize_ = 0;

        return std::make_tuple(std::unique_ptr<std::uint8_t[]>(data_), currSizeCopy_);
    }
};

class iobuf
{
    // TODO: replace with small vector
    std::vector<chunk> chunks_;

public:
    iobuf() = default;

    // TOOD: check if move should be defined as noexcept

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