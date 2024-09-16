// deprecated, not using this

#pragma once
////////////////////////////////////////////
#include <msquic.h>
////////////////////////////////////////////
#include <setup_messages.pb.h>
////////////////////////////////////////////
#include <optional>
#include <variant>
////////////////////////////////////////////

namespace rvn::depracated::messages
{
// (b) type which is encoded as a variable-length integer followed by that many
// bytes of data.
using BinaryBufferData = std::string;
using iType = std::uint64_t; // variable size integer, check QUIC RFC
using MOQTVersionT = iType;

enum class MoQtMessageType
{
    OBJECT_STREAM = 0x0,
    OBJECT_DATAGRAM = 0x1,
    SUBSCRIBE_UPDATE = 0x2,
    SUBSCRIBE = 0x3,
    SUBSCRIBE_OK = 0x4,
    SUBSCRIBE_ERROR = 0x5,
    ANNOUNCE = 0x6,
    ANNOUNCE_OK = 0x7,
    ANNOUNCE_ERROR = 0x8,
    UNANNOUNCE = 0x9,
    UNSUBSCRIBE = 0xA,
    SUBSCRIBE_DONE = 0xB,
    ANNOUNCE_CANCEL = 0xC,
    TRACK_STATUS_REQUEST = 0xD,
    TRACK_STATUS = 0xE,
    GOAWAY = 0x10,
    CLIENT_SETUP = 0x40,
    SERVER_SETUP = 0x41,
    STREAM_HEADER_TRACK = 0x50,
    STREAM_HEADER_GROUP = 0x51
};

struct ControlMessageHeader
{
    MoQtMessageType messageType;
};

///////////////////////////// Parameters ///////////////////////////////

enum class ParameterType
{
    RoleParameter = 0x00,
    PathParameter = 0x01
};

enum class Role
{
    Publisher = 0x00,
    Subscriber = 0x01,
    PubSub = 0x02,
};
struct RoleParameter
{
    Role role;

    static constexpr ParameterType parameterType = ParameterType::RoleParameter;
    static constexpr std::uint32_t parameterLenth = sizeof(role);
};

// only to be used by the client
// should not be used by server or when using WebTransport
// specifies MoQ URI when using native QUIC
struct PathParameter
{
    /*
     * TODO : To keep in note when serializaation, deserialization
     * When connecting to a server using a URI with the "moq" scheme, the client
     * MUST set the PATH parameter to the path-abempty portion of the URI; if
     * query is present, the client MUST concatenate ?, followed by the query
     * portion of the URI to the parameter.
     */
    std::string path;

    static constexpr ParameterType parameterType = ParameterType::PathParameter;
    static constexpr std::uint32_t parameterLenth = sizeof(path);
};

/*
    CLIENT_SETUP Message Payload {
      Number of Supported Versions (i),
      Supported Version (i) ...,
      Number of Parameters (i) ...,
      Setup Parameters (..) ...,
    }
*/
struct ClientSetupMessage
{
    struct ParamT
    {
        RoleParameter role;
        PathParameter path;
    };

    iType numSupportedVersions;
    std::vector<MOQTVersionT> supportedVersions;
    std::vector<iType> numberOfParameters;
    std::vector<ParamT> parameters;
};

/*
    multiple number parameters in case client is using extensions
    SERVER_SETUP Message Payload {
      Selected Version (i),
      Number of Parameters (i) ...,
      Setup Parameters (..) ...,
    }
*/
struct ServerSetupMessage
{
    struct ParamT
    {
        RoleParameter role;
    };

    MOQTVersionT selectedVersion;
    iType numberOfParameters;
    std::vector<ParamT> parameters;
};

/*
    GOAWAY Message {
      New Session URI (b)
    }
*/
struct GoAwayMessage
{
    BinaryBufferData newSessionURI;
};

/*
    SUBSCRIBE Message {
      Subscribe ID (i),
      Track Alias (i),
      Track Namespace (b),
      Track Name (b),
      Subscriber Priority (8),
      Group Order (8),
      Filter Type (i),
      [StartGroup (i),
       StartObject (i)],
      [EndGroup (i),
       EndObject (i)],
      Number of Parameters (i),
      Subscribe Parameters (..) ...
    }
*/

struct SubscribeMessage
{
    enum class FilterType : iType
    {
        LatestGroup = 0x1, // Specifies an open-ended subscription with objects
                           // from the beginning of the current group.

