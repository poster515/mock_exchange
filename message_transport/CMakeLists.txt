
project(messaging)

set(PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/public/messaging)

find_package(spdlog REQUIRED)

# Library
add_library(messaging STATIC
    ${PUBLIC_INCLUDE_DIR}/mpsc_ipc_queue.h
    ${PUBLIC_INCLUDE_DIR}/SpinPolicy.h
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
add_subdirectory(benchmark)
add_subdirectory(example)

