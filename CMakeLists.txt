cmake_minimum_required(VERSION 3.25)

project(Raven)

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

# Main executable
set(SOURCE ${RAVEN_SRC_DIR}/wrappers.cpp ${RAVEN_SRC_DIR}/main.cpp)
add_executable(main raven/src/wrappers.cpp)
add_compile_options(main PRIVATE -Wall -Wextra)
target_include_directories(main PUBLIC ${MSQUIC_INCLUDE_DIR})
target_link_libraries(main PUBLIC ${MSQUIC_LINK_LIBRARY})

# Add playground server
add_executable(Server server.cpp)
add_compile_options(Server PRIVATE -Wall -Wextra)
target_include_directories(Server PUBLIC ${MSQUIC_INCLUDE_DIR})
target_link_libraries(Server PUBLIC ${MSQUIC_LINK_LIBRARY})

# Add playground client
add_executable(Client client.cpp)
add_compile_options(Client PRIVATE -Wall -Wextra)
target_include_directories(Client PUBLIC ${MSQUIC_INCLUDE_DIR})
target_link_libraries(Client PUBLIC ${MSQUIC_LINK_LIBRARY})
