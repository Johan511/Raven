# CMake Test utilities file

function(add_raven_test source_file)
    get_filename_component(test_name ${source_file} NAME_WE)
    add_executable(${test_name} ${source_file})
    target_include_directories(${test_name} PUBLIC ${RAVEN_INCLUDE_DIR})
    target_include_directories(${test_name} SYSTEM PUBLIC ${MSQUIC_INCLUDE_DIR} ${MOODY_CAMEL_INCLUDE_DIR} ${PROTOBUF_MESSAGES_INCLUDE_DIR})
    target_link_libraries(${test_name} PUBLIC raven)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()
