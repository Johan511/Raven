#include "test_serialization_utils.hpp"
#include <cassert>
#include <iostream>
#include <serialization/chunk.hpp>
#include <serialization/deserialization_impl.hpp>
#include <serialization/messages.hpp>
#include <serialization/serialization_impl.hpp>
#include <utilities.hpp>

using namespace rvn;
using namespace rvn::serialization;

void test_serialize_subscribe_error()
{
    SubscribeUpdateMessage msg;
    ds::chunk c;
    msg.requestId_ = 0x12345678;
    GroupId group(0x87654321);
    ObjectId object(0x11111111);
    msg.startLocation_.group_ = group;
    msg.startLocation_.object_ = object;
    msg.endGroup_ = 0;
    msg.subscriberPriority_ = 255;
    msg.forward_ = 100;
    Parameter param;
    msg.parameters_.push_back(param);

    serialization::detail::serialize(c, msg);

    // clang-format off
    /*
            00000010         00011000     10010010 00110100 01010110 01111000        11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001 
        [msg type 0x02]    [msg len 18]         [requestId 0x12345678]                                              [groupId 0x87654321]          

         10010001 00010001 00010001 00010001    00000000            11111111              01100100              00000001                  00000011                           
                [objectId 0x11111111]         [endGroup 0]   [subscriberPriority 255]   [forward 100]   [Number of Parameters 1]   [Parameter Type 0x03]

             00000001           00000000
        [Parameter Length]    [Timeout 0]
    
    */
    std::string expectedSerializationString = "00000010 00011000 10010010 00110100 01010110 01111000 11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001 10010001 00010001 00010001 00010001 00000000 11111111 01100100 00000001 00000011 00000001 00000000";
    // // clang-format on
    auto expectedSerialization = binary_string_to_vector(expectedSerializationString);

    utils::ASSERT_LOG_THROW(c.size() == expectedSerialization.size(), "Size mismatch\n",
                            "Expected size: ", expectedSerialization.size(),
                            "\n", "Actual size: ", c.size(), "\n");
    for (std::size_t i = 0; i < c.size(); i++)
        utils::ASSERT_LOG_THROW(c[i] == expectedSerialization[i], "Mismatch at index: ", i,
                                "\n", "Expected: ", int(expectedSerialization[i]),
                                "\n", "Actual: ", int(c[i]), "\n");

    ds::ChunkSpan span(c);

    ControlMessageHeader header;
    serialization::detail::deserialize(header, span);

    utils::ASSERT_LOG_THROW(header.messageType_ == MoQtMessageType::SUBSCRIBE_UPDATE,
                            "Message type mismatch\n", "Expected: ",
                            utils::to_underlying(MoQtMessageType::SUBSCRIBE_UPDATE), "\n",
                            "Actual: ", utils::to_underlying(header.messageType_), "\n");

    SubscribeUpdateMessage deserializedMsg;
    serialization::detail::deserialize(deserializedMsg, span);

    std::cout << "Deserialization Succesfull !!" << std::endl;
    utils::ASSERT_LOG_THROW(msg == deserializedMsg, "Deserialization failed\n",
                            "Expected: ", msg, "\n", "Actual: ", deserializedMsg, "\n");
}

void tests()
{
    try
    {
        test_serialize_subscribe_error();
    }
    catch (const std::exception& e)
    {
        std::cerr << "test failed\n";
        std::cerr << e.what() << '\n';
    }
}
int main()
{
    tests();
    return 0;
}