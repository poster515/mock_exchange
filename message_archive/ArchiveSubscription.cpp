#include "archive/ArchiveSubscription.h"

#include <ranges>
#include <algorithm>

namespace archive {
    ArchiveSubscription::ArchiveSubscription(ArchiveSubscriptionParams&& params)
            : queue(nullptr) {
        queue = std::make_unique<message_transport::IpcQueue>(message_transport::IpcQueue::IpcQueueParameters{
            .file_name = params.queue_params.file_name,
            .queue_size = params.queue_params.queue_size,
            .is_writer = false
        });
    }

    ArchiveSubscription::~ArchiveSubscription() {
        if (queue) close();
    }
 
    bool ArchiveSubscription::is_ready() const {
        return true;
    }

    void ArchiveSubscription::close() {
        // calls the queue destructor
        queue->close();
        queue.reset();
    }
}