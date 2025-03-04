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
    TrackStatusRequestMessage msg;
    msg.trackNamespace_ = "h";
    msg.trackName_ = "i";

    ds::chunk c;
    serialization::detail::serialize(c, msg);
    // clang-format off
    // [ 00001101 ]            [ 00000100 ]   [ 00000001 01101000 ]     [ 00000001 01101001 ]
    // (quic_msg_type: 0xD)    (msglen = 4)     (trackNameSpace)             (trackName)
    std::string expectedSerializationString = "[00001101][00000100][00000001 01101000][00000001 01101001]";
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

    utils::ASSERT_LOG_THROW(header.messageType_ == MoQtMessageType::TRACK_STATUS_REQUEST,
                            "Message type mismatch\n", "Expected: ",
                            utils::to_underlying(MoQtMessageType::TRACK_STATUS_REQUEST),
                            "\n", "Actual: ", utils::to_underlying(header.messageType_),
                            "\n");

    TrackStatusRequestMessage deserializedMsg;
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