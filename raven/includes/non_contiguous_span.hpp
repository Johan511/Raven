#pragma once

#include <algorithm>
#include <boost/container/small_vector.hpp>
#include <memory>
#include <msquic.h>
#include <span>
#include <utilities.hpp>
#include <wrappers.hpp>

namespace rvn::serialization
{

class NonContiguousSpan
{
    std::span<SharedQuicBuffer> buffers_;
    // where is begins in the first QUIC_BUFFER
    std::uint64_t beginIdx_;

    // where it ends in the last QUIC_BUFFER
    std::uint64_t endIdx_;

    // cumulativeSize_[i] = sum of sizes of first i + 1 buffers
    boost::container::small_vector<std::uint64_t, 5> cumulativeSize_;

public:
    NonContiguousSpan(std::span<SharedQuicBuffer> buffers, std::uint64_t beginIdx, std::uint64_t endIdx)
    : buffers_(buffers), beginIdx_(beginIdx), endIdx_(endIdx)
    {
        utils::ASSERT_LOG_THROW(buffers_[0]->Length > beginIdx_,
                                "beginIdx out of bounds");

        std::uint64_t cumulativeLen = buffers_[0]->Length - beginIdx_;
        // pushes cumulativeSize_ of first buffers_.size() - 1 buffers
        for (std::size_t i = 1; i < buffers_.size(); ++i)
        {
            cumulativeSize_.push_back(cumulativeLen);
            cumulativeLen += buffers_[i]->Length;
        }

        // we have added full Length of last buffer, which is not required
        utils::ASSERT_LOG_THROW(buffers_.back()->Length >= endIdx_,
                                "endIdx out of bounds");
        cumulativeLen -= buffers_.back()->Length;
        cumulativeLen += endIdx_;
        cumulativeSize_.push_back(cumulativeLen);
    }


    NonContiguousSpan(std::span<SharedQuicBuffer> buffers, std::uint64_t beginIdx)
    : NonContiguousSpan(buffers, beginIdx, buffers.back()->Length)
    {
    }

    NonContiguousSpan(std::span<SharedQuicBuffer> buffers)
    : NonContiguousSpan(buffers, 0, buffers.back()->Length)
    {
    }

    std::uint64_t size() const noexcept
    {
        return cumulativeSize_.back();
    }

    std::uint8_t& at(std::uint64_t index)
    {
        if (index >= size())
            throw std::runtime_error("Index out of bounds, at()");

        return (*this)[index];
    }

    const std::uint8_t& at(std::uint64_t index) const
    {
        if (index >= size())
            throw std::runtime_error("Index out of bounds, at()");

        return (*this)[index];
    }

    std::uint8_t& operator[](std::uint64_t index) noexcept
    {
        return const_cast<std::uint8_t&>(static_cast<const NonContiguousSpan&>(*this)[index]);
    }

    const std::uint8_t& operator[](std::uint64_t index) const noexcept
    {
        std::size_t quicBufferIdx =
        std::upper_bound(cumulativeSize_.begin(), cumulativeSize_.end(), index) -
        cumulativeSize_.begin();

        std::uint64_t idxInSpecificBuffer = index;
        if (quicBufferIdx != 0)
            idxInSpecificBuffer -= cumulativeSize_[quicBufferIdx - 1];
        else
            idxInSpecificBuffer += beginIdx_;

        return buffers_[quicBufferIdx]->Buffer[idxInSpecificBuffer];
    }

    // we are going for linear algorithm because we expect numBytes to be small
    void advance_begin(std::uint64_t numBytes) noexcept
    {
        if (numBytes >= size())
        {
            beginIdx_ = 0;
            endIdx_ = 0;
            cumulativeSize_.clear();
            return;
        }

        std::uint64_t newBeginIdx = beginIdx_ + numBytes;
        std::size_t quicBufferIdx = 0;
        while (newBeginIdx >= buffers_[quicBufferIdx]->Length)
        {
            newBeginIdx -= buffers_[quicBufferIdx]->Length;
            ++quicBufferIdx;
        }

        beginIdx_ = newBeginIdx;
        cumulativeSize_.erase(cumulativeSize_.begin(), cumulativeSize_.begin() + quicBufferIdx);
        buffers_ = buffers_.subspan(quicBufferIdx);
        // TODO: make sure simd execution
        for (std::uint64_t& i : cumulativeSize_)
            i -= numBytes;
    }

    // copies numBytes from src to this span starting from beginIdx
    void copy_into(std::uint64_t copyAtIdx, std::uint8_t* src, std::uint64_t numBytes) noexcept
    {
        for (std::uint64_t i = 0; i < numBytes; ++i)
            operator[](copyAtIdx + i) = src[i];
    };

    void copy_into(std::uint8_t* src, std::uint64_t numBytes) noexcept
    {
        copy_into(0, src, numBytes);
    };

    void copy_to(void* dst, std::uint64_t numBytes, std::uint64_t copyFromBeginIdx) const noexcept
    {
        for (std::uint64_t i = 0; i < numBytes; ++i)
            static_cast<std::uint8_t*>(dst)[i] = operator[](i + copyFromBeginIdx);
    };
    void copy_to(void* dst, std::uint64_t numBytes) const noexcept
    {
        copy_to(dst, numBytes, 0);
    };
};
} // namespace rvn::serialization
