
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

# and finally we need to compile the python wrapper into a .so file
# this may require sudo perms depending on your setup
include(GNUInstallDirs)
set(CMAKE_INSTALL_LIBDIR /usr/local/${CMAKE_INSTALL_LIBDIR})

message(STATUS "Using install lib dir: ${CMAKE_INSTALL_LIBDIR}")

add_library(archive_shared
    SHARED
    python_wrapper.cpp
)

target_include_directories(archive_shared
    PRIVATE
)

target_link_libraries(archive_shared
    PRIVATE
    archive
)

install(TARGETS
    archive_shared
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
