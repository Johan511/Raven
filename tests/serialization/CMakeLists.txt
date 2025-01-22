add_executable(stest1 serialize_trivial_tests.cpp)
target_include_directories(stest1 PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(stest1 PUBLIC ${MSQUIC_INCLUDE_DIR} ${MOODY_CAMEL_INCLUDE_DIR} ${PROTOBUF_MESSAGES_INCLUDE_DIR})
target_link_libraries(stest1 PUBLIC raven)

add_executable(stest2 serialize_quic_var_int_tests.cpp)
target_include_directories(stest2 PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(stest2 PUBLIC ${MSQUIC_INCLUDE_DIR} ${MOODY_CAMEL_INCLUDE_DIR} ${PROTOBUF_MESSAGES_INCLUDE_DIR})
target_link_libraries(stest2 PUBLIC raven)


add_executable(stest3 serialize_client_setup_message.cpp)
target_include_directories(stest3 PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(stest3 PUBLIC ${MSQUIC_INCLUDE_DIR} ${MOODY_CAMEL_INCLUDE_DIR} ${PROTOBUF_MESSAGES_INCLUDE_DIR})
target_link_libraries(stest3 PUBLIC raven)
