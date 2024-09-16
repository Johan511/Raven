#pragma once

///////////////////////////////////c
#include "utilities.hpp"
#include <msquic.h>
///////////////////////////////////
#include <google/protobuf/util/delimited_message_util.h>
#include <setup_messages.pb.h>
///////////////////////////////////
#include <algorithm>
///////////////////////////////////
#include <protobuf_messages.hpp>
///////////////////////////////////

namespace rvn::serialization
{
// do not use templates here, we are not preventing any duplication by using
// templatees and knowing type helps catch errors easily while compiling (do not
// have to wait for template to be instantiated)
using namespace protobuf_messages;

template <typename T, typename InputStream> T deserialize(InputStream& istream)
{
    T t;
    bool clean_eof;
    google::protobuf::util::ParseDelimitedFromZeroCopyStream(&t, &istream, &clean_eof);
    if (clean_eof)
        throw std::runtime_error("Failed to parse message");
    return t;
};

} // namespace rvn::serialization
