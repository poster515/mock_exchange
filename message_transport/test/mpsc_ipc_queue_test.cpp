#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <ranges>
#include <unordered_set>
#include <sys/mman.h>

#include "messaging/mpsc_ipc_queue.h"
#include "messaging/mpsc_ipc_queue_element_wrapper.h"

using namespace message_transport;

class MpscIpcQueueTest : public ::testing::Test {
protected:
    static constexpr const char* SHM_NAME = "/mpsc_ipc_queue_test";
    static constexpr size_t QUEUE_SIZE = 4096;

    void SetUp() override {
        shm_unlink(SHM_NAME);
    }

    void TearDown() override {
        shm_unlink(SHM_NAME);
    }
};

TEST_F(MpscIpcQueueTest, BasicWriteAndRead) {
    MpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](MpscIpcQueueRaiiWrapper){});

    std::string_view test_data = "Hello, World!";

    auto wrapper = writer.blocking_claim_buffer(test_data.size());
    ASSERT_TRUE(wrapper.write_to_buffer(test_data.data(), test_data.size()));
    wrapper.~MpscIpcQueueRaiiWriterWrapper(); // explicitly call the destructor to commit the message to the queue

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto read_wrapper = reader.poll_buffer();
    ASSERT_TRUE(read_wrapper.has_value());

    auto read_data = read_wrapper->get_as_view<std::string_view>();
    EXPECT_EQ(read_data, test_data);
}

