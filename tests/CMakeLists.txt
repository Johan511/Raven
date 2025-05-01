# CMAKE functions----------------------------------------------------------------
function(get_all_files_in_directory directory result)
    file(GLOB_RECURSE files "${directory}/*")
    set(${result} ${files} PARENT_SCOPE)
endfunction()
# -------------------------------------------------------------------------------

enable_testing()

include(${CMAKE_SOURCE_DIR}/cmake_utilities/test_utilities.cmake)

add_subdirectory(serialization)

add_raven_test(src/simple_data_transfer.cpp)
add_raven_test(src/chunk_transfer.cpp)
add_raven_test(src/deserializer_tests.cpp)

find_package(LTTngUST REQUIRED)
MESSAGE(STATUS "LTTNGUST_INCLUDE_DIRS: ${LTTNGUST_INCLUDE_DIRS}")

add_raven_test(perf/chunk_transfer_perf.cpp lttng_utils/chunk_transfer_perf_lttng.c)
target_link_libraries(chunk_transfer_perf PRIVATE Boost::program_options Boost::log ${LTTNGUST_LIBRARIES})
target_include_directories(chunk_transfer_perf PRIVATE ${LTTNGUST_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/lttng_utils)
target_compile_definitions(chunk_transfer_perf PRIVATE -DBOOST_LOG_DYN_LINK)

add_raven_test(perf/timer_wheel.cpp)

add_raven_test(relays/relay.cpp lttng_utils/chunk_transfer_perf_lttng.c)
target_link_libraries(relay PRIVATE Boost::program_options Boost::log ${LTTNGUST_LIBRARIES})
target_include_directories(relay PRIVATE ${LTTNGUST_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/lttng_utils)
target_compile_definitions(relay PRIVATE -DBOOST_LOG_DYN_LINK)
