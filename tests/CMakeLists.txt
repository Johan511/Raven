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


add_raven_test(perf/chunk_transfer_perf.cpp)
target_link_libraries(chunk_transfer_perf PRIVATE Boost::program_options)
