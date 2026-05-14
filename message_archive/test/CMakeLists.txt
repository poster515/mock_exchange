cmake_minimum_required(VERSION 3.10)
include(CTest)

project(message_archive_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GTest
find_package(GTest REQUIRED CONFIG)

# Add test executable
add_executable(message_archive_tests
    archive_publication_tests.cpp
)

# Link against gtest and main library
target_link_libraries(message_archive_tests
    messaging
    archive
    GTest::gtest
    GTest::gtest_main
)

# Enable testing
enable_testing()

add_test(NAME MessageArchiveTests COMMAND message_archive_tests)
set_tests_properties(MessageArchiveTests PROPERTIES TIMEOUT 60)
