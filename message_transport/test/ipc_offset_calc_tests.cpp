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

class IpcQueueOffsetCalcTests : public ::testing::Test {
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


TEST_F(IpcQueueOffsetCalcTests, BasicWriteAndRead) {
    message_transport::IpcQueue::UnpackedReadersAndWriterOffset wrapper { 0 };

    EXPECT_TRUE(wrapper.num_readers == 0);
    EXPECT_TRUE(wrapper.write_offset == 0);

    auto result = wrapper.return_add_offset(10);
    wrapper.unwrap(result);
    EXPECT_TRUE(wrapper.num_readers == 0);
    EXPECT_TRUE(wrapper.write_offset == 10);

    result = wrapper.return_add_reader();
    wrapper.unwrap(result);
    EXPECT_TRUE(wrapper.num_readers == 1);
    EXPECT_TRUE(wrapper.write_offset == 10);

    result = wrapper.return_sub_reader();
    wrapper.unwrap(result);
    EXPECT_TRUE(wrapper.num_readers == 0);
    EXPECT_TRUE(wrapper.write_offset == 10);
}
