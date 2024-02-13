cmake_minimum_required(VERSION 3.25)

project(Raven)

# CMAKE functions----------------------------------------------------------------
function(get_all_files_in_directory directory result)
    file(GLOB_RECURSE files "${directory}/*")
    set(${result} ${files} PARENT_SCOPE)
endfunction()
# -------------------------------------------------------------------------------


# General Settings--------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
# Set compile options as private to not warn about msquic header files
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address")
endif()
# -------------------------------------------------------------------------------

# Setting MSQUIC related variables
set(MSQUIC_INSTALL_DIR "/home/hhn/makes/msquic/install")
set(MSQUIC_INCLUDE_DIR ${MSQUIC_INSTALL_DIR}/include)
set(MSQUIC_LINK_LIBRARY ${MSQUIC_INSTALL_DIR}/lib/libmsquic.so.2.3.0)

# Setting Raven variables
set(RAVEN_SRC_DIR "raven/src")
set(RAVEN_INCLUDE_DIR "raven/includes")

set(RAVEN_SRC "")
get_all_files_in_directory(${RAVEN_SRC_DIR} RAVEN_SRC)

# Building Raven shared library linked with msquic -----------------------------
add_library(raven SHARED ${RAVEN_SRC})
add_compile_options(raven PRIVATE -Wall -Wextra)
target_include_directories(raven PUBLIC ${RAVEN_INCLUDE_DIR} ${MSQUIC_INCLUDE_DIR})
# MSQUIC include directories should not leak to users of Raven
# They should be able to use their own msquic library includes
target_include_directories(raven PRIVATE ${MSQUIC_INCLUDE_DIR}) 
target_link_libraries(raven PUBLIC ${MSQUIC_LINK_LIBRARY})
# -------------------------------------------------------------------------------

# Add playground server
add_executable(Server server.cpp)
add_compile_options(Server PRIVATE -Wall -Wextra)
target_include_directories(Server PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(Server PUBLIC ${MSQUIC_INCLUDE_DIR})
target_link_libraries(Server PUBLIC Raven)

# Add playground client
add_executable(Client client.cpp)
add_compile_options(Client PRIVATE -Wall -Wextra)
target_include_directories(Client PUBLIC ${RAVEN_INCLUDE_DIR})
target_include_directories(Client PUBLIC ${MSQUIC_INCLUDE_DIR})
target_link_libraries(Client PUBLIC Raven)
