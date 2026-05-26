#pragma once

#include <string>

#include "messaging/SpinPolicy.h"
#include "messaging/ipc_queue_element_wrapper.h"

namespace message_transport {

    /**
     * This class implements a single-producer, multi-consumer (MPSC) inter-process communication (IPC) queue.
     * 
     * This class supports arbitrary message sizes and handles synchronization internally to ensure safe communication between the producer(s) and consumer.
     * 
     * The implementation uses shared memory and synchronization primitives to achieve efficient communication without busy-waiting.
     * If a callback is provided, a new thread will be spawned which constantly polls the buffer. See consumer.cpp for an example.
     *
     * TODO: Can we just consolidate with MpscQueue? Write semantics are almost identical.
     */
    class SpmcIpcQueue {
    public:

        struct SpmcQueueParameters {
            const std::string file_name;
            size_t queue_size;
            bool is_writer {true};
        };
        
        SpmcIpcQueue(SpmcQueueParameters&& params);
        ~SpmcIpcQueue();

        // Method to claim a buffer for writing a message to the queue. Upon destruction of the 
        // returned wrapper, the buffer will be committed to the queue. Only available for the 
        // single dedicated writer of this queue.
        template <CSpinPolicy WritePolicy>
        IpcQueueRaiiWriterWrapper claim_buffer(size_t size);

        // public API that exposes a single, non-blocking call for the consumer to poll for new messages in the queue.
        // This method will return immediately if there are no new messages available, and will return a wrapper around 
        // the message buffer if a new message is available for the consumer to read.
        std::optional<IpcQueueRaiiReaderWrapper> poll_buffer();

        template <CSpinPolicy ReadPolicy>
        IpcQueueRaiiReaderWrapper read_buffer();

        void release_buffer(MessageHeader& header);

    };
}