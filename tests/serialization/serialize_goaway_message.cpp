#include "test_serialization_utils.hpp"
#include <serialization/chunk.hpp>
#include <serialization/deserialization_impl.hpp>
#include <serialization/messages.hpp>
#include <serialization/serialization_impl.hpp>

using namespace rvn;
using namespace rvn::serialization;

void test1()
{
    depracated::messages::GoAwayMessage msg;
    msg.newSessionURI_="moqt://abcdef";
    ds::chunk c;
    detail::serialize(c, msg);
    // clang-format off
    //  [ 00010000 ]            [00001110]     [00001101]           [01101101 01101111 01110001 01110100 00111010 00101111 00101111 01100001 01100010 01100011 01100100 01100101 01100110]
    // ( quic_msg_type: 0x16 )  (msglen=16)  (newsessionURI_len=13)       (newsessionURI)
 
    std::string expectedSerializationString = "[00010000] [00001110] [00001101] [01101101 01101111 01110001 01110100 00111010 00101111 00101111 01100001 01100010 01100011 01100100 01100101 01100110]";

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

    utils::ASSERT_LOG_THROW(header.messageType_ ==
                            rvn::depracated::messages::MoQtMessageType::GOAWAY,
                            "Header type mismatch\n", "Expected: ",
                            utils::to_underlying(rvn::depracated::messages::MoQtMessageType::GOAWAY),
                            "\n", "Actual: ", utils::to_underlying(header.messageType_),
                            "\n");

    depracated::messages::GoAwayMessage deserialized;
    detail::deserialize(deserialized, span);

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

