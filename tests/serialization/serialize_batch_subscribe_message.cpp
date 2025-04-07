#include "strong_types.hpp"
#include "test_serialization_utils.hpp"
#include "utilities.hpp"
#include <serialization/chunk.hpp>
#include <serialization/deserialization_impl.hpp>
#include <serialization/messages.hpp>
#include <serialization/serialization_impl.hpp>

using namespace rvn;
using namespace rvn::serialization;


void test1()
{
    // clang-format off
    BatchSubscribeMessage msg;
    msg.trackNamespacePrefix_ = { "namespace1", "namespace2" };

    SubscribeMessage subMsg1;
    subMsg1.subscribeId_ = 0x12345678;
    subMsg1.trackAlias_ = TrackAlias(0x87654321);
    subMsg1.trackNamespace_ = { "namespace1", "namespace2" };
    subMsg1.trackName_ = "trackName";
    subMsg1.subscriberPriority_ = 0x12;
    subMsg1.groupOrder_ = 0x34;
    subMsg1.filterType_ = SubscribeFilterType::AbsoluteRange;
    subMsg1.start_ = GroupObjectPair{ GroupId(0x5678), ObjectId(0x1234) };
    subMsg1.end_ = GroupObjectPair{ GroupId(0x5678), ObjectId(0x1234) };
    subMsg1.parameters_ = { Parameter{DeliveryTimeoutParameter{std::chrono::milliseconds(100)}} };

    SubscribeMessage subMsg2;
    subMsg2.subscribeId_ = 0x12345678;
    subMsg2.trackAlias_ = TrackAlias(0x87654321);
    subMsg2.trackNamespace_ = { "namespace1", "namespace2" };
    subMsg2.trackName_ = "trackName";
    subMsg2.subscriberPriority_ = 0x12;
    subMsg2.groupOrder_ = 0x34;
    subMsg2.filterType_ = SubscribeFilterType::AbsoluteRange;
    subMsg2.start_ = GroupObjectPair{ GroupId(0x5678), ObjectId(0x1234) };
    subMsg2.end_ = GroupObjectPair{ GroupId(0x5678), ObjectId(0x1234) };
    subMsg2.parameters_ = { Parameter{DeliveryTimeoutParameter{std::chrono::milliseconds(100)}} };

    msg.subscriptions_ = {subMsg1, subMsg2};
    // clang-format on

    ds::chunk c;
    serialization::detail::serialize(c, msg);

    // clang-format off
    const std::string_view expectedSerializationString = 
    // [msg type]     [ msg len ]    [num numspaces]                                [ namespace 1 length + text ] 
    //  00010001   01000000 10011010    00000010      [ 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110001 ] 
    //                                      [ namespace 2 length + text ] 
    // [ 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110010 ] 
    // [num subscriptions] 
    //      00000010         
    // [ subscribe messages and parameters ] NOTE: subscribe messages do not have header for obvious reasons
    // 10010010 00110100 01010110 01111000 11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001 00000010 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110001 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110010 00001001 01110100 01110010 01100001 01100011 01101011 01001110 01100001 01101101 01100101 00010010 00110100 00000100 10000000 00000000 01010110 01111000 01010010 00110100 10000000 00000000 01010110 01111000 01010010 00110100 00000001 00000011 00000010 01000000 01100100 10010010 00110100 01010110 01111000 11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001 00000010 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110001 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110010 00001001 01110100 01110010 01100001 01100011 01101011 01001110 01100001 01101101 01100101 00010010 00110100 00000100 10000000 00000000 01010110 01111000 01010010 00110100 10000000 00000000 01010110 01111000 01010010 00110100 00000001 00000011 00000010 01000000 01100100";

    "00010001 01000000 10011010 00000010 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110001 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110010 00000010 10010010 00110100 01010110 01111000 11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001 00000010 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110001 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110010 00001001 01110100 01110010 01100001 01100011 01101011 01001110 01100001 01101101 01100101 00010010 00110100 00000100 10000000 00000000 01010110 01111000 01010010 00110100 10000000 00000000 01010110 01111000 01010010 00110100 00000001 00000011 00000010 01000000 01100100 10010010 00110100 01010110 01111000 11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001 00000010 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110001 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110010 00001001 01110100 01110010 01100001 01100011 01101011 01001110 01100001 01101101 01100101 00010010 00110100 00000100 10000000 00000000 01010110 01111000 01010010 00110100 10000000 00000000 01010110 01111000 01010010 00110100 00000001 00000011 00000010 01000000 01100100";
    // clang-format on

    auto expectedSerialization = binary_string_to_vector(expectedSerializationString);
    utils::ASSERT_LOG_THROW(c.size() == expectedSerialization.size(), "Size mismatch\n",
                            "Expected size: ", expectedSerialization.size(),
                            "\n", "Actual size: ", c.size(), "\n");
    for (std::size_t i = 0; i < c.size(); i++)
        utils::ASSERT_LOG_THROW(c[i] == expectedSerialization[i], "Mismatch at index: ", i,
                                "\n", "Expected: ", expectedSerialization[i],
                                "\n", "Actual: ", c[i], "\n");

    ds::ChunkSpan span(c);

    ControlMessageHeader header;
    serialization::detail::deserialize(header, span);

    utils::ASSERT_LOG_THROW(header.messageType_ == MoQtMessageType::BATCH_SUBSCRIBE,
                            "Message type mismatch\n", "Expected: ",
                            utils::to_underlying(MoQtMessageType::BATCH_SUBSCRIBE), "\n",
                            "Actual: ", utils::to_underlying(header.messageType_), "\n");

    BatchSubscribeMessage deserializedMsg;
    serialization::detail::deserialize(deserializedMsg, span);

    utils::ASSERT_LOG_THROW(msg == deserializedMsg, "Deserialization failed\n",
                            "Expected: ", msg, "\n", "Actual: ", deserializedMsg, "\n");
}

void tests()
{
    try
    {
        test1();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Test failed\n";
        std::cerr << e.what() << std::endl;
    }
}


int main()
{
    tests();
    return 0;
}
