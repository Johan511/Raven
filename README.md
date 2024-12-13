# Build instructions

Please make sure the following external dependencies are installed
1. boost
2. openssl-devel (for RHEL and fedora)
 
```
git clone https://github.com/Johan511/Raven
cd Raven
git submodule update --init --recursive
mkdir build && cd build
cmake ..
make -j$(nproc)
```

# User APIs

## Client
Once the client connection has been started by `start_connection`, a subscribe message can be
built using the `SubscriptionBuilder` class after mentioning the `filter_type`, `track_alias`,
`track_namespace`, `subscriber_priority`, `group_order`

Once the client has subscribed, it is given a reference to a queue to which objects are queued
and it's the consumer's responsibility to dequeue them and use it.


## Server
`moqtServer->register_object()` can be used to specify the object and it's identifiers
