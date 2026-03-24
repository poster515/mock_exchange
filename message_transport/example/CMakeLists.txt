project(mpsc_example)

# Producer Binary
add_executable(producer test_producer.cpp)
target_link_libraries(producer PRIVATE messaging)

# Consumer Binary
add_executable(consumer test_consumer.cpp)
target_link_libraries(consumer PRIVATE messaging)