# TSAN Warnings
Updatea all knows TSAN warnings over here

```
WARNING: ThreadSanitizer: data race (pid=28877)
  Write of size 8 at 0x720400008f50 by thread T15:
    #0 eventfd <null> (libtsan.so.2+0x1c3ee) (BuildId: a4fcf6899c58ea19315282d1878579e13b50c9f5)
    #1 CxPlatSocketContextSqeInitialize <null> (libmsquic.so.2+0x45725) (BuildId: 5ada94b94ef330715b3b5450b5008ef67ca0b549)

  Previous write of size 8 at 0x720400008f50 by main thread:
    #0 pipe <null> (libtsan.so.2+0x1da3c) (BuildId: a4fcf6899c58ea19315282d1878579e13b50c9f5)
    #1 <null> <null> (libubsan.so.1+0x19bce) (BuildId: 0f19588fcca5f3cd9503b1e832c61c11f0263f51)
    #2 main /home/hhn/raven/raven/client.cpp:56 (client+0x405f7f) (BuildId: 00187c723e1bc92382cbbbc29e67b5c722943ab8) (unique_connection call)

  Thread T15 (tid=28894, running) created by main thread at:
    #0 pthread_create <null> (libtsan.so.2+0x205e6) (BuildId: a4fcf6899c58ea19315282d1878579e13b50c9f5)
    #1 CxPlatThreadCreate <null> (libmsquic.so.2+0x43777) (BuildId: 5ada94b94ef330715b3b5450b5008ef67ca0b549)
    #2 rvn::MOQTClient::start_connection(unsigned short, char const*, unsigned short) /home/hhn/raven/raven/raven/src/moqt_client.cpp:18 (client+0x49606a) (BuildId: 00187c723e1bc92382cbbbc29e67b5c722943ab8) (unique_registeration call)
    #3 main /home/hhn/raven/raven/client.cpp:56 (client+0x405f7f) (BuildId: 00187c723e1bc92382cbbbc29e67b5c722943ab8)

SUMMARY: ThreadSanitizer: data race (/lib64/libtsan.so.2+0x1c3ee) (BuildId: a4fcf6899c58ea19315282d1878579e13b50c9f5) in eventfd
==================
Server Setup Message received: 
 SelectedVersion: 0 Parameters: 

==================
WARNING: ThreadSanitizer: data race (pid=28877)
  Write of size 8 at 0x7204000092e0 by main thread:
    #0 pipe <null> (libtsan.so.2+0x1da3c) (BuildId: a4fcf6899c58ea19315282d1878579e13b50c9f5)
    #1 <null> <null> (libubsan.so.1+0x19bce) (BuildId: 0f19588fcca5f3cd9503b1e832c61c11f0263f51)
    #2 rvn::MOQTClient::start_connection(unsigned short, char const*, unsigned short) /home/hhn/raven/raven/raven/src/moqt_client.cpp:38 (client+0x498126) (BuildId: 00187c723e1bc92382cbbbc29e67b5c722943ab8)
    #3 main /home/hhn/raven/raven/client.cpp:56 (client+0x405f7f) (BuildId: 00187c723e1bc92382cbbbc29e67b5c722943ab8)

  Previous write of size 8 at 0x7204000092e0 by thread T15:
    #0 pipe <null> (libtsan.so.2+0x1da3c) (BuildId: a4fcf6899c58ea19315282d1878579e13b50c9f5)
    #1 <null> <null> (libubsan.so.1+0x19bce) (BuildId: 0f19588fcca5f3cd9503b1e832c61c11f0263f51)
    #2 std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_release_last_use() /usr/include/c++/14/bits/shared_ptr_base.h:175 (client+0x4221d2) (BuildId: 00187c723e1bc92382cbbbc29e67b5c722943ab8)
    #3 std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_release() /usr/include/c++/14/bits/shared_ptr_base.h:361 (client+0x4221d2)
    #4 std::__shared_count<(__gnu_cxx::_Lock_policy)2>::~__shared_count() /usr/include/c++/14/bits/shared_ptr_base.h:1069 (client+0x4221d2)
    #5 std::__shared_ptr<QUIC_BUFFER const, (__gnu_cxx::_Lock_policy)2>::~__shared_ptr() /usr/include/c++/14/bits/shared_ptr_base.h:1525 (client+0x40ca62) (BuildId: 00187c723e1bc92382cbbbc29e67b5c722943ab8)
    #6 std::shared_ptr<QUIC_BUFFER const>::~shared_ptr() /usr/include/c++/14/bits/shared_ptr.h:175 (client+0x40ca62)
    #7 operator() /home/hhn/raven/raven/raven/includes/callbacks.hpp:246 (client+0x40ca62)
    #8 __invoke_impl<unsigned int, rvn::callbacks::<lambda(HQUIC, void*, QUIC_STREAM_EVENT*)>&, QUIC_HANDLE*, void*, QUIC_STREAM_EVENT*> /usr/include/c++/14/bits/invoke.h:61 (client+0x40ca62)
    #9 __invoke_r<unsigned int, rvn::callbacks::<lambda(HQUIC, void*, QUIC_STREAM_EVENT*)>&, QUIC_HANDLE*, void*, QUIC_STREAM_EVENT*> /usr/include/c++/14/bits/invoke.h:114 (client+0x40ca62)
    #10 _M_invoke /usr/include/c++/14/bits/std_function.h:290 (client+0x40ca62)
    #11 std::function<unsigned int (QUIC_HANDLE*, void*, QUIC_STREAM_EVENT*)>::operator()(QUIC_HANDLE*, void*, QUIC_STREAM_EVENT*) const /usr/include/c++/14/bits/std_function.h:591 (client+0x461e4d) (BuildId: 00187c723e1bc92382cbbbc29e67b5c722943ab8)
    #12 rvn::MOQT::control_stream_cb_wrapper(QUIC_HANDLE*, void*, QUIC_STREAM_EVENT*) /home/hhn/raven/raven/raven/includes/moqt_base.hpp:129 (client+0x461e4d)
    #13 QuicStreamRecvFlush <null> (libmsquic.so.2+0xd95f) (BuildId: 5ada94b94ef330715b3b5450b5008ef67ca0b549)

  Thread T15 (tid=28894, running) created by main thread at:
    #0 pthread_create <null> (libtsan.so.2+0x205e6) (BuildId: a4fcf6899c58ea19315282d1878579e13b50c9f5)
    #1 CxPlatThreadCreate <null> (libmsquic.so.2+0x43777) (BuildId: 5ada94b94ef330715b3b5450b5008ef67ca0b549)
    #2 rvn::MOQTClient::start_connection(unsigned short, char const*, unsigned short) /home/hhn/raven/raven/raven/src/moqt_client.cpp:18 (client+0x49606a) (BuildId: 00187c723e1bc92382cbbbc29e67b5c722943ab8)
    #3 main /home/hhn/raven/raven/client.cpp:56 (client+0x405f7f) (BuildId: 00187c723e1bc92382cbbbc29e67b5c722943ab8)

SUMMARY: ThreadSanitizer: data race (/lib64/libtsan.so.2+0x1da3c) (BuildId: a4fcf6899c58ea19315282d1878579e13b50c9f5) in pipe
==================
```

