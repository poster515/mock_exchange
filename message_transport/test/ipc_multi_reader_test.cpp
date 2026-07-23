#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <ranges>
#include <unordered_set>
#include <sys/mman.h>

#include "messaging/ipc_queue.h"
#include "messaging/ipc_queue_element_wrapper.h"

using namespace message_transport;

class IpcQueueMultiReaderTest : public ::testing::Test {
protected:
    static constexpr const char* SHM_NAME = "/ipc_queue_test";
    static constexpr size_t QUEUE_SIZE = 4096;
    static constexpr size_t SMALL_QUEUE_SIZE = 128;

    void SetUp() override {
        shm_unlink(SHM_NAME);
    }

    void TearDown() override {
        // shm_unlink(SHM_NAME);
    }
};

TEST_F(IpcQueueMultiReaderTest, BasicWriteAndRead) {
    message_transport::IpcQueue writer{
        message_transport::IpcQueue::IpcQueueParameters{
            .file_name = SHM_NAME,
            .queue_size = QUEUE_SIZE,
            .is_writer = true
        }
    };
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    IpcQueue reader1(
        message_transport::IpcQueue::IpcQueueParameters{
            .file_name = SHM_NAME,
            .queue_size = QUEUE_SIZE,
            .is_writer = false
        }
    );
    IpcQueue reader2(
        message_transport::IpcQueue::IpcQueueParameters{
            .file_name = SHM_NAME,
            .queue_size = QUEUE_SIZE,
            .is_writer = false
        }
    );

    std::string_view test_data = "Hello, World!";

    auto wrapper = writer.claim_buffer<message_transport::SleepPolicy>(test_data.size());
    ASSERT_TRUE(wrapper.write_to_buffer(test_data.data(), test_data.size()));
    wrapper.~IpcQueueRaiiWriterWrapper(); // explicitly call the destructor to commit the message to the queue

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto read_wrapper1 = reader1.poll_buffer();
    ASSERT_TRUE(read_wrapper1.has_value());
    auto read_data1 = read_wrapper1->get_as_view<std::string_view>();
    EXPECT_EQ(read_data1, test_data);

    auto read_wrapper2 = reader2.poll_buffer();
    ASSERT_TRUE(read_wrapper2.has_value());
    auto read_data2 = read_wrapper2->get_as_view<std::string_view>();
    EXPECT_EQ(read_data2, test_data);
}

