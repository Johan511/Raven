# Session Initialization
Only one bidirectional stream => control stream => Client initiated
Control messages sent only on control stream
Objects (sent on unidirectional stream) not send on control stream
Both lead to session closed as `protocol violation`

Close any other bi directional streams

Abruptly closed control stream => `protocol violation`


# Session cancellation
All other streams can be abruptly closed with no effect

# Termination
Transport session can be terminated with any error message as follows
quic CONNECTION_CLOSE frame must be sent

0x0 No Error
0x1 Generic Error
0x2 Unauthorized
0x3 Protocol Violation
0x10 GOAWAY Timeout 


No Error: The session is being terminated without an error.
Generic Error: An unclassified error occurred.
Unauthorized: The endpoint breached an agreement, which MAY have
been pre-negotiated by the application.
Protocol Violation: The remote endpoint performed an action that
was disallowed by the specification.
GOAWAY Timeout: The session was closed because the client took
too long to close the session in response to a GOAWAY
(Section 6.14) message. See session migration (Section 3.6)

# Migration
MoQ => long lived stateful session
but should be able to restart server independently => done by GOAWAY message

GOAWAY message => next URI => else retry same URI

Server should terminate the session with GOAWAY timeout for any open subscriptions after the timeout

Client needs to unsubscribe => GOAWAY does not change this state
publisher may reject new subs

If server is sub => send GOAWAY before UNSUB

Client waits for subscriptions to close and then send NO_ERROR to close session

