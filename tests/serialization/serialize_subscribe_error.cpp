#include "test_serialization_utils.hpp"
#include <utilities.hpp>
#include <serialization/chunk.hpp>
#include <serialization/serialization_impl.hpp>
#include <serialization/deserialization_impl.hpp>
#include <serialization/messages.hpp>
#include <cassert>
#include <iostream>


using namespace rvn;
using namespace rvn::serialization;

void test_serialize_subscribe_error()
{
    //// Create a SubscribeErrorMessage object with sample data for a 404 error
    SubscribeErrorMessage msg;
    ds::chunk c;
    msg.subscribeId_ = 0x12345678;
    msg.errorCode_ = 404;
    msg.reasonPhraseLength_ = 21;
    msg.reasonPhrase_ = "Server Not Reachable";
    msg.trackAlias_ = 0x87654321;
    

    // Serialize the object
    serialization::detail::serialize(c, msg);
    // Ensure span is created correctly

    // ANALYSIS OF CHUNK


    /*
        raw chunk
        "00000101 00100100 10010010 00110100 01010110 01111000 01000001 10010100 00010101 00010100
        01010011 01100101 01110010 01110110 01100101 01110010 00100000 01001110 01101111 01110100 00100000 01010010 01100101 01100001 01100011 
        01101000 01100001 01100010 01101100 01100101 11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001"
    */

   /*
    std::string expectedSerializationString = "
    
    
    
    00000101         00100100      10010010 00110100 01010110 01111000        01000001 10010100       00010101 
    [msg type 0x05]  [msg len 36]         [ subsid 0x12345678]             [error code 404 ]       [reasonPhraseLength_ 21]

     
     -00010100- length of reasonPhrase_
     
     message
     01010011 01100101 01110010 01110110 01100101 01110010 00100000 01001110 01101111 01110100 00100000 01010010 01100101 01100001 01100011 01101000 01100001 01100010 01101100 01100101 
     [ reasonPhrase_ = "Server Not Reachable" ]

     11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001 
             [trackAlias_ = 0x87654321 ]

     ";
*/

    
    std::string expectedSerializationString = "00000101 00100100 10010010 00110100 01010110 01111000 01000001 10010100 00010101 00010100 01010011 01100101 01110010 01110110 01100101 01110010 00100000 01001110 01101111 01110100 00100000 01010010 01100101 01100001 01100011 01101000 01100001 01100010 01101100 01100101 11000000 00000000 00000000 00000000 10000111 01100101 01000011 00100001";
   


    auto expectedSerialization = binary_string_to_vector(expectedSerializationString);

    std::cout<< "Serialized values in Chunk:" << '\n';
    for (int i = 0; i < c.size(); i++) {
        std::uint8_t temp = c[i];
        for (int j = 7; j >= 0; j--) {  
            std::cout << ((temp >> j) & 1);  
        }
        std::cout << ' ';
    }

    
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

    utils::ASSERT_LOG_THROW(header.messageType_ == MoQtMessageType::SUBSCRIBE_ERROR,
                            "Message type mismatch\n", "Expected: ",
                            utils::to_underlying(MoQtMessageType::SUBSCRIBE_ERROR),
                            "\n", "Actual: ", utils::to_underlying(header.messageType_),
                            "\n");

    SubscribeErrorMessage deserializedMsg;
    serialization::detail::deserialize(deserializedMsg, span);

    utils::ASSERT_LOG_THROW(msg == deserializedMsg, "Deserialization failed\n",
                            "Expected: ", msg, "\n", "Actual: ", deserializedMsg, "\n");

}


void tests(){
    try
    {
       test_serialize_subscribe_error();
    }
    catch(const std::exception& e)
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