TEST_F(MpscIpcQueueTest, ProducerBlocksWhenQueueFull) {
    MpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](MpscIpcQueueRaiiWrapper){});

    const size_t msg_size = 64;
    const size_t available_space = QUEUE_SIZE - sizeof(message_transport::GlobalHeader);
    const int num_messages_to_fill = available_space / (msg_size + sizeof(message_transport::MessageHeader));

    std::vector<int> written_values;
    std::atomic<bool> producer_blocked(false);

    // Fill the queue
    for (int i : std::ranges::iota_view{0, num_messages_to_fill}) {
        auto wrapper = writer.blocking_claim_buffer(msg_size);
        int value { i };
        wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(int));
        written_values.push_back(value);
    }

    // Start producer thread that will block trying to write
    std::thread producer([&writer, &producer_blocked, msg_size]() {
        producer_blocked.store(true, std::memory_order_release);
        int value = 999;
        auto wrapper = writer.blocking_claim_buffer(msg_size);
        wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(int));
        producer_blocked.store(false, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(producer_blocked.load(std::memory_order_acquire));

    // Read messages one by one to free up space
    for (int i : std::ranges::iota_view{0, num_messages_to_fill}) {
        auto read_wrapper = reader.poll_buffer();
        ASSERT_TRUE(read_wrapper.has_value());
        int value;
        std::memcpy(&value, read_wrapper->get_buffer(), sizeof(int));
        EXPECT_EQ(value, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Producer should now be able to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(producer_blocked.load(std::memory_order_acquire));

    producer.join();

    // Verify the last message from producer
    auto final_read = reader.poll_buffer();
    ASSERT_TRUE(final_read.has_value());
    int final_value;
    std::memcpy(&final_value, final_read->get_buffer(), sizeof(int));
    EXPECT_EQ(final_value, 999);
}

TEST_F(MpscIpcQueueTest, BasicQueueWrapping) {
    const size_t SMALL_QUEUE_SIZE = 128;
    MpscIpcQueue writer(SHM_NAME, SMALL_QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, SMALL_QUEUE_SIZE, [](MpscIpcQueueRaiiWrapper){});

    std::string_view message = "this_is_a_long_message";
    const auto iters_to_fill_buffer = (SMALL_QUEUE_SIZE  - sizeof(message_transport::GlobalHeader) - sizeof(message_transport::MessageHeader)) / (message.size() + sizeof(message_transport::MessageHeader));

    for (auto i = 0; i < iters_to_fill_buffer; ++i) {
        auto wrapper = writer.blocking_claim_buffer(message.size());
        ASSERT_TRUE(wrapper.write_to_buffer(message.data(), message.size()));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // now the queue is full and we want to make sure the wrapping works correctly
    // need to consume one message though to free up enough space for a new message at the front
    auto read_wrapper = reader.poll_buffer();
    ASSERT_TRUE(read_wrapper.has_value());

    auto read_data = read_wrapper->get_as_view<std::string_view>();
    EXPECT_EQ(read_data, message);
    read_wrapper->release(); // have to manually release (or destroy) here.

    // now we can write one more message which should wrap around to the beginning of the queue
    {
        auto wrapper = writer.blocking_claim_buffer(message.size());
        ASSERT_TRUE(wrapper.write_to_buffer(message.data(), message.size()));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // anddddd then read everything we can
    while (auto read_wrapper = reader.poll_buffer()) {
        ASSERT_TRUE(read_wrapper.has_value());
        auto read_data = read_wrapper->get_as_view<std::string_view>();
        EXPECT_EQ(read_data, message);
    }
}

TEST_F(MpscIpcQueueTest, MultipleMessagesSequential) {
    MpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](MpscIpcQueueRaiiWrapper){});

    const std::vector<std::string_view> messages = {"msg1", "msg2", "msg3"};

    for (const auto& msg : messages) {
        auto wrapper = writer.blocking_claim_buffer(msg.size());
        ASSERT_TRUE(wrapper.write_to_buffer(msg.data(), msg.size()));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    for (const auto& expected_msg : messages) {
        auto read_wrapper = reader.poll_buffer();
        ASSERT_TRUE(read_wrapper.has_value());

        auto read_data = read_wrapper->get_as_view<std::string_view>();
        EXPECT_EQ(read_data, expected_msg);
    }
}

TEST_F(MpscIpcQueueTest, SlowProducerFastConsumer) {
    MpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](MpscIpcQueueRaiiWrapper){});

    std::vector<int> written_values;
    std::vector<int> read_values;

    const auto NUM_MESSAGES = 10;

    auto producer = [&writer, &written_values]() {
        for (int i : std::ranges::iota_view{0, NUM_MESSAGES}) {
            auto wrapper = writer.blocking_claim_buffer(sizeof(int));
            ASSERT_TRUE(wrapper.write_to_buffer(reinterpret_cast<const char*>(&i), sizeof(int)));
            written_values.push_back(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    };

    auto consumer = [&reader, &read_values]() {
        while (read_values.size() < NUM_MESSAGES) {
            auto wrapper = reader.poll_buffer();
            if (wrapper.has_value()) {
                int value;
                std::memcpy(&value, wrapper->get_buffer(), sizeof(int));
                read_values.push_back(value);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };

    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);

    producer_thread.join();
    consumer_thread.join();

    EXPECT_EQ(written_values.size(), read_values.size());
    for (size_t i = 0; i < written_values.size(); ++i) {
        EXPECT_EQ(written_values[i], read_values[i]);
    }
}

TEST_F(MpscIpcQueueTest, FastProducerSlowConsumer) {
    MpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](MpscIpcQueueRaiiWrapper){});

    std::vector<int> written_values;
    std::vector<int> read_values;
    std::mutex values_mutex;

    const auto NUM_MESSAGES = 10;

    auto producer = [&writer, &written_values]() {
        for (int i : std::ranges::iota_view{0, NUM_MESSAGES}) {
            auto wrapper = writer.blocking_claim_buffer(sizeof(int));
            ASSERT_TRUE(wrapper.write_to_buffer(reinterpret_cast<const char*>(&i), sizeof(int)));
            written_values.push_back(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    };

    auto consumer = [&reader, &read_values]() {
        while (read_values.size() < NUM_MESSAGES) {
            auto wrapper = reader.poll_buffer();
            if (wrapper.has_value()) {
                int value;
                std::memcpy(&value, wrapper->get_buffer(), sizeof(int));
                read_values.push_back(value);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    };

    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);

    producer_thread.join();
    consumer_thread.join();

    EXPECT_EQ(written_values.size(), read_values.size());
    for (size_t i = 0; i < written_values.size(); ++i) {
        EXPECT_EQ(written_values[i], read_values[i]);
    }
}

TEST_F(MpscIpcQueueTest, QueueWrapAroundFastProducerSlowConsumer) {

    const auto SMALL_QUEUE_SIZE_BYTES = 128;
    MpscIpcQueue writer(SHM_NAME, SMALL_QUEUE_SIZE_BYTES, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, SMALL_QUEUE_SIZE_BYTES, [](MpscIpcQueueRaiiWrapper){});

    std::vector<uint64_t> written_values;
    std::vector<uint64_t> read_values;

    const auto iters_to_fill_buffer = (SMALL_QUEUE_SIZE_BYTES  - sizeof(message_transport::GlobalHeader)) / (sizeof(uint64_t) + sizeof(message_transport::MessageHeader));
    const int NUM_MESSAGES = iters_to_fill_buffer * 1.5; // write enough messages to fill the buffer and cause wrap around
    std::cout << "Buffer can hold " << iters_to_fill_buffer << " messages, writing " << NUM_MESSAGES << " messages to force wrap around\n";
    auto producer = [&writer, &written_values]() {
        for (int i : std::ranges::iota_view{0, NUM_MESSAGES}) {
            auto wrapper = writer.blocking_claim_buffer(sizeof(uint64_t));
            const uint64_t value = static_cast<uint64_t>(i);
            ASSERT_TRUE(wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(uint64_t)));
            written_values.push_back(value);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    };

    auto consumer = [&reader, &read_values]() {
        while (read_values.size() < NUM_MESSAGES) {
            auto wrapper = reader.poll_buffer();
            if (wrapper.has_value()) {
                uint64_t value;
                std::memcpy(&value, wrapper->get_buffer(), sizeof(uint64_t));
                read_values.push_back(value);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    };

    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);

    producer_thread.join();
    consumer_thread.join();

    EXPECT_EQ(written_values.size(), read_values.size());
    for (size_t i = 0; i < written_values.size(); ++i) {
        EXPECT_EQ(written_values[i], read_values[i]);
    }
}

TEST_F(MpscIpcQueueTest, ExceedQueueCapacity) {
    MpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);

    ASSERT_THROW(auto wrapper = writer.blocking_claim_buffer(QUEUE_SIZE + 1), std::runtime_error);
}

TEST_F(MpscIpcQueueTest, ReaderCannotClaim) {
    MpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](MpscIpcQueueRaiiWrapper){});
    EXPECT_THROW(reader.blocking_claim_buffer(64), std::runtime_error);
}

TEST_F(MpscIpcQueueTest, WriterCannotPoll) {
    MpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);

    EXPECT_THROW(writer.poll_buffer(), std::runtime_error);
}

TEST_F(MpscIpcQueueTest, LargeMessageSequence) {
    MpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](MpscIpcQueueRaiiWrapper){});

    const size_t large_msg_size = 512;
    std::vector<std::vector<char>> written_data;
    std::vector<std::vector<char>> read_data;

    auto producer = [&writer, &written_data, large_msg_size]() {
        for (int i = 0; i < 5; ++i) {
            std::vector<char> data(large_msg_size, static_cast<char>(i));
            auto wrapper = writer.blocking_claim_buffer(large_msg_size);
            wrapper.write_to_buffer(data.data(), data.size());
            written_data.push_back(data);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    };

    auto consumer = [&reader, &read_data, large_msg_size]() {
        int count = 0;
        while (count < 5) {
            auto wrapper = reader.poll_buffer();
            if (wrapper.has_value()) {
                std::vector<char> data(large_msg_size);
                std::memcpy(data.data(), wrapper->get_buffer(), large_msg_size);
                read_data.push_back(data);
                count++;
            }
            std::this_thread::yield();
        }
    };

    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);

    producer_thread.join();
    consumer_thread.join();

    EXPECT_EQ(written_data.size(), read_data.size());
    for (size_t i = 0; i < written_data.size(); ++i) {
        EXPECT_EQ(written_data[i], read_data[i]);
    }
}
TEST_F(MpscIpcQueueTest, VariousSizedMessagesWithMultipleWraparounds) {
    const auto SMALL_QUEUE_SIZE_BYTES = 512;
    MpscIpcQueue writer(SHM_NAME, SMALL_QUEUE_SIZE_BYTES, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, SMALL_QUEUE_SIZE_BYTES, [](MpscIpcQueueRaiiWrapper){});

    struct TestMessage {
        uint8_t byte_val;
        uint32_t uint32_val;
        uint64_t uint64_val;
        unsigned long long ull_val;
    };

    std::vector<TestMessage> written_messages;
    std::vector<TestMessage> read_messages;

    const int NUM_ITERATIONS = 20; // Write enough to cause multiple wraparounds

    auto producer = [&writer, &written_messages]() {
        for (int i = 0; i < NUM_ITERATIONS; ++i) {
            TestMessage msg{
                static_cast<uint8_t>(i % 256),
                static_cast<uint32_t>(i * 1000),
                static_cast<uint64_t>(i * 1000000),
                static_cast<unsigned long long>(i * 9999999)
            };

            auto wrapper = writer.blocking_claim_buffer(sizeof(TestMessage));
            ASSERT_TRUE(wrapper.write_to_buffer(reinterpret_cast<const char*>(&msg), sizeof(TestMessage)));
            written_messages.push_back(msg);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };

    auto consumer = [&reader, &read_messages]() {
        while (read_messages.size() < NUM_ITERATIONS) {
            auto wrapper = reader.poll_buffer();
            if (wrapper.has_value()) {
                TestMessage msg;
                std::memcpy(&msg, wrapper->get_buffer(), sizeof(TestMessage));
                read_messages.push_back(msg);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
    };

    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);

    producer_thread.join();
    consumer_thread.join();

    EXPECT_EQ(written_messages.size(), read_messages.size());
    for (size_t i = 0; i < written_messages.size(); ++i) {
        EXPECT_EQ(written_messages[i].byte_val, read_messages[i].byte_val);
        EXPECT_EQ(written_messages[i].uint32_val, read_messages[i].uint32_val);
        EXPECT_EQ(written_messages[i].uint64_val, read_messages[i].uint64_val);
        EXPECT_EQ(written_messages[i].ull_val, read_messages[i].ull_val);
    }
}

TEST_F(MpscIpcQueueTest, LongRunningProducerConsumer) {
    MpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](MpscIpcQueueRaiiWrapper){});

    const size_t NUM_MESSAGES = 4096;
    std::vector<uint64_t> written_values;
    std::vector<uint64_t> read_values;
    written_values.reserve(NUM_MESSAGES);
    read_values.reserve(NUM_MESSAGES);

    auto producer = [&writer, &written_values, NUM_MESSAGES]() {
        for (uint64_t i = 0; i < NUM_MESSAGES; ++i) {
            auto wrapper = writer.blocking_claim_buffer(sizeof(uint64_t));
            const uint64_t value = i;
            ASSERT_TRUE(wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(uint64_t)));
            written_values.push_back(value);
            std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        }
    };

    auto consumer = [&reader, &read_values, NUM_MESSAGES]() {
        while (read_values.size() < NUM_MESSAGES) {
            auto wrapper = reader.poll_buffer();
            if (wrapper.has_value()) {
                uint64_t value;
                std::memcpy(&value, wrapper->get_buffer(), sizeof(uint64_t));
                read_values.push_back(value);
            }
            std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        }
    };

    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);

    producer_thread.join();
    consumer_thread.join();

    EXPECT_EQ(written_values.size(), read_values.size());
    for (size_t i = 0; i < written_values.size(); ++i) {
        EXPECT_EQ(written_values[i], read_values[i]);
    }
}

