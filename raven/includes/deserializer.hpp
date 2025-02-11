#pragma once
///////////////////////////////////////////////////////////////////////////////
#include "strong_types.hpp"
#include <limits>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
///////////////////////////////////////////////////////////////////////////////
#include <msquic.h>
#include <non_contiguous_span.hpp>
#include <serialization/deserialization_impl.hpp>
#include <serialization/messages.hpp>
#include <serialization/quic_var_int.hpp>
#include <serialization/serialization_impl.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>
///////////////////////////////////////////////////////////////////////////////


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

    enum class DeserializerType : bool
    {
        DATA_STREAM,
        CONTROL_STREAM
    };

    enum class DeserializerState
    {
        // for control stream messages
        READING_MESSAGE_TYPE,
        READING_MESSAGE_LENGTH,
        READING_MESSAGE,

        // for data stream messages
        READING_OBJECT_HEADER, // reading the header (first message on data stream)
                               // rest of the messages are on one of these formats (based on header)
        READING_OBJECT_DATAGRAM,
        READING_SUBGROUP_OBJECT,
        READING_FETCH_OBJECT
    };


    DeserializedMessageHandler messageHandler_;
    DeserializerType type_;
    DeserializerState state_;

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

    std::uint64_t read_quic_var_int()
    {
        if (size() == 0)
            return std::numeric_limits<std::uint64_t>::max();

        std::uint8_t prefix2Bits = at(0) >> 6;
        std::uint8_t quicVarIntLength = 1 << prefix2Bits;

        if (size() < quicVarIntLength)
            return std::numeric_limits<std::uint64_t>::max();

        // get the span
        NonContiguousSpan span(quicBuffers_, beginIndex_);
        std::uint64_t quicVarInt;

        auto numBytesDeserialized =
        detail::deserialize<ds::quic_var_int>(quicVarInt, span);
        bytes_deserialized_hook(numBytesDeserialized);

        return quicVarInt;
    }

    ////////////////////////////////////////////////////////////////////////////
    // control message related
    // set after reading control message type
    MoQtMessageType messageType_;
    // set after reading control message length
    std::uint64_t messageLength_ = 0;
    void read_message_type()
    {
        std::uint64_t messageTypeInt;
        std::uint64_t messageTypeOpt = read_quic_var_int();
        // if we don't have enough bytes to read the message type
        if (messageTypeOpt == std::numeric_limits<std::uint64_t>::max())
            return;
        messageTypeInt = messageTypeOpt;

        messageType_ = static_cast<MoQtMessageType>(messageTypeInt);
        state_ = DeserializerState::READING_MESSAGE_LENGTH;

        read_message_length();
    }

    void read_message_length()
    {
        std::uint64_t messageLengthOpt = read_quic_var_int();
        // if we don't have enough bytes to read the message length
        if (messageLengthOpt == std::numeric_limits<std::uint64_t>::max())
            return;
        messageLength_ = messageLengthOpt;
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

        if (messageType_ == MoQtMessageType::CLIENT_SETUP)
        {
            ClientSetupMessage msg;
            numBytesDeserialized = detail::deserialize(msg, span);
            messageHandler_(std::move(msg));
        }
        else if (messageType_ == MoQtMessageType::SERVER_SETUP)
        {
            ServerSetupMessage msg;
            numBytesDeserialized = detail::deserialize(msg, span);
            messageHandler_(std::move(msg));
        }
        else if (messageType_ == MoQtMessageType::SUBSCRIBE)
        {
            SubscribeMessage msg;
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
    ////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////////////////////////////////////////////////
    // Data stream related
    // TODO: does deserializer need to know this?
    std::variant<std::nullopt_t, StreamHeaderSubgroupMessage> dataStreamHeader_;
    enum class ObjectStreamHeaderType
    {
        OBJECT_DATAGRAM = 0x1,
        STREAM_HEADER_SUBGROUP = 0x4,
        FETCH_HEADER = 0x5
    };
    // clang-format off
    /*
        Object Header:
        Header Id: 0x1 = OBJECT_DATAGRAM, 0x4 = STREAM_HEADER_SUBGROUP, 0x5 =FETCH_HEADER Header Content
        followed by the object header content
        STREAM_HEADER_SUBGROUP Message {
          Track Alias (i),
          Group ID (i),
          Subgroup ID (i),
          Publisher Priority (8),
        }
    */
    // clang-format on

    std::optional<TrackAlias> trackAlias_;
    std::optional<GroupId> groupId_;
    std::optional<SubGroupId> subgroupId_;
    void read_subgroup_header()
    {
        if (!trackAlias_.has_value())
        {
            std::uint64_t trackAliasInt = read_quic_var_int();
            if (trackAliasInt == std::numeric_limits<std::uint64_t>::max())
                return;
            trackAlias_ = TrackAlias(trackAliasInt);
        }

        if (!groupId_.has_value())
        {
            std::uint64_t groupIdInt = read_quic_var_int();
            if (groupIdInt == std::numeric_limits<std::uint64_t>::max())
                return;
            groupId_ = GroupId(groupIdInt);
        }

        if (!subgroupId_.has_value())
        {
            std::uint64_t subgroupIdInt = read_quic_var_int();
            if (subgroupIdInt == std::numeric_limits<std::uint64_t>::max())
                return;
            subgroupId_ = SubGroupId(subgroupIdInt);
        }

        // to read publisher priority
        if (size() < 1)
            return;

        std::uint8_t publisherPriority = at(0);
        bytes_deserialized_hook(1);

        auto msg =
        StreamHeaderSubgroupMessage{ trackAlias_.value(), groupId_.value(),
                                     subgroupId_.value(), publisherPriority };
        messageHandler_(msg);
        dataStreamHeader_ = msg;

        state_ = DeserializerState::READING_SUBGROUP_OBJECT;
        read_subgroup_object();
    }

    /*
        {
          Object ID = 0
          Object Payload Length = 4
          Payload = "abcd"
        }
    */
    std::optional<ObjectId> subGroupObjectId_;
    std::optional<std::uint64_t> subGroupObjectPayloadLength_;
    void read_subgroup_object()
    {
        // TODO: fix this buggy code
        // Why buggy? if objectId is read and length fails, when reading again, we read from objectId
        if (!subGroupObjectId_.has_value())
        {
            std::uint64_t objectId = read_quic_var_int();
            if (objectId == std::numeric_limits<std::uint64_t>::max())
                return;
            subGroupObjectId_ = ObjectId(objectId);
        }

        if (!subGroupObjectPayloadLength_.has_value())
        {
            std::uint64_t objectPayloadLength = read_quic_var_int();
            if (objectPayloadLength == std::numeric_limits<std::uint64_t>::max())
                return;
            subGroupObjectPayloadLength_ = objectPayloadLength;
        }

        if (size() < subGroupObjectPayloadLength_)
            return;

        std::string payload;
        payload.reserve(subGroupObjectPayloadLength_.value());
        for (std::uint64_t i = 0; i < subGroupObjectPayloadLength_; ++i)
            payload.push_back(at(i));
        bytes_deserialized_hook(subGroupObjectPayloadLength_.value());

        auto msg =
        StreamHeaderSubgroupObject{ subGroupObjectId_.value(), std::move(payload) };
        messageHandler_(std::move(msg));

        subGroupObjectId_ = std::nullopt;
        subGroupObjectPayloadLength_ = std::nullopt;

        read_subgroup_object();
    }

    std::optional<ObjectStreamHeaderType> dataStreamHeaderId_;
    void read_object_header()
    {
        if (!dataStreamHeaderId_.has_value())
        {
            std::uint64_t headerIdInt = read_quic_var_int();
            if (headerIdInt == std::numeric_limits<std::uint64_t>::max())
                return;

            dataStreamHeaderId_ = static_cast<ObjectStreamHeaderType>(headerIdInt);
        }

        switch (dataStreamHeaderId_.value())
        {
            case ObjectStreamHeaderType::STREAM_HEADER_SUBGROUP:
            {
                read_subgroup_header();
                break;
            }
            default:
            {
                // TODO handle OBJECT_DATAGRAM_HEADER and FETCH_HEADER
                utils::ASSERT_LOG_THROW(false, "Invalid object header",
                                        utils::to_underlying(dataStreamHeaderId_.value()));
            }
        }


        if (std::holds_alternative<std::nullopt_t>(dataStreamHeader_))
            // haven't read the object header yet
            return;
        else if (std::holds_alternative<StreamHeaderSubgroupMessage>(dataStreamHeader_))
            // read the object header
            state_ = DeserializerState::READING_SUBGROUP_OBJECT;
        else
            // TODO handle OBJECT_DATAGRAM_HEADER and FETCH_HEADER
            utils::ASSERT_LOG_THROW(false, "Invalid object header",
                                    utils::to_underlying(dataStreamHeaderId_.value()));
    }
    ////////////////////////////////////////////////////////////////////////////


    void process_state_machine_input()
    {
        std::shared_lock l(quicBuffersMutex_);
        utils::ASSERT_LOG_THROW(quicBuffers_.size(),
                                "Expected at least one buffer");

        switch (type_)
        {
            case DeserializerType::CONTROL_STREAM:
            {
                if (state_ == DeserializerState::READING_MESSAGE_TYPE)
                    read_message_type();
                else if (state_ == DeserializerState::READING_MESSAGE_LENGTH)
                    read_message_length();
                else if (state_ == DeserializerState::READING_MESSAGE)
                    read_message();
                else
                    utils::ASSERT_LOG_THROW(false, "Invalid state",
                                            utils::to_underlying(state_));
                break;
            }
            case DeserializerType::DATA_STREAM:
            {
                if (state_ == DeserializerState::READING_OBJECT_HEADER)
                    read_object_header();
                else if (state_ == DeserializerState::READING_SUBGROUP_OBJECT)
                    read_subgroup_object();
                else
                    // TOOD: implement reading OBJECT_DATAGRAM and and FETCH_HEADER
                    utils::ASSERT_LOG_THROW(false, "Invalid state",
                                            utils::to_underlying(state_));
                break;
            }
        }
    }

public:
    Deserializer(bool isControlStream, DeserializedMessageHandler messageHandler = {})
    : messageHandler_(messageHandler), dataStreamHeader_(std::nullopt)
    {
        if (isControlStream)
        {
            type_ = DeserializerType::CONTROL_STREAM;
            state_ = DeserializerState::READING_MESSAGE_TYPE;
        }
        else
        {
            type_ = DeserializerType::DATA_STREAM;
            state_ = DeserializerState::READING_OBJECT_HEADER;
        }
    }

    void append_buffer(SharedQuicBuffer buffer)
    {
        {
            // writer lock
            std::unique_lock<std::shared_mutex> lock(quicBuffersMutex_);
            quicBuffers_.push_back(buffer);
            std::cout << buffer->Length << std::endl;
        }

        process_state_machine_input();
    }

    std::uint8_t& at(std::size_t index)
    {
        // reader lock
        std::shared_lock<std::shared_mutex> lock(quicBuffersMutex_);

        if (quicBuffers_.size() == 0)
            throw std::runtime_error("No buffers to read from, at()");
        index += beginIndex_;
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
        // reader lock
        std::shared_lock<std::shared_mutex> lock(const_cast<std::shared_mutex&>(quicBuffersMutex_));
        std::uint64_t totalLen = 0;
        for (const auto& buffer : quicBuffers_)
            totalLen += buffer->Length;

        totalLen -= beginIndex_;
        return totalLen;
    }
};
} // namespace rvn::serialization
