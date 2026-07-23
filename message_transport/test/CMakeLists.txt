cmake_minimum_required(VERSION 3.10)
include(CTest)

project(message_transport_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GTest
find_package(GTest REQUIRED CONFIG)

# Add test executable
add_executable(message_transport_tests
    ipc_queue_test.cpp
)

# Link against gtest and main library
target_link_libraries(message_transport_tests
    messaging
    GTest::gtest
    GTest::gtest_main
)

# Enable testing
enable_testing()

add_test(NAME MessageTransportTests COMMAND message_transport_tests)
set_tests_properties(MessageTransportTests PROPERTIES TIMEOUT 60)


# Add test executable
add_executable(multi_reader_message_transport_tests
    ipc_multi_reader_test.cpp
)

# Link against gtest and main library
target_link_libraries(multi_reader_message_transport_tests
    messaging
    GTest::gtest
    GTest::gtest_main
)

# Enable testing
enable_testing()

add_test(NAME MultiReaderMessageTransportTests COMMAND multi_reader_message_transport_tests)
set_tests_properties(MultiReaderMessageTransportTests PROPERTIES TIMEOUT 60)
