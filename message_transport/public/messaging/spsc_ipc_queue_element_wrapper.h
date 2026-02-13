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
        SpscIpcQueueRaiiWrapper(void* buffer, size_t available_size, SpscIpcQueue& queue)
            : buffer(buffer)
            , available_size(available_size)
            , queue(queue) {}

        ~SpscIpcQueueRaiiWrapper() {
            auto* message_header = static_cast<MessageHeader*>(buffer);
            if (client_data_written) {
                message_header->message_size = available_size; // set the message size in the header to reflect the actual size of the message payload
                message_header->flags |= MESSAGE_AVAILABLE;

                queue.commit_buffer(buffer, available_size + sizeof(MessageHeader)); // commit the buffer to the queue, including the size of the message header
            } else {
                // if we never wrote data to the buffer, we can just set the message header flags to indicate that the message is available for the consumer to read, and the consumer will skip over this message since it will have a size of 0.
                message_header->message_size = 0;
                message_header->flags &= ~MESSAGE_AVAILABLE;

                queue.commit_buffer(buffer, sizeof(MessageHeader)); // commit the buffer to the queue, only including the size of the message header since there is no message payload
            }
        }

        bool write_to_buffer(const char* data, size_t data_size) {

            // set message header 
            auto* message_header = static_cast<MessageHeader*>(buffer);
            auto* message_payload = static_cast<void*>(static_cast<char*>(buffer) + sizeof(MessageHeader));

            if (data_size > available_size - sizeof(MessageHeader)) {
                return false; // Not enough space in the buffer
            }
            // first we write the message payload, then we set the message header to indicate that the message
            // is available for the consumer to read. This ensures that the consumer will never see a partially
            // written message, as it will only read messages that have their header set to MESSAGE_AVAILABLE.
            std::memcpy(message_payload, data, data_size);
            client_data_written = true;
            available_size = data_size; // update available size to reflect the actual size of the message payload
            return true;
        }

        void* buffer;
        size_t available_size;
        SpscIpcQueue& queue;

        bool client_data_written{false};
    };
}