
project(gateway)

find_package(spdlog REQUIRED)

# Library
add_library(gateway_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public/parser/FixSbeDecoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/public/parser/FixSbeEncoder.h
    FixSbeEncoder.cpp
    FixSbeDecoder.cpp
)

target_include_directories(gateway_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public
    ${CMAKE_CURRENT_SOURCE_DIR}/public/generated
)

target_link_libraries(gateway_lib PUBLIC
    spdlog::spdlog
    sbe
    messaging
)

# Unit Tests
add_subdirectory(test)

# Actual gateway binary
add_executable(gateway_main
    sbe_gateway.cpp
)

target_link_libraries(gateway_main
    PUBLIC
    gateway_lib
)
