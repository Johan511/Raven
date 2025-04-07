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
    ServerSetupMessage msg;
    msg.selectedVersion_ = 0x12345678;

    ds::chunk c;
    serialization::detail::serialize(c, msg);
    // clang-format off
    // [ 01000000 01000001 ]    [ 00000101 ] [ 10010010 00110100 01010110 01111000 ]     [ 00000000 ]
    // (quic_msg_type: 0x41)    (msglen = 5)          (selected version)               (num parameters)
    std::string expectedSerializationString = "[01000000 01000001][00000101][10010010 00110100 01010110 01111000][00000000]";
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

    utils::ASSERT_LOG_THROW(header.messageType_ == MoQtMessageType::SERVER_SETUP,
                            "Message type mismatch\n", "Expected: ",
                            utils::to_underlying(MoQtMessageType::SERVER_SETUP), "\n",
                            "Actual: ", utils::to_underlying(header.messageType_), "\n");

    ServerSetupMessage deserializedMsg;
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
