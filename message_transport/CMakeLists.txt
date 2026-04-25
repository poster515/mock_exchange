
project(message_transport)

set(PUBLIC_MESSAGING_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/public/messaging)
set(PUBLIC_ARCHIVE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/public/archive)

find_package(spdlog REQUIRED)
find_package(args-parser REQUIRED)

# Queue Library
add_library(messaging STATIC
    mpsc_ipc_queue.cpp
)

target_include_directories(messaging PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public/
)

target_link_libraries(messaging PUBLIC
    spdlog::spdlog
)

#--------------------------------
# Archive Library
add_library(archive STATIC
    MpscArchiveMediaDriver.cpp
)

target_include_directories(archive PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(archive PUBLIC
    utils
)

# Unit Tests
add_subdirectory(test)
add_subdirectory(benchmark)
add_subdirectory(example)


# Actual media driver binary
add_executable(media_driver
    media_driver.cpp
)

target_link_libraries(media_driver
    PUBLIC
    args-parser::args-parser
    messaging
    archive
)


