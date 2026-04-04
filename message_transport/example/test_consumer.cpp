#include <set>

#include <assert.h>
#include <format>

#include <spdlog/spdlog.h>

#include "messaging/mpsc_ipc_queue.h"
#include "messaging/mpsc_ipc_queue_element_wrapper.h"

int main() {
    message_transport::MpscIpcQueue queue(
        message_transport::MpscIpcQueue::MpscQueueParameters{
            .file_name = "/dev/shm/queue_test",
            .queue_size = message_transport::MpscIpcQueue::MAX_QUEUE_SIZE_BYTES,
            .is_writer = false
        }
    );

    const int TOTAL_MESSAGES = 1'000'000;
    std::vector<uint32_t> received_values;
    received_values.reserve(TOTAL_MESSAGES);
    spdlog::info("Consumer started, waiting for {} values", TOTAL_MESSAGES);

    size_t count = 0;
    auto start = std::chrono::high_resolution_clock::now();
    while (count < TOTAL_MESSAGES) {
        auto wrapper = queue.poll_buffer();
        if (wrapper.has_value()) {
            uint32_t value;
            assert(wrapper->get_payload_size() == sizeof(uint32_t));
            received_values.push_back(wrapper->get_as<uint32_t>());
            ++count;
        }
        std::this_thread::sleep_for(std::chrono::nanoseconds(10));
    }

    const auto end = std::chrono::high_resolution_clock::now();

    spdlog::info("Received all {} values in {} us, recv_size: {}", TOTAL_MESSAGES, (end - start).count(), received_values.size());
    assert(received_values.size() == TOTAL_MESSAGES);
}