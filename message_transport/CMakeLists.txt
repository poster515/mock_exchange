cmake_minimum_required(VERSION 3.10)
project(messaging)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/public/messaging)

find_package(spdlog REQUIRED)

# Library
add_library(messaging SHARED
    ${PUBLIC_INCLUDE_DIR}/mpsc_ipc_queue.h
    mpsc_ipc_queue.cpp
)

target_include_directories(messaging PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(messaging PUBLIC
    spdlog::spdlog
)

# Unit Tests
add_subdirectory(test)

# Producer Binary
add_executable(producer test_producer.cpp)
target_link_libraries(producer PRIVATE messaging)

# Consumer Binary
add_executable(consumer test_consumer.cpp)
target_link_libraries(consumer PRIVATE messaging)
