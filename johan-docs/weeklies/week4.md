# 3/2 - 10/2
Progress
1. `MOQTClient` and `MOQTServer` now derive from MOQT object. They have only 
    one class memeber as of now which is `unique_listener` or `unique_connection`.
2. `listener_cb_lambda` of MOQT object now has different meaning, it unifies to
    listener_cb in case of server and connection_cb in case of server
3. General refactoring to support MOQT client



