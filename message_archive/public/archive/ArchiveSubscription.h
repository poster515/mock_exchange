#pragma once

#include "messaging/spmc_ipc_queue.h"

namespace archive {

    /**
     * A subscription to an active message queue. This adds this subscription
     * to the set of readers for a given publication. This is an atomically
     * safe operation that works with the SP-MC queue design.
     */
    class ArchiveSubscription {
    
    public:
        struct ArchiveSubscriptionParams {
            const std::string file_name;
        };
        ArchiveSubscription(ArchiveSubscriptionParams&& params);

        // users should poll this to determine queue health
        bool is_ready() const;

        std::span<const std::byte> poll_buffer();

        void close();

    private:
        message_transport::SpmcIpcQueue queue;
        std::optional<message_transport::MpscIpcQueueRaiiReaderWrapper> reader;
    };
}