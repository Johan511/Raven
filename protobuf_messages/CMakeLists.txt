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

find_program(PROTOC_PATH
    NAMES protoc
    HINTS /usr/bin /usr/local/bin /home/hhn/makes/protobuf/install/bin
    DOC "protoc compiler"
)

if(NOT PROTOC_PATH)
    message(FATAL_ERROR "protoc compiler not found")
else()
    message("protoc compiler found at ${PROTOC}")
endif()

set(PROTOBUF_INSTALL_DIR "/home/hhn/makes/protobuf/install")
set(Protobuf_USE_STATIC_LIBS ON)

find_package(protobuf CONFIG REQUIRED PATHS ${PROTOBUF_INSTALL_DIR})

get_all_proto_files(${CMAKE_CURRENT_SOURCE_DIR} PROTO_FILES)

MESSAGE("PROTO FILES: ${PROTO_FILES}")
# compile .proto files to .pb.cc and .pb.h
execute_process(
    COMMAND ${PROTOC_PATH} --cpp_out=${CMAKE_CURRENT_SOURCE_DIR} --proto_path=${CMAKE_CURRENT_SOURCE_DIR} ${PROTO_FILES}
    RESULT_VARIABLE PROTO_COMPILATION
)

get_all_proto_cc_files(${CMAKE_CURRENT_SOURCE_DIR} PROTO_CC_FILES)
get_all_proto_h_files(${CMAKE_CURRENT_SOURCE_DIR} PROTO_H_FILES)

# move .pb.cc and .pb.h files to src and includes directories
move_files(${PROTO_CC_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/src)
move_files(${PROTO_H_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/includes)

set(PROTO_CC_FILES "")
set(PROTO_H_FILES "")
get_all_proto_cc_files(${CMAKE_CURRENT_SOURCE_DIR}/src PROTO_CC_FILES)
get_all_proto_h_files(${CMAKE_CURRENT_SOURCE_DIR}/includes PROTO_H_FILES)

message("PROTO_CC_FILES: ${PROTO_CC_FILES}")
message("PROTO_H_FILES: ${PROTO_H_FILES}")

get_all_proto_archive_files(${PROTOBUF_INSTALL_DIR}/lib64 PROTOBUF_LIBS)
add_library(protobuf_messages OBJECT ${PROTO_CC_FILES} ${PROTOBUF_LIBS}) 
target_include_directories(protobuf_messages SYSTEM PUBLIC ${Protobuf_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/includes)
message("Protobuf_LIBRARIES: ${Protobuf_LIBRARIES}")
target_link_libraries(protobuf_messages PUBLIC protobuf::libprotobuf ${Protobuf_LIBRARIES})
