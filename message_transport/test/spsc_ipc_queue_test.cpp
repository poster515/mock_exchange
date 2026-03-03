#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <ranges>
#include <sys/mman.h>

#include "spsc_ipc_queue.h"
#include "spsc_ipc_queue_element_wrapper.h"

using namespace message_transport;

class SpscIpcQueueTest : public ::testing::Test {
protected:
    static constexpr const char* SHM_NAME = "/spsc_ipc_queue_test";
    static constexpr size_t QUEUE_SIZE = 4096;

    void SetUp() override {
        shm_unlink(SHM_NAME);
    }

    void TearDown() override {
        shm_unlink(SHM_NAME);
    }
};

TEST_F(SpscIpcQueueTest, BasicWriteAndRead) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](SpscIpcQueueRaiiWrapper){});

    std::string_view test_data = "Hello, World!";

    auto wrapper = writer.blocking_claim_buffer(test_data.size());
    ASSERT_TRUE(wrapper.write_to_buffer(test_data.data(), test_data.size()));
    wrapper.~SpscIpcQueueRaiiWriterWrapper(); // explicitly call the destructor to commit the message to the queue

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto read_wrapper = reader.poll_buffer();
    ASSERT_TRUE(read_wrapper.has_value());

    auto read_data = read_wrapper->get_as_view<std::string_view>();
    EXPECT_EQ(read_data, test_data);
}

TEST_F(SpscIpcQueueTest, BasicQueueWrapping) {
    const size_t SMALL_QUEUE_SIZE = 128;
    SpscIpcQueue writer(SHM_NAME, SMALL_QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, SMALL_QUEUE_SIZE, [](SpscIpcQueueRaiiWrapper){});

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

TEST_F(SpscIpcQueueTest, MultipleMessagesSequential) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](SpscIpcQueueRaiiWrapper){});

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

TEST_F(SpscIpcQueueTest, SlowProducerFastConsumer) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](SpscIpcQueueRaiiWrapper){});

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

TEST_F(SpscIpcQueueTest, FastProducerSlowConsumer) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](SpscIpcQueueRaiiWrapper){});

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

TEST_F(SpscIpcQueueTest, QueueWrapAroundFastProducerSlowConsumer) {

    const auto SMALL_QUEUE_SIZE_BYTES = 128;
    SpscIpcQueue writer(SHM_NAME, SMALL_QUEUE_SIZE_BYTES, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, SMALL_QUEUE_SIZE_BYTES, [](SpscIpcQueueRaiiWrapper){});

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

TEST_F(SpscIpcQueueTest, ExceedQueueCapacity) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);

    ASSERT_THROW(auto wrapper = writer.blocking_claim_buffer(QUEUE_SIZE + 1), std::runtime_error);
}

TEST_F(SpscIpcQueueTest, ReaderCannotClaim) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](SpscIpcQueueRaiiWrapper){});
    EXPECT_THROW(reader.blocking_claim_buffer(64), std::runtime_error);
}

TEST_F(SpscIpcQueueTest, WriterCannotPoll) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);

    EXPECT_THROW(writer.poll_buffer(), std::runtime_error);
}

TEST_F(SpscIpcQueueTest, LargeMessageSequence) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, [](SpscIpcQueueRaiiWrapper){});

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
TEST_F(SpscIpcQueueTest, VariousSizedMessagesWithMultipleWraparounds) {
    const auto SMALL_QUEUE_SIZE_BYTES = 512;
    SpscIpcQueue writer(SHM_NAME, SMALL_QUEUE_SIZE_BYTES, std::nullopt);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, SMALL_QUEUE_SIZE_BYTES, [](SpscIpcQueueRaiiWrapper){});

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
