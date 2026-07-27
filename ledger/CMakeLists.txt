project(ledger)

find_package(spdlog REQUIRED)
find_package(args-parser REQUIRED)
find_package(absl REQUIRED)

# Library
add_library(ledger STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public/ledger/Ledger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/public/ledger/ActiveOrder.h
    Ledger.cpp
)

target_include_directories(ledger PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(ledger PUBLIC
    absl::flat_hash_map
    absl::node_hash_map
    utils
    sbe
    polling
    archive # always pull in args-parser
)