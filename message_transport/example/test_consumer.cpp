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
    std::vector<uint32_t> received_values(TOTAL_MESSAGES);
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
            // spdlog::info("Received value {}, total_received: {}", value, received_values.size());
        }
        std::this_thread::sleep_for(std::chrono::nanoseconds(10));
    }

    const auto end = std::chrono::high_resolution_clock::now();

    spdlog::info("Received all {} values in {} us", TOTAL_MESSAGES, (end - start).count());
    assert(received_values.size() == TOTAL_MESSAGES);

    // uint32_t expected = 0;
    // for (int i : std::ranges::iota_view{0, TOTAL_MESSAGES}) {
    //     if (!received_values.contains(static_cast<uint32_t>(i))) {
    //         spdlog::error("Expected value {} but did not find it!!!", i);
    //     }
    // }
}