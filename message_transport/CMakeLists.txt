
project(message_transport)

set(PUBLIC_MESSAGING_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/public/messaging)

find_package(spdlog REQUIRED)
find_package(args-parser REQUIRED)

# Queue Library
add_library(messaging STATIC
    ipc_queue.cpp
)

target_include_directories(messaging PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public/
)

target_link_libraries(messaging PUBLIC
    spdlog::spdlog
)

# Unit Tests
add_subdirectory(test)
add_subdirectory(benchmark)
add_subdirectory(example)