TEST_F(IpcQueueMultiReaderTest, ProducerBlocksWhenQueueFull) {
    message_transport::IpcQueue writer{
        message_transport::IpcQueue::IpcQueueParameters{
            .file_name = SHM_NAME,
            .queue_size = QUEUE_SIZE,
            .is_writer = true
        }
    };
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    IpcQueue reader1(
        message_transport::IpcQueue::IpcQueueParameters{
            .file_name = SHM_NAME,
            .queue_size = QUEUE_SIZE,
            .is_writer = false
        }
    );

    IpcQueue reader2(
        message_transport::IpcQueue::IpcQueueParameters{
            .file_name = SHM_NAME,
            .queue_size = QUEUE_SIZE,
            .is_writer = false
        }
    );

    const size_t msg_size = 64;
    const size_t available_space = QUEUE_SIZE - sizeof(message_transport::MessageHeader);
    const int num_messages_to_fill = available_space / (msg_size + sizeof(message_transport::MessageHeader));

    std::vector<int> written_values;
    std::atomic<bool> producer_blocked(false);

    // Fill the queue
    for (int i : std::ranges::iota_view{0, num_messages_to_fill}) {
        auto wrapper = writer.claim_buffer<message_transport::SleepPolicy>(msg_size);
        int value { i };
        wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(int));
        written_values.push_back(value);
    }

    // Start producer thread that will block trying to write
    std::thread producer([&writer, &producer_blocked, msg_size]() {
        producer_blocked.store(true, std::memory_order_release);
        int value = 999;
        auto wrapper = writer.claim_buffer<message_transport::SleepPolicy>(msg_size);
        wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(int));
        producer_blocked.store(false, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(producer_blocked.load(std::memory_order_acquire));

    // Read messages one by one to free up space
    for (int i : std::ranges::iota_view{0, num_messages_to_fill}) {
        auto read_wrapper = reader1.poll_buffer();
        ASSERT_TRUE(read_wrapper.has_value());
        int value;
        std::memcpy(&value, read_wrapper->get_buffer(), sizeof(int));
        EXPECT_EQ(value, i);
        read_wrapper->release();

        // writer should still be blocked on very first message until reader2 releases
        if (i == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            EXPECT_TRUE(producer_blocked.load(std::memory_order_acquire));
        }

        auto read_wrapper2 = reader2.poll_buffer();
        ASSERT_TRUE(read_wrapper2.has_value());
        read_wrapper2->release();
    }

    // Producer should now be able to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(producer_blocked.load(std::memory_order_acquire));

    producer.join();

    // Verify the last message from producer
    auto final_read = reader1.poll_buffer();
    ASSERT_TRUE(final_read.has_value());
    int final_value;
    std::memcpy(&final_value, final_read->get_buffer(), sizeof(int));
    EXPECT_EQ(final_value, 999);
}


TEST_F(IpcQueueMultiReaderTest, BasicQueueWrapping) {
    message_transport::IpcQueue writer{
        message_transport::IpcQueue::IpcQueueParameters{
            .file_name = SHM_NAME,
            .queue_size = SMALL_QUEUE_SIZE,
            .is_writer = true
        }
    };
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    IpcQueue reader1(
        message_transport::IpcQueue::IpcQueueParameters{
            .file_name = SHM_NAME,
            .queue_size = SMALL_QUEUE_SIZE,
            .is_writer = false
        }
    );
    IpcQueue reader2(
        message_transport::IpcQueue::IpcQueueParameters{
            .file_name = SHM_NAME,
            .queue_size = SMALL_QUEUE_SIZE,
            .is_writer = false
        }
    );

    std::string_view message = "this_is_a_long_message";
    const auto iters_to_fill_buffer = (SMALL_QUEUE_SIZE - sizeof(message_transport::MessageHeader)) / (message.size() + sizeof(message_transport::MessageHeader));

    for (auto i = 0; i < iters_to_fill_buffer; ++i) {
        auto wrapper = writer.claim_buffer<message_transport::SleepPolicy>(message.size());
        ASSERT_TRUE(wrapper.write_to_buffer(message.data(), message.size()));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // now the queue is full and we want to make sure the wrapping works correctly
    // need to consume one message though to free up enough space for a new message at the front
    auto read_wrapper = reader1.poll_buffer();
    ASSERT_TRUE(read_wrapper.has_value());
    auto read_data = read_wrapper->get_as_view<std::string_view>();
    EXPECT_EQ(read_data, message);
    read_wrapper->release(); // have to manually release (or destroy) here.

    auto read_wrapper2 = reader2.poll_buffer();
    ASSERT_TRUE(read_wrapper2.has_value());
    auto read_data2 = read_wrapper2->get_as_view<std::string_view>();
    EXPECT_EQ(read_data2, message);
    read_wrapper2->release(); // have to manually release (or destroy) here.

    // now we can write one more message which should wrap around to the beginning of the queue
    {
        auto wrapper = writer.claim_buffer<message_transport::SleepPolicy>(message.size());
        ASSERT_TRUE(wrapper.write_to_buffer(message.data(), message.size()));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // anddddd then read everything we can
    while (auto read_wrapper = reader1.poll_buffer()) {
        ASSERT_TRUE(read_wrapper.has_value());
        auto read_data = read_wrapper->get_as_view<std::string_view>();
        EXPECT_EQ(read_data, message);

        auto read_wrapper2 = reader2.poll_buffer();
        ASSERT_TRUE(read_wrapper2.has_value());
        auto read_data2 = read_wrapper2->get_as_view<std::string_view>();
        EXPECT_EQ(read_data2, message);
    }
}