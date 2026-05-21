project(sbe)


# Library
add_library(sbe INTERFACE)

target_include_directories(sbe INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)
