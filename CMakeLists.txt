
cmake_minimum_required(VERSION 3.31)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

project(sandbox LANGUAGES CXX)

enable_testing()

add_subdirectory(polling)
add_subdirectory(message_transport)
