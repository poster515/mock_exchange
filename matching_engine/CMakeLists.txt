project(matching_lib)

find_package(spdlog REQUIRED)

# Library
add_library(matching_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public/matching/MatchingEngine.h
    MatchingEngine.cpp
)

target_include_directories(matching_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public
    ${CMAKE_CURRENT_SOURCE_DIR}/public/generated
)

target_link_libraries(matching_lib PUBLIC
    spdlog::spdlog
    sbe_messages
    messaging
)