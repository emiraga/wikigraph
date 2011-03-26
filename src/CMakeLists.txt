cmake_minimum_required ( VERSION 2.6 )
add_definitions ( -Wall -fstrict-aliasing -Wstrict-aliasing=2 )
find_package (Threads)


include_directories (
    ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
)

#add_library ( graphlib
#    graphlib.cc
#)
#target_link_libraries (test_graph graphlib)

add_executable ( run_tests
        test_writerlib.cc
        gmock/gmock-gtest-all.cc
        run_tests.cc
)

target_link_libraries (run_tests ${CMAKE_THREAD_LIBS_INIT})
