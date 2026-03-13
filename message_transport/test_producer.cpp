#include <ranges>

#include "messaging/mpsc_ipc_queue.h"
#include "messaging/mpsc_ipc_queue_element_wrapper.h"

int main() {
    message_transport::MpscIpcQueue queue{
        message_transport::MpscIpcQueue::MpscQueueParameters{
            .file_name = "/dev/shm/queue_test",
            .queue_size = message_transport::MpscIpcQueue::MAX_QUEUE_SIZE_BYTES,
            .is_writer = true,
            .callback = std::nullopt
        }
    };

    const int TOTAL_MESSAGES = 1'000'000u;

    auto producer1 = std::thread([&queue]() {
        for (int i : std::ranges::iota_view{0, TOTAL_MESSAGES / 2}) {
            auto wrapper = queue.blocking_claim_buffer(sizeof(uint32_t));
            const uint32_t temp = static_cast<uint32_t>(i);
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&temp), sizeof(int));
            std::this_thread::sleep_for(std::chrono::nanoseconds(500));
        }
    });

    auto producer2 = std::thread([&queue]() {
        for (int i : std::ranges::iota_view{TOTAL_MESSAGES / 2, TOTAL_MESSAGES}) {
            auto wrapper = queue.blocking_claim_buffer(sizeof(uint32_t));
            const uint32_t temp = static_cast<uint32_t>(i);
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&temp), sizeof(int));
            std::this_thread::sleep_for(std::chrono::nanoseconds(500));
        }
    });

    producer1.join();
    producer2.join();
}