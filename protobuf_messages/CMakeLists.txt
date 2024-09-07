# CMAKE functions----------------------------------------------------------------
function(get_all_proto_files directory result)
    file(GLOB files "${directory}/*.proto")
    set(${result} ${files} PARENT_SCOPE)
endfunction()

function(get_proto_cc_files proto_files proto_cc_files)
    set(${proto_cc_files} "" PARENT_SCOPE)
    foreach(item IN LISTS ${proto_files})
        get_filename_component(file_name "${item}" NAME_WE)
        message("file_name: ${file_name}")
        set(cc_file "${CMAKE_CURRENT_BINARY_DIR}/${file_name}.pb.cc")
        list(APPEND result_list "${cc_file}")
    endforeach()
    set(${proto_cc_files} "${result_list}" PARENT_SCOPE)
endfunction()

function(get_proto_h_files proto_files proto_h_files)
    set(${proto_h_files} "" PARENT_SCOPE)
    foreach(item IN LISTS ${proto_files})
        get_filename_component(file_name "${item}" NAME_WE)
        set(h_file "${CMAKE_CURRENT_BINARY_DIR}/${file_name}.pb.h")
        list(APPEND result_list "${h_file}")
    endforeach()
    set(${proto_h_files} "${result_list}" PARENT_SCOPE)
endfunction()

function(get_all_proto_h_files directory result)
    file(GLOB files "${directory}/*.pb.h")
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

set(PROTOBUF_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(PROTOC "${CMAKE_BINARY_DIR}/submodules/protobuf/protoc")
set(PROTOBUF_MESSAGES_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

get_all_proto_files(${CMAKE_CURRENT_SOURCE_DIR} PROTO_FILES)

get_proto_cc_files(PROTO_FILES PROTO_CC_FILES)
get_proto_h_files(PROTO_FILES PROTO_H_FILES)

MESSAGE("PROTO FILES: ${PROTO_FILES}")
MESSAGE("PROTO CC FILES: ${PROTO_CC_FILES}")
MESSAGE("PROTO H FILES: ${PROTO_H_FILES}")

add_custom_command(
    OUTPUT ${PROTO_CC_FILES} ${PROTO_H_FILES}
    COMMAND ${PROTOC} -I ${PROTOBUF_MESSAGES_DIR} --cpp_out=${PROTOBUF_BINARY_DIR} ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
    VERBATIM
)

# TODO: libraven.a(setup_messages.pb.cc.o):/home/hhn/cs/raven/build/submodules/protobuf/setup_messages.pb.cc:335:(.rodata+0x1080): undefined reference to `descriptor_table_message_5ftype_2eproto'

# compile .proto files to .pb.cc and .pb.h
add_custom_target(ProtoFilesCompilation DEPENDS protobuf::protoc ${PROTO_CC_FILES} ${PROTO_H_FILES})

add_library(protobuf_messages OBJECT ${PROTO_CC_FILES}) 
add_dependencies(protobuf_messages ProtoFilesCompilation)
target_include_directories(protobuf_messages SYSTEM PUBLIC ${PROTOBUF_BINARY_DIR})
target_link_libraries(protobuf_messages PUBLIC protobuf::libprotobuf)
