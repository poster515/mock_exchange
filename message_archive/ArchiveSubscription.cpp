#include "archive/ArchiveSubscription.h"

#include <ranges>
#include <algorithm>

namespace archive {
    ArchiveSubscription::ArchiveSubscription(ArchiveSubscriptionParams&& params)
        : queue(message_transport::IpcQueue::IpcQueueParameters{
            .file_name = params.file_name,
            .queue_size = 0,
            .is_writer = false
        }) {

    }
 
    bool ArchiveSubscription::is_ready() const {
        return true;
    }

    void ArchiveSubscription::close() {
    }

    std::span<const std::byte> ArchiveSubscription::poll_buffer() {
        return reader.has_value() ? reader->get_as_view<std::span<const std::byte>, std::byte>() : std::span<const std::byte>{};
    }
}