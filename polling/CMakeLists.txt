project(polling)

find_package(spdlog REQUIRED)

add_library(polling INTERFACE)

target_include_directories(polling INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(polling INTERFACE
    spdlog::spdlog
)