        LatestObject = 0x2, // Specifies an open-ended subscription beginning
                            // from the current object of the current group.

        AbsoluteStart = 0x3, // Specifies an open-ended subscription beginning
                             // from the object identified in the StartGroup and
                             // StartObject fields.

        AbsoluteRange = 0x4, // Specifies a closed subscription starting at
                             // StartObject in StartGroup and ending at
                             // EndObject in EndGroup. The start and end of the
                             // range are inclusive. EndGroup and EndObject MUST
                             // specify the same or a later object than
                             // StartGroup and StartObject.
    };

    using Parameter = std::variant<>;

    iType subscribeId;
    iType trackAlias;
    BinaryBufferData trackNamespace;
    BinaryBufferData trackName;
    std::uint8_t subscriberPriority;
    std::uint8_t groupOrder;
    FilterType filterType;
    std::optional<std::pair<iType, iType>> startGroupObjectPair;
    std::optional<std::pair<iType, iType>> endGroupObjectPair;
    // iType numberOfParameters;
    std::vector<Parameter> params;
};

/*
    SUBSCRIBE_UPDATE Message {
      Subscribe ID (i),
      StartGroup (i),
      StartObject (i),
      EndGroup (i),
      EndObject (i),
      Subscriber Priority (8),
      Number of Parameters (i),
      Subscribe Parameters (..) ...
    }
*/
struct SubscribeUpdateMessage
{
    iType subscribeId;
    iType trackAlias;
    iType group;
    iType object;
    // iType numberOfParameters;
    std::vector<SubscribeMessage::Parameter> params;
};

/*
    UNSUBSCRIBE Message {
      Subscribe ID (i)
    }
*/
struct UnsubscribeMessage
{
    iType subscribeId;
};

/*
    ANNOUNCE_OK
    {
      Track Namespace (b),
    }
*/
struct AnnounceOkMessage
{
    BinaryBufferData trackNamespace;
};

/*
    ANNOUNCE_ERROR
    {
      Track Namespace (b),
      Error Code (i),
      Reason Phrase (b),
    }
*/
struct AnnounceErrorMessage
{
    BinaryBufferData trackNamespace;
    iType errorCode;
    BinaryBufferData reasonPhrase;
};

/*
    ANNOUNCE_CANCEL Message {
      Track Namespace (b),
    }
*/
struct AnnounceCancelMessage
{
    BinaryBufferData trackNamespace;
};

/*
    TRACK_STATUS_REQUEST Message {
      Track Namespace (b),
      Track Name (b),
    }
*/
struct TrackStatusRequestMessage
{
    BinaryBufferData trackNamespace;
    BinaryBufferData trackName;
};

enum class ObjectStatus : iType
{
    NORMAL = 0x0, // Normal object. The payload is array of bytes and can be
                  // empty.

    OBJECT_DNE = 0x1, // Indicates Object does not exist. Indicates that this
                      // object does not exist at any publisher and it will not
                      // be published in the future. This SHOULD be cached.

    GROUP_DNE = 0x2, // Indicates Group does not exist. Indicates that objects
                     // with this GroupID do not exist at any publisher and they
                     // will not be published in the future. This SHOULD be
                     // cached and have empty payload.

    END_OF_GROUP = 0x3, // Indicates end of Group. ObjectId is one greater that
                        // the largest object produced in the group identified
                        // by the GroupID. This is sent right after the last
                        // object in the group. This SHOULD be cached and have
                        // empty payload.

