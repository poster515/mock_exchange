#pragma once

#include <ranges>
#include <assert.h>

namespace message_transport {

    // forward declaration of SpscIpcQueue
    class SpscIpcQueue;

    /**
     * This class implements a single-producer, single-consumer (SPSC) inter-process communication (IPC) queue buffer wrapper.
     * 
     * It provides a thread-safe mechanism for one producer to send messages to one consumer across process boundaries.
     */
    class SpscIpcQueueRaiiWrapper
    {
    public:
        SpscIpcQueueRaiiWrapper(uint8_t* buffer, size_t buffer_size, SpscIpcQueue& queue)
            : wrapper(buffer, buffer_size)
            , queue(queue) {}

        ~SpscIpcQueueRaiiWrapper() {
            // TODO: might want to rethink this a little - seems like this destructor + bool flag are overkill
            // when we can just set these fields after the client write to the buffer.
            auto* message_header = reinterpret_cast<MessageHeader*>(wrapper.data());
            if (client_data_written) {
                message_header->message_size.store(wrapper.size_bytes(), std::memory_order_relaxed);
                message_header->flags.store(MESSAGE_COMMITTED, std::memory_order_release);
            } else {
                // if we never wrote data to the buffer, we can just set the message header flags to indicate that the message is available for the consumer to read, and the consumer will skip over this message since it will have a size of 0.
                message_header->message_size.store(0, std::memory_order_relaxed);
                message_header->flags.store(MESSAGE_AVAILABLE, std::memory_order_release);
            }
        }

        bool write_to_buffer(const char* data) {

            // set message header 
            auto* message_header = reinterpret_cast<MessageHeader*>(wrapper.data());
            auto* message_payload = static_cast<void*>(wrapper.data() + sizeof(MessageHeader));

            // first we write the message payload, then we set the message header to indicate that the message
            // is available for the consumer to read. This ensures that the consumer will never see a partially
            // written message, as it will only read messages that have their header set to MESSAGE_AVAILABLE.
            std::memcpy(message_payload, data, wrapper.size_bytes());
            client_data_written = true;
            return true;
        }

        template <typename T>
        [[nodiscard]] const T& get_as() const {
            assert(sizeof(T) <= wrapper.size_bytes()); // ensure that the size of the requested type is less than or equal to the size of the message payload
            auto* message_header = reinterpret_cast<MessageHeader*>(wrapper.data());
            auto* message_payload = static_cast<void*>(wrapper.data() + sizeof(MessageHeader));
            return *reinterpret_cast<const T*>(message_payload);
        }

        // ideally only test function, but you could get cheeky with this.
        [[nodiscard]] const void* get_buffer() const {
            auto* message_header = reinterpret_cast<MessageHeader*>(wrapper.data());
            if (message_header->flags.load(std::memory_order_acquire) & MESSAGE_AVAILABLE) {
                return static_cast<void*>(wrapper.data() + sizeof(MessageHeader));
            }
            return nullptr; // message is not available for reading
        }

    private:
        std::span<uint8_t> wrapper;
        SpscIpcQueue& queue;

        bool client_data_written{false};
    };
}