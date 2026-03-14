project(sbe)


# Library
add_library(sbe INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/public/sbe/generated
)
