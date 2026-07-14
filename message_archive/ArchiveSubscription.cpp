#include "archive/ArchiveSubscription.h"

#include <ranges>
#include <algorithm>

namespace archive {
    ArchiveSubscription::ArchiveSubscription(ArchiveSubscriptionParams&& params)
            : queue(nullptr) {
        queue = std::make_unique<message_transport::IpcQueue>(message_transport::IpcQueue::IpcQueueParameters{
            .file_name = params.file_name,
            .queue_size = 0,
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
        queue.reset();
    }
}