#pragma once

#include <string_view>
#include <optional>

#include "messaging/spsc_ipc_queue_headers.h"

namespace message_transport {

    // forward decl
    struct SpscIpcQueueRaiiWrapper;

    // some checkers for safety
    // static_assert(std::atomic<uint64_t>::is_always_lock_free);

    /**
     * This class implements a single-producer, single-consumer (SPSC) inter-process communication (IPC) queue.
     * 
     * It provides a thread-safe mechanism for one producer to send messages to one consumer across process boundaries.
     * 
     * This class will support arbitrary message sizes and will handle synchronization internally to ensure safe communication between the producer and consumer.
     * 
     * The implementation will use shared memory and synchronization primitives to achieve efficient communication without busy-waiting.
     */
    class SpscIpcQueue {

        static const size_t MAX_QUEUE_SIZE_BYTES = 1024 * 1024 * 1024; // 1 GB
    public:
        SpscIpcQueue(std::string_view shm_file_name, size_t queue_size_bytes, bool is_writer);
        ~SpscIpcQueue();

        // Method to claim a buffer for writing a message to the queue. Upon destruction of the 
        // returned wrapper, the buffer will be committed to the queue.
        SpscIpcQueueRaiiWrapper claim_buffer(size_t size);

        // public API that exposes a single, non-blocking call for the consumer to poll for new messages in the queue.
        // This method will return immediately if there are no new messages available, and will return a wrapper around 
        // the message buffer if a new message is available for the consumer to read.
        std::optional<SpscIpcQueueRaiiWrapper> poll_buffer();

    private:

        // whether this instance is the writer or reader, used for managing the state of the
        // shared memory and ensuring proper synchronization between producer and consumer.
        // not super happy with this, but it'll help do some stupid sanity checks on startup.
        bool is_writer;

        // the total size of the queue in bytes, which will be used to manage the shared 
        // memory and ensure that messages do not exceed the queue capacity.
        const size_t queue_size_bytes;

        // grab and/or set the state of the shared memory region
        message_transport::GlobalHeader* global_header;

        // if the queue owner is the reader this can optionally be looped forever, reading messages
        // as they become available in the queue, and then processing them using some user-provided callback function.
        void read_buffer();

        // TODO: implement a callback_model concept and establish ownership semantics for the queue that allow us to
        // have multiple producers and/or consumers, and to allow producers and consumers to dynamically join and leave
        // the queue without disrupting the overall communication between other producers and consumers that are still active in the queue.
};
}