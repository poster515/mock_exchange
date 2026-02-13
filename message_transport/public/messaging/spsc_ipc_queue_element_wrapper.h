#pragma once

namespace message_transport {

    // forward declaration of SpscIpcQueue
    class SpscIpcQueue;

    /**
     * This class implements a single-producer, single-consumer (SPSC) inter-process communication (IPC) queue.
     * 
     * It provides a thread-safe mechanism for one producer to send messages to one consumer across process boundaries.
     */
    struct SpscIpcQueueRaiiWrapper
    {
        SpscIpcQueueRaiiWrapper(char* buffer, size_t size, SpscIpcQueue& queue)
            : buffer(buffer), size(size), queue(queue) {}

        ~SpscIpcQueueRaiiWrapper() {
            if (buffer) {
                queue.commit_buffer(size);
            }
        }

        char* buffer;
        size_t size;
        SpscIpcQueue& queue;
    };
}