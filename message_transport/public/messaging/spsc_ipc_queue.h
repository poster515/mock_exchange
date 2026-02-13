#pragma once

#include <string_view>

#include "messaging/spsc_ipc_queue_element_wrapper.h"

namespace message_transport {

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
    public:
        SpscIpcQueue(std::string_view shm_file_name);
        ~SpscIpcQueue();

        // Method to claim a buffer for writing a message to the queue. Upon destruction of the 
        // returned wrapper, the buffer will be committed to the queue.
        SpscIpcQueueRaiiWrapper claim_buffer(size_t size);

    private:
        // Internal method to commit a buffer to the queue after writing is done.
        void commit_buffer(size_t size);

        // Internal data members for managing shared memory, synchronization primitives, and queue state would be declared here.
        std::string_view shm_file_name;

    };
}