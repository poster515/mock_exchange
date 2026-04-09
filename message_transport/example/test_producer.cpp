#include <ranges>
#include <barrier>

#include "messaging/mpsc_ipc_queue.h"
#include "messaging/mpsc_ipc_queue_element_wrapper.h"

int main() {
    message_transport::MpscIpcQueue queue{
        message_transport::MpscIpcQueue::MpscQueueParameters{
            .file_name = "/dev/shm/queue_test",
            .queue_size = message_transport::MpscIpcQueue::MAX_QUEUE_SIZE_BYTES,
            .is_writer = true
        }
    };

    const int TOTAL_MESSAGES = 1'000'000u;

    std::barrier start_barrier(3);
    std::atomic_bool start {false};

    auto producer1 = std::thread([&]() {
        start_barrier.arrive_and_wait();
        while (!start.load(std::memory_order_acquire));
        size_t sent = 0;
        for (int i : std::ranges::iota_view{0, TOTAL_MESSAGES / 2}) {
            auto wrapper = queue.claim_buffer<message_transport::SleepPolicy>(sizeof(uint32_t));
            const uint32_t temp = static_cast<uint32_t>(i);
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&temp), sizeof(int));
            ++sent;
        }
        spdlog::info("Producer1 sent {} messages", sent);
    });

    auto producer2 = std::thread([&]() {
        start_barrier.arrive_and_wait();
        while (!start.load(std::memory_order_acquire));
        size_t sent = 0;
        for (int i : std::ranges::iota_view{TOTAL_MESSAGES / 2, TOTAL_MESSAGES}) {
            auto wrapper = queue.claim_buffer<message_transport::SleepPolicy>(sizeof(uint32_t));
            const uint32_t temp = static_cast<uint32_t>(i);
            wrapper.write_to_buffer(reinterpret_cast<const char*>(&temp), sizeof(int));
            ++sent;
        }
        spdlog::info("Producer2 sent {} messages", sent);
    });

    start_barrier.arrive_and_wait();
    start.store(true, std::memory_order_release);

    producer1.join();
    producer2.join();
}