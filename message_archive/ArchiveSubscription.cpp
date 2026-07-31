#include "archive/ArchiveSubscription.h"

#include <ranges>
#include <algorithm>

namespace archive {
    ArchiveSubscription::ArchiveSubscription(ArchiveSubscriptionParams&& params)
            : queue(nullptr) {
        queue = std::make_unique<message_transport::IpcQueue>(std::move(params.queue_params));
    }

    ArchiveSubscription::~ArchiveSubscription() {
        if (queue) close();
    }
 
    bool ArchiveSubscription::is_ready() const {
        return true;
    }

    void ArchiveSubscription::close() {
        // calls the queue destructor
        if (!queue) return;
        spdlog::info("Attempting to delete subscription for {}", queue->name());
        queue->close();
        queue.reset();
    }
}