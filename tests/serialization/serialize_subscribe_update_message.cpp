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
    depracated::messages::SubscribeUpdateMessage msg;
    ds::chunk c;

    msg.subscribeId_ = 123456789;
    msg.startGroup_ = 987654321;
    msg.startObject_ = 111111111;
    msg.endGroup_ = 222222222;
    msg.endObject_ = 333333333;
    msg.subscriberPriority_ = 255;
    
    rvn::depracated::messages::Parameter param;
    param.parameterType_ = static_cast<rvn::depracated::messages::ParameterType>(0);
    param.parameterValue_ = "hello";
    msg.parameters_.push_back(param); 

    detail::serialize(c, msg);
    // clang-format off
    //     [ 00000010 ]        [ 00011101 ]     [ 10000111 ][ 01011011 ][ 11001101 ][ 00010101]   
    // (quic_msg_type: 0x2)    (msglen = 29)                     (SubscribeID)       
    //     [ 00111010 ][ 11011110 ][ 01101000 ][ 10110001 ]   [ 00000110 ][ 10011111 ][ 01101011 ][ 11000111 ]
    //                      (StartGroup)                                        (StartObject)
    //     [ 10001101 ][ 00111110 ][ 11010111 ][ 10001110 ]   [ 10010011 ][ 11011110 ][ 01000011 ][ 01010101 ]
    //                      (EndGroup)                                          (EndObject)
    //     [ 11111111 ]            [ 00000001 ]          [ 00000000 ]            [ 00000101 ]          
    //  (SubscriberPriority)  (Number of Parameters)    (ParameterType)     (ParamaterValueLength = 5)
    //     [ 01101000 ][ 01100101 ][ 01101100 ][ 01101100 ][ 01101111 ]
    //                          (ParameterValue) 
    std::string expectedSerializationString = "[ 00000010 ][ 00011101 ][ 10000111 ][ 01011011 ][ 11001101 ][ 00010101 ][ 10111010 ] \
                                               [ 11011110 ][ 01101000 ][ 10110001 ][ 10000110 ][ 10011111 ][ 01101011 ][ 11000111 ] \
                                               [ 10001101 ][ 00111110 ][ 11010111 ][ 10001110 ][ 10010011 ][ 11011110 ][ 01000011 ] \
                                               [ 01010101 ][ 11111111 ][ 00000001 ][ 00000000 ][ 00000101 ][ 01101000 ][ 01100101 ] \
                                               [ 01101100 ][ 01101100 ][ 01101111 ]";
    // // clang-format on
    auto expectedSerialization = binary_string_to_vector(expectedSerializationString);
    
    // DEBUG

    std::cout<< "Serialized values in Chunk:" << '\n';
    for (int i = 0; i < c.size(); i++) {
        std::uint8_t temp = c[i];
        for (int j = 7; j >= 0; j--) {  
            std::cout << ((temp >> j) & 1);  
        }
        std::cout << '\n';
    }
    
    ///

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

    utils::ASSERT_LOG_THROW(header.messageType_ == depracated::messages::MoQtMessageType::SUBSCRIBE_UPDATE,
                            "Message type mismatch\n", "Expected: ",
                            utils::to_underlying(depracated::messages::MoQtMessageType::SUBSCRIBE_UPDATE),
                            "\n", "Actual: ", utils::to_underlying(header.messageType_),
                            "\n");

    depracated::messages::SubscribeUpdateMessage deserializedMsg;
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
