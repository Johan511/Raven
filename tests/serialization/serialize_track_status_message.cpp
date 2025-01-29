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
    depracated::messages::TrackStatusMessage msg;
    msg.trackNamespace_ = { "namespace1", "namespace2" };
    msg.trackName_ = "trackName";
    msg.statusCode_ = depracated::messages::TrackStatusMessage::StatusCode::InProgress;
    msg.lastgroupId_ = 0x12345678;
    msg.lastobjectId_ = 0x87654321;

    ds::chunk c;
    detail::serialize(c, msg);
    // clang-format off
    /*   [ 00001110 ]    [ 00101110 ]            [ 00000010 ]                       [ 00001010 ]            [ 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110001 ]
     * (msg_type: 0x0E)    (len: 46)    TrackNamespace Tuple num elements       ( namespace1 len: 10 )                                               "namespace1"
     * 
     *      [ 00001010 ]        [ 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110010 ]      [ 00001001 ]
     * ( namespace2 len: 10 )                                        "namespace2"                                              ( trackName len: 9 )
     *
     *  [ 01110100 01110010 01100001 01100011 01101011 01001110 01100001 01101101 01100101 ]         [ 00000000 ] 
     *                                        "trackName"                                            (statusCode_)
     *
     * [ 10010010 00110100 01010110 01111000 ] [ 11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001 ]
     *      (lastgroupId_: 0x12345678)                                 (lastobjectId_: 0x87654321)    
     */
    std::string expectedSerializationString = "00001110 00101110 00000010 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110001 00001010 01101110 01100001 01101101 01100101 01110011 01110000 01100001 01100011 01100101 00110010 00001001 01110100 01110010 01100001 01100011 01101011 01001110 01100001 01101101 01100101 00000000 10010010 00110100 01010110 01111000 11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001";
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

    utils::ASSERT_LOG_THROW(header.messageType_ == depracated::messages::MoQtMessageType::TRACK_STATUS,
                            "Message type mismatch\n", "Expected: ",
                            utils::to_underlying(depracated::messages::MoQtMessageType::TRACK_STATUS),
                            "\n", "Actual: ", utils::to_underlying(header.messageType_),
                            "\n");

    depracated::messages::TrackStatusMessage deserializedMsg;
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