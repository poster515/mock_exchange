#pragma once

#include <string_view>

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

    private:

        friend struct SpscIpcQueueRaiiWrapper;

        void commit_buffer(void* buffer, size_t total_size);

        // whether this instance is a writer or reader, used for managing the state of the
        // shared memory and ensuring proper synchronization between producer and consumer.
        // not super happy with this, but it'll help do some stupid sanity checks on startup.
        bool is_writer;

        // the total size of the queue in bytes, which will be used to manage the shared 
        // memory and ensure that messages do not exceed the queue capacity.
        const size_t queue_size_bytes;

        // grab and/or set the state of the shared memory region
        message_transport::GlobalHeader* global_header;
    };
}