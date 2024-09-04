cmake_minimum_required(VERSION 3.25)

project(Raven)

# CMAKE functions----------------------------------------------------------------
function(get_all_files_in_directory directory result)
    file(GLOB_RECURSE files "${directory}/*")
    set(${result} ${files} PARENT_SCOPE)
endfunction()
# -------------------------------------------------------------------------------


# General Settings--------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
# Set compile options as private to not warn about msquic header files
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address")
endif()

# -------------------------------------------------------------------------------

# Setting MSQUIC related variables
set(MSQUIC_INSTALL_DIR "/home/hhn/makes/msquic/install")
set(MSQUIC_INCLUDE_DIR ${MSQUIC_INSTALL_DIR}/include)
set(MSQUIC_LINK_LIBRARY ${MSQUIC_INSTALL_DIR}/lib/libmsquic.so.2.4.0)

# Setting Boost variables
find_package(Boost COMPONENTS serialization REQUIRED)
message("boost lib: ${Boost_LIBRARIES}")
message("boost inc:${Boost_INCLUDE_DIR}")

# Setting Raven variables
set(RAVEN_SRC_DIR "raven/src")
set(RAVEN_INCLUDE_DIR "raven/includes")

# disabling protobuf warnings
set(ORIGINAL_COMPILE_OPTIONS ${CMAKE_CXX_FLAGS})
set(PROTOBUF_COMPILE_OPTIONS "${ORIGINAL_COMPILE_OPTIONS} -Wno-missing-requires -Wno-conversion -Wno-attributes -Wno-sign-conversion -Wno-this-pointer -Wno-bool-conversion -Wno-deprecated-declarations")
set(CMAKE_CXX_FLAGS ${PROTOBUF_COMPILE_OPTIONS})
add_subdirectory(submodules/protobuf)
set(CMAKE_CXX_FLAGS ${ORIGINAL_COMPILE_OPTIONS})

add_subdirectory(protobuf_messages)
set(PROTOBUF_MESSAGES_DIR "protobuf_messages")
set(PROTOBUF_MESSAGES_INCLUDE_DIR "${PROTOBUF_MESSAGES_DIR}/src")
set(PROTOBUF_MESSAGES_SRC_DIR "${PROTOBUF_MESSAGES_DIR}/src")

set(RAVEN_SRC "")
get_all_files_in_directory(${RAVEN_SRC_DIR} RAVEN_SRC)

# Building Raven shared library linked with msquic -----------------------------
add_library(raven STATIC ${RAVEN_SRC})
# target_compile_options(raven PRIVATE -Wall -Wextra -Werror -Wpedantic)
# MSQUIC include directories should not leak to users of Raven
# They should be able to use their own msquic library includes
target_include_directories(raven PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(raven PUBLIC ${MSQUIC_INCLUDE_DIR} ${Boost_INCLUDE_DIRS} ${PROTOBUF_MESSAGES_INCLUDE_DIR}) 
target_link_libraries(raven PUBLIC ${MSQUIC_LINK_LIBRARY} ${Boost_LIBRARIES} protobuf_messages)
# -------------------------------------------------------------------------------

# Add playground server
add_executable(server server.cpp)
# target_compile_options(server PRIVATE -Wall -Wextra -Werror -Wpedantic)
target_include_directories(server PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(server PUBLIC ${MSQUIC_INCLUDE_DIR})
target_link_libraries(server PUBLIC raven)

# Add playground client
add_executable(client client.cpp)
# target_compile_options(client PRIVATE -Wall -Wextra -Werror -Wpedantic)
target_include_directories(client PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(client PUBLIC ${MSQUIC_INCLUDE_DIR})
target_link_libraries(client PUBLIC raven)
