# CMAKE functions----------------------------------------------------------------
function(get_all_files_in_directory directory result)
    file(GLOB_RECURSE files "${directory}/*")
    set(${result} ${files} PARENT_SCOPE)
endfunction()
# -------------------------------------------------------------------------------

get_all_files_in_directory(${CMAKE_CURRENT_SOURCE_DIR} RAVEN_EXAMPLES)


# Add playground server
add_executable(server_ex1 src/server.cpp)
add_executable(client_ex1 src/client.cpp)

target_include_directories(server_ex1 PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(server_ex1 PUBLIC ${MSQUIC_INCLUDE_DIR} ${MOODY_CAMEL_INCLUDE_DIR})
target_link_libraries(server_ex1 PUBLIC raven)

target_include_directories(client_ex1 PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(client_ex1 PUBLIC ${MSQUIC_INCLUDE_DIR} ${MOODY_CAMEL_INCLUDE_DIR})
target_link_libraries(client_ex1 PUBLIC raven)
