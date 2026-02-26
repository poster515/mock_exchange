#pragma once

#include <ranges>
#include <assert.h>

namespace message_transport {

    /**
     * This class implements a single-producer, single-consumer (SPSC) inter-process communication (IPC) queue buffer wrapper.
     * 
     * It provides a thread-safe mechanism for one producer to send messages to one consumer across process boundaries.
     */
    class SpscIpcQueueRaiiWrapper
    {
    public:
        SpscIpcQueueRaiiWrapper(uint8_t* buffer, size_t buffer_size)
            : wrapper(buffer, buffer_size) {}

        ~SpscIpcQueueRaiiWrapper() {
        }

        bool write_to_buffer(const char* data, size_t size) {

            assert(size <= wrapper.size_bytes() - sizeof(MessageHeader));

            // first we write the message payload, then we set the message header to indicate that the message
            // is available for the consumer to read. This ensures that the consumer will never see a partially
            // written message, as it will only read messages that have their header set to MESSAGE_AVAILABLE.
            auto* message_payload = static_cast<void*>(wrapper.data() + sizeof(MessageHeader));
            std::memcpy(message_payload, data, size);

            // set message header 
            auto* message_header = reinterpret_cast<MessageHeader*>(wrapper.data());
            message_header->flags.store(MESSAGE_COMMITTED, std::memory_order_release);
            return true;
        }

        template <typename T>
        [[nodiscard]] const T& get_as() const {
            assert(sizeof(T) <= wrapper.size_bytes()); // ensure that the size of the requested type is less than or equal to the size of the message payload
            auto* message_header = reinterpret_cast<MessageHeader*>(wrapper.data());
            auto* message_payload = static_cast<void*>(wrapper.data() + sizeof(MessageHeader));
            return *reinterpret_cast<const T*>(message_payload);
        }

        // Use this for non-owning views e.g., string_view, span, etc.
        template <typename T>
            requires std::is_trivially_copyable_v<T>
        [[nodiscard]] T get_as_view() const {
            auto* message_payload = static_cast<void*>(wrapper.data() + sizeof(MessageHeader));
            return T(reinterpret_cast<const char*>(message_payload), wrapper.size_bytes() - sizeof(MessageHeader));
        }

        // ideally only test function, but you could get cheeky with this.
        [[nodiscard]] const void* get_buffer() const {
            auto* message_header = reinterpret_cast<MessageHeader*>(wrapper.data());
            if (message_header->flags.load(std::memory_order_acquire) & MESSAGE_COMMITTED) {
                return static_cast<void*>(wrapper.data() + sizeof(MessageHeader));
            }
            return nullptr; // message is not available for reading
        }

    private:
        std::span<uint8_t> wrapper;
    };
}