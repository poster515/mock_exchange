
project(message_archive)

set(PUBLIC_ARCHIVE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/public/archive)

find_package(spdlog REQUIRED)
find_package(args-parser REQUIRED)

#--------------------------------
# Archive Library
add_library(archive STATIC
    MpscArchiveMediaDriver.cpp
    ArchivePublication.cpp
    ArchiveSubscription.cpp
)

target_include_directories(archive PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(archive PUBLIC
    utils
    messaging
)

# Unit Tests
add_subdirectory(test)

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


