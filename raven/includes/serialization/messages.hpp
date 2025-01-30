// deprecated, not using this

#pragma once
////////////////////////////////////////////
#include "utilities.hpp"
#include <msquic.h>
////////////////////////////////////////////
#include <ostream>
#include <serialization/quic_var_int.hpp>
////////////////////////////////////////////
#include <optional>
#include <string>
#include <variant>
#include <vector>
////////////////////////////////////////////

namespace rvn::depracated::messages
{
// (b) type which is encoded as a variable-length integer followed by that many
// bytes of data.
using BinaryBufferData = std::string;
using iType = std::uint64_t; // variable size integer, check QUIC RFC
using MOQTVersion = std::uint32_t;

enum class MoQtMessageType
{
    // not in draft v7 // OBJECT_STREAM = 0x0,
    // not in draft v7 // OBJECT_DATAGRAM = 0x1,
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
    // not in draft v7 // STREAM_HEADER_TRACK = 0x50,
    // not in draft v7 // STREAM_HEADER_GROUP = 0x51
};

struct ControlMessageHeader
{
    MoQtMessageType messageType_;
    std::uint64_t length_;
};

///////////////////////////// Parameters ///////////////////////////////

enum class ParameterType : std::uint64_t
{

};

// Only one parameter of each type should be sent
struct Parameter
{
    ParameterType parameterType_;
    std::string parameterValue_;

    bool operator==(const Parameter&) const = default;

    friend inline std::ostream& operator<<(std::ostream& os, const Parameter& parameter)
    {
        os << "ParameterType: " << utils::to_underlying(parameter.parameterType_)
           << " ParameterValue: " << parameter.parameterValue_;
        return os;
    }
};

///////////////////////////// Messages ///////////////////////////////
struct ControlMessageBase
{
    MoQtMessageType messageType_;

    ControlMessageBase(MoQtMessageType messageType) : messageType_(messageType)
    {
    }

    virtual ~ControlMessageBase() = default;

    bool operator==(const ControlMessageBase&) const = default;
};


/*
    CLIENT_SETUP Message {
      Type (i) = 0x40,
      Length (i),
      Number of Supported Versions (i),
      Supported Version (i) ...,
      Number of Parameters (i) ...,
      Setup Parameters (..) ...,
    }
*/
struct ClientSetupMessage : ControlMessageBase
{
    /*
        MoQ Transport versions are a 32-bit unsigned integer, encoded as a
       varint. This version of the specification is identified by the number
       0x00000001. Versions with the most significant 16 bits of the version
       number cleared are reserved for use in future IETF consensus documents.
    */
    std::vector<MOQTVersion> supportedVersions_;
    std::vector<Parameter> parameters_;

    ClientSetupMessage() : ControlMessageBase(MoQtMessageType::CLIENT_SETUP)
    {
    }

    bool operator==(const ClientSetupMessage&) const = default;

    friend inline std::ostream& operator<<(std::ostream& os, const ClientSetupMessage& msg)
    {
        os << "SupportedVersions: ";
        for (const auto& version : msg.supportedVersions_)
            os << version << " ";
        os << "Parameters: ";
        for (const auto& parameter : msg.parameters_)
            os << parameter;
        return os;
    }
};

/*
    SERVER_SETUP Message {
      Type (i) = 0x41,
      Length (i),
      Selected Version (i),
      Number of Parameters (i) ...,
      Setup Parameters (..) ...,
    }
*/
struct ServerSetupMessage : ControlMessageBase
{
    MOQTVersion selectedVersion_;
    std::vector<Parameter> parameters_;

    ServerSetupMessage() : ControlMessageBase(MoQtMessageType::SERVER_SETUP)
    {
    }

    bool operator==(const ServerSetupMessage&) const = default;