TEST_F(MpscIpcQueueTest, TwoProducersOneConsumer) {
    MpscIpcQueue writer1(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](MpscIpcQueueRaiiWrapper){});

    const size_t msg_size = sizeof(int32_t);
    const size_t available_space = QUEUE_SIZE - sizeof(message_transport::GlobalHeader);
    const size_t msgs_per_cycle = available_space / (msg_size + sizeof(message_transport::MessageHeader));
    const int num_messages_per_producer = (3 * msgs_per_cycle / 2) - 1;

    std::cout << "Each producer will write " << num_messages_per_producer << " messages, total messages: " << num_messages_per_producer * 2 << "\n";

    std::unordered_set<int32_t> written_values;
    std::unordered_set<int32_t> read_values;
    std::mutex written_mutex, read_mutex;

    auto producer1 = [&writer1, &written_values, &written_mutex, num_messages_per_producer, msg_size]() {
        for (int i : std::ranges::iota_view{0, num_messages_per_producer}) {
            const int32_t value = i * 2; // producer 1 writes even numbers
            auto wrapper = writer1.blocking_claim_buffer(msg_size);
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), msg_size);
            {
                std::lock_guard lock(written_mutex);
                written_values.insert(value);
            }
        }
    };

    auto producer2 = [&writer1, &written_values, &written_mutex, num_messages_per_producer, msg_size]() {
        for (int i : std::ranges::iota_view{0, num_messages_per_producer}) {
            const int32_t value = i + num_messages_per_producer;
            auto wrapper = writer1.blocking_claim_buffer(msg_size);
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), msg_size);
            {
                std::lock_guard lock(written_mutex);
                written_values.insert(value);
            }
        }
    };

    auto producer3 = [&writer1, &written_values, &written_mutex, num_messages_per_producer, msg_size]() {
        for (int i : std::ranges::iota_view{0, num_messages_per_producer}) {
            const int32_t value = i + (num_messages_per_producer * 2);
            auto wrapper = writer1.blocking_claim_buffer(msg_size);
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), msg_size);
            {
                std::lock_guard lock(written_mutex);
                written_values.insert(value);
            }
        }
    };

    auto consumer = [&reader, &read_values, &read_mutex, total_msgs = num_messages_per_producer * 3, msg_size]() {
        size_t count = 0;
        while (count < (total_msgs)) {
            auto wrapper = reader.poll_buffer();
            if (wrapper.has_value()) {
                int value;
                std::memcpy(&value, wrapper->get_buffer(), msg_size);
                {
                    std::lock_guard lock(read_mutex);
                    read_values.insert(value);
                    spdlog::info("Consumer read value: {}, total read so far: {}, total expected: {}", value, read_values.size(), total_msgs - 2);
                }
                count++;
            }
            std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        }
    };

    std::thread producer1_thread(producer1);
    std::thread producer2_thread(producer2);
    std::thread producer3_thread(producer3);
    std::thread consumer_thread(consumer);

    producer1_thread.join();
    producer2_thread.join();
    producer3_thread.join();
    consumer_thread.join();

    EXPECT_EQ(written_values.size(), read_values.size());
    EXPECT_EQ(written_values, read_values);
}

