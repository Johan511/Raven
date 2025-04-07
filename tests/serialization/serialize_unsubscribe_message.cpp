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

void test_serialize_unsubscribe()
{
    //// Create a SubscribeErrorMessage object with sample data for a 404 error
    UnsubscribeMessage msg;
    ds::chunk c;
    msg.subscribeId_ = 0x12345678;


    serialization::detail::serialize(c, msg);

    // clang-format off
    /*
           00001010          00000100     10010010 00110100 01010110 01111000   
    */
    std::string expectedSerializationString = "00001010 00100100 10010010 00110100 01010110 01111000";
    // clang-format on
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

    utils::ASSERT_LOG_THROW(header.messageType_ == MoQtMessageType::UNSUBSCRIBE,
                            "Message type mismatch\n", "Expected: ",
                            utils::to_underlying(MoQtMessageType::UNSUBSCRIBE), "\n",
                            "Actual: ", utils::to_underlying(header.messageType_), "\n");

    UnsubscribeMessage deserializedMsg;
    serialization::detail::deserialize(deserializedMsg, span);

    utils::ASSERT_LOG_THROW(msg == deserializedMsg, "Deserialization failed\n",
                            "Expected: ", msg, "\n", "Actual: ", deserializedMsg, "\n");
}


void tests()
{
    try
    {
        test_serialize_unsubscribe();
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