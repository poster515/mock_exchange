#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <ranges>
#include <unordered_set>
#include <future>
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

TEST_F(IpcQueueMultiReaderTest, MultipleJoinReaders) {
    message_transport::IpcQueue writer{
        message_transport::IpcQueue::IpcQueueParameters{
            .file_name = SHM_NAME,
            .queue_size = QUEUE_SIZE,
            .is_writer = true
        }
    };

    const size_t msg_size = 64;
    const size_t available_space = QUEUE_SIZE - sizeof(message_transport::MessageHeader);
    const int num_messages_to_fill = available_space / (msg_size + sizeof(message_transport::MessageHeader));

    std::vector<int> written_values;
    std::atomic<bool> stop(false);
    std::atomic<bool> writer_stopped(false);

    std::thread producer ([&stop, &writer, &written_values, &writer_stopped]() {
        int value = 1;
        while (!stop) {
            auto wrapper = writer.claim_buffer<message_transport::SleepPolicy>(msg_size);
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(int));
            written_values.push_back(value++);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        writer_stopped = true;
    });

    static constexpr int NUM_THREADS = 4;
    std::vector<std::thread> threads;
    std::vector<std::future<std::vector<int>>> futures;
    for (int i : std::ranges::iota_view{0, NUM_THREADS}) {
        
        std::promise<std::vector<int>> promise;
        futures.push_back(promise.get_future());
        auto func = [&writer_stopped](std::promise<std::vector<int>>&& promise){
            std::vector<int> values;
            IpcQueue reader(
                message_transport::IpcQueue::IpcQueueParameters{
                    .file_name = SHM_NAME,
                    .queue_size = QUEUE_SIZE,
                    .is_writer = false
                }
            );

            while (true) {
                auto read_wrapper = reader.poll_buffer();
                if (!read_wrapper.has_value()) {
                    if (writer_stopped) break;
                    else continue;
                }
                int v;
                std::memcpy(&v, read_wrapper->get_buffer(), sizeof(int));
                read_wrapper->release();
                values.push_back(v);

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            promise.set_value(values);
        };

        threads.push_back(std::thread(func, std::move(promise)));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // let these guys run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    stop = true;
    if (producer.joinable()) producer.join();
    for (auto& t : threads) { if (t.joinable()) t.join(); }

    std::vector<std::vector<int>> results;
    for (auto& f : futures) { results.push_back(f.get()); }

    auto start_view = results | std::views::transform([](const std::vector<int>& h) { return h.empty() ? -1 : h.front(); });
    const auto start_values = std::unordered_set<int>(start_view.begin(), start_view.end());

    auto end_view = results | std::views::transform([](const std::vector<int>& h) { return h.empty() ? -1 : h.back(); });
    const auto end_values = std::unordered_set<int>(end_view.begin(), end_view.end());

    EXPECT_FALSE(start_values.contains(-1));
    EXPECT_FALSE(end_values.contains(-1));

    EXPECT_TRUE(start_values.size() == NUM_THREADS);
    EXPECT_TRUE(end_values.size() == 1);
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


TEST_F(IpcQueueMultiReaderTest, MultiProducerDifferentTypes) {
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
            .is_writer = false,
            .agent_name = "reader1"
        }
    );

    IpcQueue reader2(
        message_transport::IpcQueue::IpcQueueParameters{
            .file_name = SHM_NAME,
            .queue_size = QUEUE_SIZE,
            .is_writer = false,
            .agent_name = "reader2"
        }
    );

    const int NUM_MESSAGES = 50000;
    std::unordered_set<uint64_t> written_values;

    std::unordered_set<uint8_t> uint8_values;
    auto byte_producer = [&writer, &uint8_values, NUM_MESSAGES]() {
        for (int i = 1; i <= NUM_MESSAGES; ++i) {
            const uint8_t value = static_cast<uint8_t>(i);
            auto wrapper = writer.claim_buffer<message_transport::SleepPolicy>(sizeof(uint8_t));
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(uint8_t));
            uint8_values.insert(value);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::unordered_set<uint32_t> uint32_values;
    auto uint32_producer = [&writer, &uint32_values, NUM_MESSAGES]() {
        for (int i = 1; i <= NUM_MESSAGES; ++i) {
            const uint32_t value = static_cast<uint32_t>(i + NUM_MESSAGES);
            auto wrapper = writer.claim_buffer<message_transport::SleepPolicy>(sizeof(uint32_t));
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(uint32_t));
            uint32_values.insert(value);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::unordered_set<uint64_t> uint64_values;
    auto uint64_producer = [&writer, &uint64_values, NUM_MESSAGES]() {
        for (int i = 1; i <= NUM_MESSAGES; ++i) {
            const uint64_t value = static_cast<uint64_t>(i + (NUM_MESSAGES * 2));
            auto wrapper = writer.claim_buffer<message_transport::SleepPolicy>(sizeof(uint64_t));
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
            uint64_values.insert(value);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::unordered_set<std::string> written_strings;
    auto str_producer = [&writer, &written_strings, NUM_MESSAGES]() {
        for (int i = 1; i <= NUM_MESSAGES; ++i) {
            const auto msg = std::format("Hello #{}!!!!", i);
            auto wrapper = writer.claim_buffer<message_transport::SleepPolicy>(msg.size());
            wrapper.write_to_buffer(msg.c_str(), msg.size());
            written_strings.insert(msg);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::unordered_set<std::string> reader1_strings, reader2_strings;
    std::unordered_set<uint64_t> reader1_values, reader2_values;
    auto consumer1 = [&reader1, &reader1_values, &reader1_strings, total_msgs = NUM_MESSAGES * 4]() {
        size_t count = 0;
        while (count < total_msgs) {
            auto wrapper = reader1.poll_buffer();
            if (wrapper.has_value()) {
                switch (wrapper->get_payload_size()) {
                    case sizeof(uint8_t): {
                        uint8_t value;
                        std::memcpy(&value, wrapper->get_buffer(), sizeof(uint8_t));
                        reader1_values.insert(static_cast<uint64_t>(value));
                        break;
                    }
                    case sizeof(uint32_t): {
                        uint32_t value;
                        std::memcpy(&value, wrapper->get_buffer(), sizeof(uint32_t));
                        reader1_values.insert(static_cast<uint64_t>(value));
                        break;
                    }
                    case sizeof(uint64_t): {
                        uint64_t value;
                        std::memcpy(&value, wrapper->get_buffer(), sizeof(uint64_t));
                        reader1_values.insert(value);
                        break;
                    }
                    default :{
                        reader1_strings.insert(std::string(wrapper->get_as_view<std::string_view>()));
                        break;
                    }
                }
                ++count;
                wrapper->release();
                // spdlog::info("Consumer read value. Total read so far: {}, total expected: {}", count, NUM_MESSAGES * 4);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    };

    auto consumer2 = [&reader2, &reader2_values, &reader2_strings, total_msgs = NUM_MESSAGES * 4]() {
        size_t count = 0;
        while (count < total_msgs) {
            auto wrapper = reader2.poll_buffer();
            if (wrapper.has_value()) {
                switch (wrapper->get_payload_size()) {
                    case sizeof(uint8_t): {
                        uint8_t value;
                        std::memcpy(&value, wrapper->get_buffer(), sizeof(uint8_t));
                        reader2_values.insert(static_cast<uint64_t>(value));
                        break;
                    }
                    case sizeof(uint32_t): {
                        uint32_t value;
                        std::memcpy(&value, wrapper->get_buffer(), sizeof(uint32_t));
                        reader2_values.insert(static_cast<uint64_t>(value));
                        break;
                    }
                    case sizeof(uint64_t): {
                        uint64_t value;
                        std::memcpy(&value, wrapper->get_buffer(), sizeof(uint64_t));
                        reader2_values.insert(value);
                        break;
                    }
                    default :{
                        reader2_strings.insert(std::string(wrapper->get_as_view<std::string_view>()));
                        break;
                    }
                }
                ++count;
                wrapper->release();
                // spdlog::info("Consumer read value. Total read so far: {}, total expected: {}", count, NUM_MESSAGES * 4);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    };

    std::thread byte_thread(byte_producer);
    std::thread uint32_thread(uint32_producer);
    std::thread uint64_thread(uint64_producer);
    std::thread str_thread(str_producer);
    std::thread consumer_thread1(consumer1);
    std::thread consumer_thread2(consumer2);

    byte_thread.join();
    uint32_thread.join();
    uint64_thread.join();
    str_thread.join();
    consumer_thread1.join();
    consumer_thread2.join();

    written_values.insert(uint8_values.begin(), uint8_values.end());
    written_values.insert(uint32_values.begin(), uint32_values.end());
    written_values.insert(uint64_values.begin(), uint64_values.end());

    EXPECT_EQ(written_values.size(), reader1_values.size());
    EXPECT_EQ(written_values, reader1_values);
    EXPECT_EQ(written_values.size(), reader2_values.size());
    EXPECT_EQ(written_values, reader2_values);

    EXPECT_EQ(written_strings.size(), reader1_strings.size());
    EXPECT_EQ(written_strings, reader1_strings);
    EXPECT_EQ(written_strings.size(), reader2_strings.size());
    EXPECT_EQ(written_strings, reader2_strings);

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
