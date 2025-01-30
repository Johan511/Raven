#pragma once

#include "serialization/quic_var_int.hpp"
#include <msquic.h>
#include <non_contiguous_span.hpp>
#include <serialization/deserialization_impl.hpp>
#include <serialization/messages.hpp>
#include <serialization/serialization_impl.hpp>
#include <shared_mutex>
#include <stdexcept>
#include <utilities.hpp>
#include <utility>
#include <wrappers.hpp>

namespace rvn::serialization
{

/*
    As we get buffers from the network, we append them to the
    deserializer queue. The deserializer reads bytes from the buffers
    and construccts the appropriate message and pushes it into the
    message queue
*/
template <typename DeserializedMessageHandler> class Deserializer
{
    std::vector<SharedQuicBuffer> quicBuffers_;
    std::shared_mutex quicBuffersMutex_;

    // begin index in first buffer
    std::uint64_t beginIndex_ = 0;


    enum class DeserializerState
    {
        READING_MESSAGE_TYPE,
        READING_MESSAGE_LENGTH,
        READING_MESSAGE
    };

    DeserializerState state_ = DeserializerState::READING_MESSAGE_TYPE;

    // set after reading message type
    depracated::messages::MoQtMessageType messageType_;

    // set after reading message length
    std::uint64_t messageLength_ = 0;

    using MessageType =
    std::variant<depracated::messages::ClientSetupMessage, depracated::messages::ServerSetupMessage, depracated::messages::SubscribeMessage>;

    DeserializedMessageHandler messageHandler_;

    // should be called after numBytes have been deserialized
    void bytes_deserialized_hook(std::uint64_t numBytes)
    {
        // advance begin index
        beginIndex_ += numBytes;
        auto iter = quicBuffers_.begin();
        while (iter != quicBuffers_.end() && beginIndex_ >= (*iter)->Length)
        {
            beginIndex_ -= (*iter)->Length;
            ++iter;
        }
        quicBuffers_.erase(quicBuffers_.begin(), iter);
    }

    void read_message_type()
    {
        // Read first byte of buffer and determine length of the quic_var_int message type
        if (size() == 0)
            return;

        std::uint8_t prefix2Bits = at(0) >> 6;
        std::uint8_t messageTypeLength = 1 << prefix2Bits;

        if (size() < messageTypeLength)
            return;

        // get the span
        NonContiguousSpan span(quicBuffers_, beginIndex_);
        std::uint64_t messageTypeInt;

        auto numBytesDeserialized =
        detail::deserialize<ds::quic_var_int>(messageTypeInt, span);
        bytes_deserialized_hook(numBytesDeserialized);

        messageType_ = static_cast<depracated::messages::MoQtMessageType>(messageTypeInt);
        state_ = DeserializerState::READING_MESSAGE_LENGTH;

        read_message_length();
    }

    void read_message_length()
    {
        // Read first byte of buffer and determine length of the quic_var_int message type
        std::uint8_t prefix2Bits = at(0) >> 6;
        std::uint8_t messageLengthLength = 1 << prefix2Bits;

        if (size() < messageLengthLength)
            return;

        // get the span
        NonContiguousSpan span(quicBuffers_, beginIndex_);
        std::uint64_t messageLength;

        auto numBytesDeserialized =
        detail::deserialize<ds::quic_var_int>(messageLength, span);
        bytes_deserialized_hook(numBytesDeserialized);

        messageLength_ = messageLength;
        state_ = DeserializerState::READING_MESSAGE;

        read_message();
    }

    void read_message()
    {
        if (size() < messageLength_)
            return;

        // get the span
        NonContiguousSpan span(quicBuffers_, beginIndex_);

        std::uint64_t numBytesDeserialized = 0;

        if (messageType_ == depracated::messages::MoQtMessageType::CLIENT_SETUP)
        {
            depracated::messages::ClientSetupMessage msg;
            numBytesDeserialized = detail::deserialize(msg, span);
            messageHandler_(std::move(msg));
        }
        else if (messageType_ == depracated::messages::MoQtMessageType::SERVER_SETUP)
        {
            depracated::messages::ServerSetupMessage msg;
            numBytesDeserialized = detail::deserialize(msg, span);
            messageHandler_(std::move(msg));
        }
        else if (messageType_ == depracated::messages::MoQtMessageType::SUBSCRIBE)
        {
            depracated::messages::SubscribeMessage msg;
            numBytesDeserialized = detail::deserialize(msg, span);
            messageHandler_(std::move(msg));
        }
        else
        {
            utils::ASSERT_LOG_THROW(false, "Unsuppored message type",
                                    utils::to_underlying(messageType_));
        }

        bytes_deserialized_hook(numBytesDeserialized);
        state_ = DeserializerState::READING_MESSAGE_TYPE;

        // proceed to next state in cycle
        read_message_type();
        return;
    }


public:
    Deserializer(DeserializedMessageHandler&& msgHandler)
    : messageHandler_(std::move(msgHandler))
    {
    }

    Deserializer(const DeserializedMessageHandler& msgHandler)
    : messageHandler_(msgHandler)
    {
    }

    Deserializer()
    {
    }

    void append_buffer(SharedQuicBuffer buffer)
    {
        {
            // writer lock
            std::unique_lock<std::shared_mutex> lock(quicBuffersMutex_);
            quicBuffers_.push_back(buffer);
        }

        process_state_machine_input();
    }

    std::uint8_t& at(std::size_t index)
    {
        // reader lock
        std::shared_lock<std::shared_mutex> lock(quicBuffersMutex_);

        if (quicBuffers_.size() == 0)
            throw std::runtime_error("No buffers to read from, at()");
        // TODO: add begin index
        std::size_t bufferMaxIdx = quicBuffers_.front()->Length;
        auto bufferIter = quicBuffers_.begin();
        while (index >= bufferMaxIdx)
        {
            ++bufferIter;
            if (bufferIter == quicBuffers_.end())
                throw std::runtime_error("Index out of bounds, at()");
            bufferMaxIdx += (*bufferIter)->Length;
        }

        return (*bufferIter)->Buffer[index - bufferMaxIdx + (*bufferIter)->Length];
    }

    std::uint64_t size() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(const_cast<std::shared_mutex&>(quicBuffersMutex_));
        std::uint64_t totalLen = 0;
        for (const auto& buffer : quicBuffers_)
            totalLen += buffer->Length;

        totalLen -= beginIndex_;
        return totalLen;
    }

    void process_state_machine_input()
    {
        utils::ASSERT_LOG_THROW(quicBuffers_.size(),
                                "Expected at least one buffer");

        if (state_ == DeserializerState::READING_MESSAGE_TYPE)
            read_message_type();
        else if (state_ == DeserializerState::READING_MESSAGE_LENGTH)
            read_message_length();
        else if (state_ == DeserializerState::READING_MESSAGE)
            read_message();
    }
};
} // namespace rvn::serialization