    friend inline std::ostream& operator<<(std::ostream& os, const ServerSetupMessage& msg)
    {
        os << "SelectedVersion: " << msg.selectedVersion_ << " Parameters: ";
        for (const auto& parameter : msg.parameters_)
            os << parameter;
        return os;
    }
};

/*
    SUBSCRIBE Message {
      Type (i) = 0x3,
      Length (i),
      Subscribe ID (i),
      Track Alias (i),
      Track Namespace (tuple),
      Track Name Length (i),
      Track Name (..),
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
struct SubscribeMessage : public ControlMessageBase
{
    enum class FilterType : std::uint64_t
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

    struct GroupObjectPair
    {
        std::uint64_t group_;
        std::uint64_t object_;

        bool operator==(const GroupObjectPair& rhs) const = default;
    };

    std::uint64_t subscribeId_;
    std::uint64_t trackAlias_;
    // encoding moqt tuple as vector<string>
    std::vector<std::string> trackNamespace_;
    std::string trackName_;
    std::uint8_t subscriberPriority_;
    std::uint8_t groupOrder_;
    FilterType filterType_;
    std::optional<GroupObjectPair> start_;
    std::optional<GroupObjectPair> end_;
    std::vector<Parameter> parameters_;

    SubscribeMessage() : ControlMessageBase(MoQtMessageType::SUBSCRIBE)
    {
    }

    bool operator==(const SubscribeMessage& rhs) const
    {
        bool isEqual = true;

        isEqual &= subscribeId_ == rhs.subscribeId_;
        isEqual &= trackAlias_ == rhs.trackAlias_;
        isEqual &= trackNamespace_ == rhs.trackNamespace_;
        isEqual &= trackName_ == rhs.trackName_;
        isEqual &= subscriberPriority_ == rhs.subscriberPriority_;
        isEqual &= groupOrder_ == rhs.groupOrder_;
        isEqual &= filterType_ == rhs.filterType_;
        isEqual &= utils::optional_equality(start_, rhs.start_);
        isEqual &= utils::optional_equality(end_, rhs.end_);
        isEqual &= parameters_ == rhs.parameters_;

        return isEqual;
    }

    friend inline std::ostream& operator<<(std::ostream& os, const SubscribeMessage& msg)
    {
        os << "SubscribeId: " << msg.subscribeId_
           << " TrackAlias: " << msg.trackAlias_ << " TrackNamespace: ";
        for (const auto& ns : msg.trackNamespace_)
            os << ns << " ";
        os << " TrackName: " << msg.trackName_ << " SubscriberPriority: " << msg.subscriberPriority_
           << " GroupOrder: " << msg.groupOrder_
           << " FilterType: " << utils::to_underlying(msg.filterType_) << " Start: "
           << (msg.start_.has_value() ? std::to_string(msg.start_->group_) + " " +
                                        std::to_string(msg.start_->object_) :
                                        "None")
           << " End: "
           << (msg.end_.has_value() ? std::to_string(msg.end_->group_) + " " +
                                      std::to_string(msg.end_->object_) :
                                      "None")
           << " Parameters: ";
        for (const auto& parameter : msg.parameters_)
            os << parameter;
        return os;
    }
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
    std::vector<Parameter> params;
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

    END_OF_GROUP = 0x3, // Indicates end of Group. ObjectId is one greater
                        // that the largest object produced in the group
                        // identified by the GroupID. This is sent right
                        // after the last object in the group. This SHOULD
                        // be cached and have empty payload.

    END_OF_TRACK = 0x4, // Indicates end of Track and Group. GroupID is one
                        // greater than the largest group produced in this
                        // track and the ObjectId is one greater than the
                        // largest object produced in that group. This is
                        // sent right after the last object in the track.
                        // This SHOULD be cached and have empty payload.
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

// Data Stream messages
enum class DataStreamType
{
    OBJECT_DATAGRAM = 0x1,
    STREAM_HEADER_SUBGROUP = 0x4,
    FETCH_HEADER = 0x5
};

/*
    STREAM_HEADER_SUBGROUP Message {
      Track Alias (i),
      Group ID (i),
      Subgroup ID (i),
      Publisher Priority (8),
    }
*/
struct StreamHeaderSubgroupMessage
{
    std::uint64_t trackAlias_;
    std::uint64_t groupId_;
    std::uint64_t subgroupId_;
    std::uint8_t publisherPriority_;
};

// Object type sent on StreamHeaderSubgroup data streams
struct StreamHeaderSubgroupObject
{
    std::uint64_t objectId_;
    std::uint64_t objectPayloadLength_;
    std::string payload_;
};
} // namespace rvn::depracated::messages
