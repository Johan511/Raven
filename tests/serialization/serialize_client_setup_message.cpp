#include "test_serialization_utils.hpp"
#include <serialization/chunk.hpp>
#include <serialization/deserialization_impl.hpp>
#include <serialization/messages.hpp>
#include <serialization/serialization_impl.hpp>

using namespace rvn;
using namespace rvn::serialization;

void test1()
{
    ClientSetupMessage msg;
    msg.supportedVersions_.push_back(0x12345678);
    msg.supportedVersions_.push_back(0x87654321);

    ds::chunk c;
    serialization::detail::serialize(c, msg);

    // clang-format off
    //  [ 01000000 01000000 ]      00001110          00000010            [ 10010010 00110100 01010110 01111000 ] [ 11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001 ]     00000000
    // ( quic_msg_type: 0x40 )  (msglen = 14)  (num supported versions)            (supported version 1)                              (supported version 2)                                   (num parameters)
    std::string expectedSerializationString = "[01000000 01000000] [00001110] [00000010] [10010010 00110100 01010110 01111000] [11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001] [00000000]";
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

    utils::ASSERT_LOG_THROW(header.messageType_ ==
                            rvn::MoQtMessageType::CLIENT_SETUP,
                            "Header type mismatch\n", "Expected: ",
                            utils::to_underlying(rvn::MoQtMessageType::CLIENT_SETUP),
                            "\n", "Actual: ", utils::to_underlying(header.messageType_),
                            "\n");

    ClientSetupMessage deserialized;
    serialization::detail::deserialize(deserialized, span);

    utils::ASSERT_LOG_THROW(msg == deserialized, "Deserialization failed", "\n",
                            "Expected: ", msg, "\n", "Actual: ", deserialized, "\n");
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
