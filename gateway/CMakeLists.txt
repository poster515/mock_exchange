
project(gateway)

find_package(args-parser REQUIRED)

# Library
add_library(gateway STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public/parser/FixSbeDecoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/public/parser/FixSbeEncoder.h
    FixSbeEncoder.cpp
    FixSbeDecoder.cpp
    FixSbeGateway.cpp
)

target_include_directories(gateway PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(gateway PUBLIC
    sbe
    utils
    messaging
    polling
)

# Unit Tests
add_subdirectory(test)

# Actual gateway binary
add_executable(gateway_main
    sbe_gateway.cpp
)

target_link_libraries(gateway_main
    PUBLIC
    args-parser::args-parser
    gateway
)
