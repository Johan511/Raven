# CMAKE functions----------------------------------------------------------------
function(get_all_files_in_directory directory result)
    file(GLOB_RECURSE files "${directory}/*")
    set(${result} ${files} PARENT_SCOPE)
endfunction()
# -------------------------------------------------------------------------------

get_all_files_in_directory(${CMAKE_CURRENT_SOURCE_DIR} RAVEN_TESTS)


# Add playground server
add_executable(test1 src/simple_data_transfer.cpp)
target_include_directories(test1 PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(test1 PUBLIC ${MSQUIC_INCLUDE_DIR} ${MOODY_CAMEL_INCLUDE_DIR} ${PROTOBUF_MESSAGES_INCLUDE_DIR})
target_link_libraries(test1 PUBLIC raven)
