

find_package(benchmark REQUIRED)

# Consumer Binary
add_executable(consumer_benchmark consumer_benchmark.cpp)
target_link_libraries(consumer_benchmark
    PUBLIC 
    benchmark::benchmark_main
    PRIVATE
    messaging)
