#pragma once

#include <span>

#include "messaging/mpsc_ipc_queue.h"
#include "messaging/mpsc_ipc_queue_element_wrapper.h"

namespace archive {
    /**
     * For use in things like a control channel, where multiple
     * processes may be requesting behavior from a singular process.
     */
    class ArchivePublication {

    public:
        struct ArchivePublicationParams {
            message_transport::MpscIpcQueue::MpscQueueParameters queue_params;
        };
        ArchivePublication(ArchivePublicationParams&& params);

        // users should poll this to determine queue health
        bool is_ready() const;

        std::span<std::byte> claim_buffer(size_t buffer_size);
        size_t commit();
        size_t uncommitted_message_count() const;

        void close();

    private:
        message_transport::MpscIpcQueue queue;
        std::deque<message_transport::MpscIpcQueueRaiiWriterWrapper> to_commit;
    };
}