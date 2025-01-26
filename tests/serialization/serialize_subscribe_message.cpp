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
    depracated::messages::SubscribeMessage msg;
    msg.subscribeId_ = 0x12345678;
    msg.trackAlias_ = 0x87654321;
    msg.trackNamespace_ = { "namespace1", "namespace2" };
    msg.trackName_ = "trackName";
    msg.subscriberPriority_ = 0x12;
    msg.groupOrder_ = 0x34;
    msg.filterType_ = depracated::messages::SubscribeMessage::FilterType::AbsoluteRange;
    msg.start_ = depracated::messages::SubscribeMessage::GroupObjectPair{ 0x5678, 0x1234 };
    msg.end_ = depracated::messages::SubscribeMessage::GroupObjectPair{ 0x5678, 0x1234 };
    msg.parameters_ = { depracated::messages::Parameter{} };


    ds::chunk c;
    detail::serialize(c, msg);
    // clang-format off
    /*   [ 00000011 ]    [ 00111111 ] [ 10010010 00110100 01010110 01111000 ] [ 11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001 ]              [ 00000010 ] 
     * (msg_type: 0x03)    (len: 63)        (subscribeId_: 0x12345678)                                  (trackAlias_: 0x87654321)                            TrackNamespace Tuple num elements
     * 
     *      [ 00001010 ]        [ 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110001 ]       [ 00001010 ]        [ 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110010 ]
     * ( namespace1 len: 10 )                                       "namespace1"                                               ( namespace1 len: 10 )                                        "namespace2"                                              
     *
     *      [ 00001001 ]        [ 01110100 01110010 01100001 01100011 01101011 01001110 01100001 01101101 01100101 ]     [ 00010010 ]          [ 00110100 ]     [ 00000100 ] 
     *  ( trackName len: 9 )                                              "trackName"                                 (subscriberPriority_)   ( groupOrder_ )   (filterType_)
     *
     * [ 10000000 00000000 01010110 01111000 ] [ 01010010 00110100 ] [ 10000000 00000000 01010110 01111000 ] [ 01010010 00110100 ]       [ 00000001 ]           [ 00000000 ]           [ 00000000 ]
     *              (start_.group_)                 (start_.object_)              (end_.group_)                 (end_.object_)        (parameters_.size)      (parameterType_)    (parameterValue_.size)
     */
    std::string expectedSerializationString = "00000011 00111111 10010010 00110100 01010110 01111000 11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001 00000010 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110001 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110010 00001001 01110100 01110010 01100001 01100011 01101011 01001110 01100001 01101101 01100101 00010010 00110100 00000100 10000000 00000000 01010110 01111000 01010010 00110100 10000000 00000000 01010110 01111000 01010010 00110100 00000001 00000000 00000000";
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

    depracated::messages::ControlMessageHeader header;
    detail::deserialize(header, span);

    utils::ASSERT_LOG_THROW(header.messageType_ == depracated::messages::MoQtMessageType::SUBSCRIBE,
                            "Message type mismatch\n", "Expected: ",
                            utils::to_underlying(depracated::messages::MoQtMessageType::SUBSCRIBE),
                            "\n", "Actual: ", utils::to_underlying(header.messageType_),
                            "\n");

    depracated::messages::SubscribeMessage deserializedMsg;
    detail::deserialize(deserializedMsg, span);

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
