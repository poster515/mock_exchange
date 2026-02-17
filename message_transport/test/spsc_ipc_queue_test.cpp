#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <sys/mman.h>

#include "messaging/spsc_ipc_queue.h"
#include "messaging/spsc_ipc_queue_element_wrapper.h"

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
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, false);

    const char* test_data = "Hello, World!";
    size_t data_size = strlen(test_data) + 1;

    auto wrapper = writer.claim_buffer(data_size);
    ASSERT_NE(wrapper.get_buffer(), nullptr);
    wrapper.write_to_buffer(test_data, data_size);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto read_wrapper = reader.poll_buffer();
    ASSERT_TRUE(read_wrapper.has_value());
    EXPECT_EQ(std::memcmp(read_wrapper->get_buffer(), test_data, data_size), 0);
}

TEST_F(SpscIpcQueueTest, MultipleMessagesSequential) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, false);

    std::vector<std::string> messages = {"msg1", "msg2", "msg3"};

    for (const auto& msg : messages) {
        auto wrapper = writer.claim_buffer(msg.size() + 1);
        ASSERT_NE(wrapper.get_buffer(), nullptr);
        wrapper.write_to_buffer(msg.c_str(), msg.size() + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    for (const auto& expected_msg : messages) {
        auto read_wrapper = reader.poll_buffer();
        ASSERT_TRUE(read_wrapper.has_value());
        EXPECT_STREQ(static_cast<const char*>(read_wrapper->get_buffer()), expected_msg.c_str());
    }
}

TEST_F(SpscIpcQueueTest, SlowProducerFastConsumer) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, false);

    std::vector<int> written_values;
    std::vector<int> read_values;

    auto producer = [&writer, &written_values]() {
        for (int i = 0; i < 10; ++i) {
            auto wrapper = writer.claim_buffer(sizeof(int));
            ASSERT_NE(wrapper.get_buffer(), nullptr);
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&i), sizeof(int));
            // wrapper.commit(); // commit is now handled by the destructor of the wrapper, so we don't need to call it explicitly here.
            written_values.push_back(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    };

    auto consumer = [&reader, &read_values]() {
        while (read_values.size() < 10) {
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
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, false);

    std::vector<int> written_values;
    std::vector<int> read_values;
    std::mutex values_mutex;

    auto producer = [&writer, &written_values, &values_mutex]() {
        for (int i = 0; i < 20; ++i) {
            auto wrapper = writer.claim_buffer(sizeof(int));
            if (wrapper.get_buffer() != nullptr) {
                wrapper.write_to_buffer(reinterpret_cast<const char*>(&i), sizeof(int));
                {
                    std::lock_guard<std::mutex> lock(values_mutex);
                    written_values.push_back(i);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    };

    auto consumer = [&reader, &read_values, &values_mutex]() {
        int count = 0;
        while (count < 20) {
            auto wrapper = reader.poll_buffer();
            if (wrapper.has_value()) {
                int value;
                std::memcpy(&value, wrapper->get_buffer(), sizeof(int));
                {
                    std::lock_guard<std::mutex> lock(values_mutex);
                    read_values.push_back(value);
                }
                count++;
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

TEST_F(SpscIpcQueueTest, QueueWrapAround) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, false);

    const size_t msg_size = 128;
    const int num_messages = 40;
    std::vector<int> written_values;
    std::vector<int> read_values;

    auto producer = [&writer, &written_values, msg_size, num_messages]() {
        for (int i = 0; i < num_messages; ++i) {
            auto wrapper = writer.claim_buffer(msg_size);
            if (wrapper.get_buffer() != nullptr) {
                wrapper.write_to_buffer(reinterpret_cast<const char*>(&i), sizeof(int));
                written_values.push_back(i);
            }
        }
    };

    auto consumer = [&reader, &read_values, num_messages]() {
        int count = 0;
        while (count < num_messages) {
            auto wrapper = reader.poll_buffer();
            if (wrapper.has_value()) {
                int value;
                std::memcpy(&value, wrapper->get_buffer(), sizeof(int));
                read_values.push_back(value);
                count++;
            }
            std::this_thread::yield();
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
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, true);

    auto wrapper = writer.claim_buffer(QUEUE_SIZE + 1);
    EXPECT_EQ(wrapper.get_buffer(), nullptr);
}

TEST_F(SpscIpcQueueTest, ReaderCannotClaim) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, false);

    EXPECT_THROW(reader.claim_buffer(64), std::runtime_error);
}

TEST_F(SpscIpcQueueTest, WriterCannotPoll) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, true);

    EXPECT_THROW(writer.poll_buffer(), std::runtime_error);
}

TEST_F(SpscIpcQueueTest, LargeMessageSequence) {
    SpscIpcQueue writer(SHM_NAME, QUEUE_SIZE, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SpscIpcQueue reader(SHM_NAME, QUEUE_SIZE, false);

    const size_t large_msg_size = 512;
    std::vector<std::vector<char>> written_data;
    std::vector<std::vector<char>> read_data;

    auto producer = [&writer, &written_data, large_msg_size]() {
        for (int i = 0; i < 5; ++i) {
            std::vector<char> data(large_msg_size, static_cast<char>(i));
            auto wrapper = writer.claim_buffer(large_msg_size);
            if (wrapper.get_buffer() != nullptr) {
                wrapper.write_to_buffer(data.data(), large_msg_size);
                written_data.push_back(data);
            }
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