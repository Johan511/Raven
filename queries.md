### Subscribe Errors?
What happens when object subscribed to does not exist?
What we do: Send subscription error

Error while satisfying a subscription
What we do: Send subscription error

<block>
0x1 := Indicates Object does not exist. Indicates that this object does not exist at any publisher and it will not be published in the future. This SHOULD be cached.

0x3 := Indicates end of Group. ObjectId is one greater that the largest object produced in the group identified by the GroupID. This is sent right after the last object in the group. If the ObjectID is 0, it indicates there are no Objects in this Group. This SHOULD be cached. A publisher MAY use an end of Group object to signal the end of all open Subgroups in a Group.

0x4 := Indicates end of Track and Group. GroupID is one greater than the largest group produced in this track and the ObjectId is one greater than the largest object produced in that group. This is sent right after the last object in the track. This SHOULD be cached.

0x5 := Indicates end of Subgroup. Object ID is one greater than the largest normal object ID in the Subgroup.
</block>

What is the use of all this?



### Double free error
In destructor of MOQT Server, connection is torn down, this might call `QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE` callback which would erase the connection
Then destructor deallocates, which causes double free
