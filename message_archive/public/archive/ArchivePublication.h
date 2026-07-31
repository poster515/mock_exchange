#pragma once

#include <span>
#include <queue>

#include "messaging/ipc_queue.h"
#include "messaging/ipc_queue_element_wrapper.h"

namespace archive {
    /**
     * For use in things like a control channel, where multiple
     * processes may be requesting behavior from a singular process.
     */
    class ArchivePublication {

    public:
        struct ArchivePublicationParams {
            message_transport::IpcQueue::IpcQueueParameters queue_params;
        };
        ArchivePublication(ArchivePublicationParams&& params);

        // users should poll this to determine queue health
        bool is_ready() const;

        std::span<std::byte> claim_buffer(size_t buffer_size);
        size_t commit();
        size_t uncommitted_message_count() const;

        void close();

        std::string_view agent_name() const { return queue->name(); }

    private:
        std::unique_ptr<message_transport::IpcQueue> queue;
        std::deque<message_transport::IpcQueueRaiiWriterWrapper> to_commit;
    };
}