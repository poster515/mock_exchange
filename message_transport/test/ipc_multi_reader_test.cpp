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
        // shm_unlink(SHM_NAME);
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