    END_OF_TRACK = 0x4, // Indicates end of Track and Group. GroupID is one
                        // greater than the largest group produced in this track
                        // and the ObjectId is one greater than the largest
                        // object produced in that group. This is sent right
                        // after the last object in the track. This SHOULD be
                        // cached and have empty payload.
};

/*
    OBJECT_STREAM Message {
      Subscribe ID (i),
      Track Alias (i),
      Group ID (i),
      Object ID (i),
      Publisher Priority (8),
      Object Status (i),
      Object Payload (..),
    }
*/
struct ObjectStreamMessage
{
    iType subscribeId;
    iType trackAlias;
    iType group;
    iType object;
    std::uint8_t publisherPriority;
    ObjectStatus objectStatus;
    BinaryBufferData objectPayload;
};

/*
    OBJECT_DATAGRAM Message {
      Subscribe ID (i),
      Track Alias (i),
      Group ID (i),
      Object ID (i),
      Publisher Priority (8),
      Object Status (i),
      Object Payload (..),
    }
*/
struct ObjectDatagramMessage
{
    iType subscribeId;
    iType trackAlias;
    iType group;
    iType object;
    std::uint8_t publisherPriority;
    ObjectStatus objectStatus;
    BinaryBufferData objectPayload;
};

/*
    STREAM_HEADER_TRACK Message {
      Subscribe ID (i)
      Track Alias (i),
      Publisher Priority (8),(i)
    }
*/
struct StreamHeaderTrackMessage
{
    iType subscribeId;
    iType trackAlias;
    std::uint8_t publisherPriority;
};

/*
    TrackStreamObject Message{
      Group ID (i),
      Object ID (i),
      Object Payload Length (i),
      [Object Status (i)],
      Object Payload (..),
    }
*/
struct TrackStreamObjectMessage
{
    iType group;
    iType object;
    iType objectPayloadLength;
    std::optional<ObjectStatus> objectStatus; // optional, exists only is
                                              // payload length is 0
    BinaryBufferData objectPayload;
};

/*
    GroupStreamObject Message{
      Object ID (i),
      Object Payload Length (i),
      [Object Status (i)],
      Object Payload (..),
    }
*/
struct GroupStreamObject
{
    iType object;
    iType objectPayloadLength;
    std::optional<ObjectStatus> objectStatus; // optional, exists only is
                                              // payload length is 0
    BinaryBufferData objectPayload;
};

/*
    SUBSCRIBE_OK
    {
      Subscribe ID (i),
      Expires (i),
      Group Order (8),
      ContentExists (f),
      [Largest Group ID (i)],
      [Largest Object ID (i)]
    }
*/
struct SubscribeOkMessage
{
    iType subscribeId;
    iType expires;
    std::uint8_t groupOrder;
    bool contentExists; // sizeof(bool) == 1, as the standard specifies flag
                        // should be of size 1
    std::optional<iType> largestGroupId;
    std::optional<iType> largestObjectId;
};

/*
    SUBSCRIBE_ERROR
    {
      Subscribe ID (i),
      Error Code (i),
      Reason Phrase (b),
      Track Alias (i),
    }
*/
struct SubscribeErrorMessage
{
    iType subscribeId;
    iType errorCode;
    BinaryBufferData reasonPhrase;
    iType trackAlias;
};

/*
    SUBSCRIBE_DONE Message {
      Subscribe ID (i),
      Status Code (i),
      Reason Phrase (b),
      ContentExists (f),
      [Final Group (i)],
      [Final Object (i)],
    }
*/
struct SubscribeDoneMessage
{
    iType subscribeId;
    iType statusCode;
    BinaryBufferData reasonPhrase;
    bool contentExists;
    std::optional<iType> finalGroup;
    std::optional<iType> finalObject;
};

/*
    ANNOUNCE Message {
      Track Namespace (b),
      Number of Parameters (i),
      Parameters (..) ...,
    }
*/
struct AnnounceMessage
{
    using Parameter = std::variant<>;

    BinaryBufferData trackNamespace;
    iType numberOfParameters;
    std::vector<Parameter> params;
};

/*
    UNANNOUNCE Message {
      Track Namespace (b),
    }
*/
struct UnannounceMessage
{
    BinaryBufferData trackNamespace;
};

/*
    TRACK_STATUS Message {
      Track Namespace (b),
      Track Name (b),
      Status Code (i),
      Last Group ID (i),
      Last Object ID (i),
    }
*/
struct TrackStatusMessage
{
    BinaryBufferData trackNamespace;
    BinaryBufferData trackName;
    iType statusCode;
    iType lastGroupId;
    iType lastObjectId;
};
} // namespace rvn::depracated::messages
