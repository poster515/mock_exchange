
cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

project(sandbox LANGUAGES CXX)

enable_testing()

add_subdirectory(polling)
add_subdirectory(sbe_messages)
add_subdirectory(gateway)
add_subdirectory(message_transport)
