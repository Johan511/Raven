cmake_minimum_required(VERSION 3.25)

add_compile_options(-fsanitize=address -g)
add_link_options(-fsanitize=address -g)

project(Raven)
set(MSQUIC_INSTALL_DIR "/home/hhn/makes/msquic/install")

add_executable(Main raven/src/wrappers.cpp)

# Include msquic includes
target_include_directories(Main PUBLIC ${MSQUIC_INSTALL_DIR}/include)

add_executable(Server server.cpp)

# Include msquic includes
target_include_directories(Server PUBLIC ${MSQUIC_INSTALL_DIR}/include)

# Link msquic libraries
target_link_libraries(Server
                      PUBLIC ${MSQUIC_INSTALL_DIR}/lib/libmsquic.so.2.3.0)

add_executable(Client client.cpp)

# Include msquic includes
target_include_directories(Client PUBLIC ${MSQUIC_INSTALL_DIR}/include)

# Link msquic libraries
target_link_libraries(Client
                      PUBLIC ${MSQUIC_INSTALL_DIR}/lib/libmsquic.so.2.3.0)
