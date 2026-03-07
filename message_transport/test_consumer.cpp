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
            .is_writer = false,
            .callback = std::nullopt
        }
    );
    std::set<uint32_t> received_values;

    spdlog::info("Consumer started, waiting for 1000 values");

    while (received_values.size() < 1000) {
        auto wrapper = queue.poll_buffer();
        if (wrapper.has_value()) {
            uint32_t value;
            assert(wrapper->get_payload_size() == sizeof(uint32_t));
            std::memcpy(&value, wrapper->get_buffer(), sizeof(uint32_t));
            received_values.insert(value);
            spdlog::info("Received value {}, total_received: {}", value, received_values.size());
        }
        std::this_thread::sleep_for(std::chrono::nanoseconds(250));
    }

    spdlog::info("Received all 1000 values");
    assert(received_values.size() == 1000);

    uint32_t expected = 0;
    for (int i : std::ranges::iota_view{1, 1001}) {
        if (!received_values.contains(static_cast<uint32_t>(i))) {
            spdlog::error("Expected value {} but did not find it!!!", i);
        }
    }
}