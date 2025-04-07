#pragma once

#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <ostream>
#include <utilities.hpp>

namespace rvn::ds
{

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
    using type = std::uint8_t;

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

    std::tuple<std::uint8_t*, std::uint64_t> release()
    {
        std::uint8_t* data = data_;
        std::uint64_t size = currSize_;

        currSize_ = 0;
        maxSize_ = 0;
        data_ = nullptr;
        return { data, size };
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

    bool operator==(const chunk& other) const noexcept
    {
        if (currSize_ != other.currSize_)
            return false;

        return std::memcmp(data_, other.data_, currSize_) == 0;
    }

    std::uint8_t& operator[](std::uint64_t index) noexcept
    {
        return data_[index];
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
            std::uint64_t toAllocSize = utils::next_power_of_2(requiredSizeMin);

            void* reallocedData = realloc(data_, toAllocSize);
            if (reallocedData == nullptr)
                throw std::bad_alloc();

            data_ = static_cast<std::uint8_t*>(reallocedData);
            maxSize_ = toAllocSize;
        }
        std::memcpy(data_ + currSize_, src, size);
        currSize_ += size;
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
};

class ChunkSpan
{
    const chunk& chunk_;
    std::uint64_t beginOffset_;
    std::uint64_t endOffset_;

public:
    ChunkSpan(const chunk& chunk)
    : chunk_(chunk), beginOffset_(0), endOffset_(chunk.size())
    {
        utils::ASSERT_LOG_THROW(chunk_.size() > 0, "Chunk size must be greater "
                                                   "than 0");
    }

    ChunkSpan(const chunk& chunk, std::uint64_t beginOffset) : ChunkSpan(chunk)
    {
        beginOffset_ = beginOffset;
        utils::ASSERT_LOG_THROW(beginOffset_ <= endOffset_,
                                "beginOffset must be less than or equal to "
                                "endOffset");
    }
    ChunkSpan(const chunk& chunk, std::uint64_t beginOffset, std::uint64_t endOffset)
    : ChunkSpan(chunk, beginOffset)
    {
        endOffset_ = endOffset;
        utils::ASSERT_LOG_THROW(endOffset_ <= chunk_.size(),
                                "endOffset must be less than or equal to chunk "
                                "size");
    }

    void copy_to(void* dest, std::uint64_t size) const
    {
        utils::ASSERT_LOG_THROW(size <= this->size(),
                                "size must be less than or equal to span size");
        std::memcpy(dest, data(), size);
    }

    const std::uint8_t* data() const noexcept
    {
        return chunk_.data() + beginOffset_;
    }

    std::uint64_t size() const noexcept
    {
        return endOffset_ - beginOffset_;
    }

    std::uint8_t operator[](std::uint64_t index) const noexcept
    {
        return chunk_.data()[beginOffset_ + index];
    }

    void advance_begin(std::uint64_t size)
    {
        beginOffset_ += size;
        utils::ASSERT_LOG_THROW(beginOffset_ <= endOffset_,
                                "beginOffset must be less than or equal to "
                                "endOffset");
    }

    friend inline std::ostream& operator<<(std::ostream& os, ChunkSpan span)
    {
        // Save the current formatting state of the stream
        std::ios_base::fmtflags f(os.flags());

        for (std::uint64_t i = 0; i < span.size(); ++i)
        {
            os << std::bitset<8>(span[i]) << " ";
        }

        // Restore the original formatting state
        os.flags(f);

        return os;
    }
};
} // namespace rvn::ds
