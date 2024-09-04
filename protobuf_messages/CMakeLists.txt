# CMAKE functions----------------------------------------------------------------
function(get_all_proto_files directory result)
    file(GLOB files "${directory}/*.proto")
    set(${result} ${files} PARENT_SCOPE)
endfunction()

function(get_all_proto_cc_files directory result)
    file(GLOB files "${directory}/*.pb.cc")
    set(${result} ${files} PARENT_SCOPE)
endfunction()

function(get_all_proto_h_files directory result)
    file(GLOB files "${directory}/*.pb.h")
    set(${result} ${files} PARENT_SCOPE)
endfunction()

function(get_all_proto_archive_files directory result)
    file(GLOB files "${directory}/*.a")
    set(${result} ${files} PARENT_SCOPE)
endfunction()

function(move_files FILES DESTINATION_DIR)
    if(EXISTS "${DESTINATION_DIR}")
        file(REMOVE_RECURSE "${DESTINATION_DIR}")
    endif()

    file(MAKE_DIRECTORY ${DESTINATION_DIR})

    # Loop through the files and add a command to move each one
    foreach(FILE ${FILES})
        get_filename_component(FILENAME ${FILE} NAME)
        execute_process(
            COMMAND mv ${FILE} ${DESTINATION_DIR}/${FILENAME}
            RESULT_VARIABLE ${FILENAME}_MOVE
        )
    endforeach()
endfunction()
# -------------------------------------------------------------------------------

set(PROTOBUF_BINARY_DIR "${CMAKE_BINARY_DIR}/submodules/protobuf")
set(PROTOC "${PROTOBUF_BINARY_DIR}/protoc")
set(PROTOBUF_MESSAGES_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

get_all_proto_files(${CMAKE_CURRENT_SOURCE_DIR} PROTO_FILES)
MESSAGE("PROTO FILES: ${PROTO_FILES}")

add_custom_command(
    OUTPUT ${PROTOBUF_BINARY_DIR}/setup_messages.pb.cc ${PROTOBUF_BINARY_DIR}/setup_messages.pb.h 
    COMMAND ${PROTOC} -I ${PROTOBUF_MESSAGES_DIR} --cpp_out=${PROTOBUF_BINARY_DIR} ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
    VERBATIM
)

# compile .proto files to .pb.cc and .pb.h
add_custom_target(ProtoFilesCompilation DEPENDS protobuf::protoc ${PROTOBUF_BINARY_DIR}/setup_messages.pb.cc ${PROTOBUF_BINARY_DIR}/setup_messages.pb.h)

add_library(protobuf_messages OBJECT ${PROTOBUF_BINARY_DIR}/setup_messages.pb.cc) 
add_dependencies(protobuf_messages ProtoFilesCompilation)
target_include_directories(protobuf_messages SYSTEM PUBLIC ${Protobuf_INCLUDE_DIRS} ${PROTOBUF_BINARY_DIR})
target_link_libraries(protobuf_messages PUBLIC protobuf::libprotobuf)
