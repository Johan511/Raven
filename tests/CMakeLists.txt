# CMAKE functions----------------------------------------------------------------
function(get_all_files_in_directory directory result)
    file(GLOB_RECURSE files "${directory}/*")
    set(${result} ${files} PARENT_SCOPE)
endfunction()
# -------------------------------------------------------------------------------

enable_testing()

include(${CMAKE_SOURCE_DIR}/cmake_utilities/test_utilities.cmake)

add_subdirectory(serialization)

# add_executable(test1 src/simple_data_transfer.cpp)
# target_include_directories(test1 PUBLIC ${RAVEN_INCLUDE_DIR})
# target_include_directories(test1 SYSTEM PUBLIC ${MSQUIC_INCLUDE_DIR} ${MOODY_CAMEL_INCLUDE_DIR} ${PROTOBUF_MESSAGES_INCLUDE_DIR})
# target_link_libraries(test1 PUBLIC raven)

add_executable(deserializer_tests src/deserializer_tests.cpp)
target_include_directories(deserializer_tests PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(deserializer_tests SYSTEM PUBLIC ${MSQUIC_INCLUDE_DIR} ${MOODY_CAMEL_INCLUDE_DIR} ${PROTOBUF_MESSAGES_INCLUDE_DIR})
target_link_libraries(deserializer_tests PUBLIC raven)