TEST_F(MpscIpcQueueTest, MultiProducerDifferentTypes) {
    MpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    MpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](MpscIpcQueueRaiiWrapper){});

    const int NUM_MESSAGES = 50;
    std::unordered_set<uint64_t> written_values;
    std::unordered_set<uint64_t> read_values;
    std::mutex written_mutex, read_mutex;

    auto byte_producer = [&writer, &written_values, &written_mutex, NUM_MESSAGES]() {
        for (int i = 1; i <= NUM_MESSAGES; ++i) {
            const uint8_t value = static_cast<uint8_t>(i);
            auto wrapper = writer.blocking_claim_buffer(sizeof(uint8_t));
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(uint8_t));
            {
                std::lock_guard lock(written_mutex);
                written_values.insert(static_cast<uint64_t>(value));
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    auto uint32_producer = [&writer, &written_values, &written_mutex, NUM_MESSAGES]() {
        for (int i = 1; i <= NUM_MESSAGES; ++i) {
            const uint32_t value = static_cast<uint32_t>(i * 1000);
            auto wrapper = writer.blocking_claim_buffer(sizeof(uint32_t));
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(uint32_t));
            {
                std::lock_guard lock(written_mutex);
                written_values.insert(static_cast<uint64_t>(value));
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    auto uint64_producer = [&writer, &written_values, &written_mutex, NUM_MESSAGES]() {
        for (int i = 1; i <= NUM_MESSAGES; ++i) {
            const uint64_t value = static_cast<uint64_t>(i * 100000);
            auto wrapper = writer.blocking_claim_buffer(sizeof(uint64_t));
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
            {
                std::lock_guard lock(written_mutex);
                written_values.insert(value);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    auto consumer = [&reader, &read_values, &read_mutex, total_msgs = NUM_MESSAGES * 3]() {
        size_t count = 0;
        while (count < total_msgs) {
            auto wrapper = reader.poll_buffer();
            if (wrapper.has_value()) {
                switch (wrapper->get_payload_size()) {
                    case sizeof(uint8_t): {
                        uint8_t value;
                        std::memcpy(&value, wrapper->get_buffer(), sizeof(uint8_t));
                        {
                            std::lock_guard lock(read_mutex);
                            read_values.insert(static_cast<uint64_t>(value));
                        }
                        break;
                    }
                    case sizeof(uint32_t): {
                        uint32_t value;
                        std::memcpy(&value, wrapper->get_buffer(), sizeof(uint32_t));
                        {
                            std::lock_guard lock(read_mutex);
                            read_values.insert(static_cast<uint64_t>(value));
                        }
                        break;
                    }
                    case sizeof(uint64_t): {
                        uint64_t value;
                        std::memcpy(&value, wrapper->get_buffer(), sizeof(uint64_t));
                        {
                            std::lock_guard lock(read_mutex);
                            read_values.insert(value);
                        }
                        break;
                    }
                }
                count++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    };

    std::thread byte_thread(byte_producer);
    std::thread uint32_thread(uint32_producer);
    std::thread uint64_thread(uint64_producer);
    std::thread consumer_thread(consumer);

    byte_thread.join();
    uint32_thread.join();
    uint64_thread.join();
    consumer_thread.join();

    EXPECT_EQ(written_values.size(), read_values.size());
    EXPECT_EQ(written_values, read_values);

    // std::unordered_set<uint64_t> outer_join;
    // std::set_symmetric_difference(
    //     written_values.begin(), written_values.end(),
    //     read_values.begin(), read_values.end(),
    //     std::inserter(outer_join, outer_join.begin())
    // );

    // if (!outer_join.empty()) {
    //     std::cout << "Outer join (values in one set but not both):\n";
    //     for (const auto& value : outer_join) {
    //         std::cout << "  " << value;
    //         if (written_values.count(value)) {
    //             std::cout << " (written only)";
    //         } else {
    //             std::cout << " (read only)";
    //         }
    //         std::cout << "\n";
    //     }
    // } else {
    //     std::cout << "No differences found - sets are identical\n";
    // }
}
