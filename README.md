# Raven
Implementation of better C++ bindings for MsQuic and Media-Over-Quic Draft using MsQuic using C++

# User APIs

## Client
Once the client connection has been started by `start_connection`, a subscribe message can be
built using the `SubscriptionBuilder` class after mentioning the `filter_type`, `track_alias`,
`track_namespace`, `subscriber_priority`, `group_order`

Once the client has subscribed, it is given a reference to a queue to which objects are queued
and it's the consumer's responsibility to dequeue them and use it.


## Server
`moqtServer->register_object()` can be used to specify the object and it's identifiers
