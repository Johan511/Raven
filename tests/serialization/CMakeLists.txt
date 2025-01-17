add_executable(stest1 serialize_trivial_tests.cpp)
target_include_directories(stest1 PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(stest1 PUBLIC ${MSQUIC_INCLUDE_DIR} ${MOODY_CAMEL_INCLUDE_DIR} ${PROTOBUF_MESSAGES_INCLUDE_DIR})
target_link_libraries(stest1 PUBLIC raven)
