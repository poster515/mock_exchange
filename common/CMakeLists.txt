project (utils)

find_package(libconfig REQUIRED)
find_package(spdlog REQUIRED)


add_library(utils INTERFACE)

target_include_directories(utils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(utils INTERFACE
    spdlog::spdlog
    libconfig::libconfig++
)