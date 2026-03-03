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

        ~SpscIpcQueueRaiiWrapper() = default;

    protected:
        std::span<uint8_t> wrapper;
    };

    /**
     * RAII wrapper for the consumer side of the SPSC IPC queue. This wrapper provides methods for the consumer
     * to read messages from the queue and manage the state of the message buffer.
     * 
     * The existance of this wrapper guarantees memory safe access to the memory since this is a zero-copy
     * implementation and the producer and consumer are communicating through shared memory. The wrapper will manage
     * the state of the message buffer to ensure that the consumer only reads messages that have been fully written
     * by the producer and that the producer does not overwrite messages that have not yet been read by the consumer.
     * 
     */
    class SpscIpcQueueRaiiReaderWrapper : public SpscIpcQueueRaiiWrapper
    {
    public:
        SpscIpcQueueRaiiReaderWrapper(uint8_t* buffer, size_t buffer_size)
            : SpscIpcQueueRaiiWrapper(buffer, buffer_size) {}

        SpscIpcQueueRaiiReaderWrapper(const SpscIpcQueueRaiiReaderWrapper&) = delete;
        SpscIpcQueueRaiiReaderWrapper& operator=(const SpscIpcQueueRaiiReaderWrapper&) = delete;
        SpscIpcQueueRaiiReaderWrapper(SpscIpcQueueRaiiReaderWrapper&& other)
            : SpscIpcQueueRaiiWrapper(other.wrapper.data(), other.wrapper.size()) {
            // need to relinquish the other wrapper of its resources/ownership
            other.wrapper = std::span<uint8_t>();
            other.released = true;
        }
        SpscIpcQueueRaiiReaderWrapper& operator=(SpscIpcQueueRaiiReaderWrapper&& other) {
            if (this != &other) {
                wrapper = other.wrapper;
                other.wrapper = std::span<uint8_t>();
                other.released = true; // prevent the other wrapper from releasing the message buffer when it is destroyed
            }
            return *this;
        };

        ~SpscIpcQueueRaiiReaderWrapper() {
            if (!released) {
                release();
            }
        }

        void release() {
            auto* message_header = reinterpret_cast<MessageHeader*>(wrapper.data());
            message_header->flags.store(MESSAGE_AVAILABLE_FOR_WRITE, std::memory_order_release);
            released = true;
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
            if (message_header->flags.load(std::memory_order_acquire) & MESSAGE_LEASED_FOR_READ) {
                return static_cast<void*>(wrapper.data() + sizeof(MessageHeader));
            }
            return nullptr; // message is not available for reading
        }

    private:
        bool released { false };
    };

    class SpscIpcQueueRaiiWriterWrapper : public SpscIpcQueueRaiiWrapper
    {
    public:
        SpscIpcQueueRaiiWriterWrapper(uint8_t* buffer, size_t buffer_size)
            : SpscIpcQueueRaiiWrapper(buffer, buffer_size) {}

        // by deleting eery other constructor and assignment operator we are guaranteeing that the 
        // wrapper properly manages the memory assigned to it
        SpscIpcQueueRaiiWriterWrapper(const SpscIpcQueueRaiiWriterWrapper&) = delete;
        SpscIpcQueueRaiiWriterWrapper& operator=(const SpscIpcQueueRaiiWriterWrapper&) = delete;
        SpscIpcQueueRaiiWriterWrapper(SpscIpcQueueRaiiWriterWrapper&&) = delete;
        SpscIpcQueueRaiiWriterWrapper& operator=(SpscIpcQueueRaiiWriterWrapper&&) = delete;

        ~SpscIpcQueueRaiiWriterWrapper() {
            // upon destruction of the writer wrapper, we can set the message header flags to indicate that the message is now available for the consumer to read.
            auto* message_header = reinterpret_cast<MessageHeader*>(wrapper.data());
            message_header->flags.store(MESSAGE_AVAILABLE_FOR_READ, std::memory_order_release);
        }

        bool write_to_buffer(const char* data, size_t size) {

            assert(size <= wrapper.size_bytes() - sizeof(MessageHeader));

            // first we write the message payload, then we set the message header to indicate that the message
            // is available for the consumer to read. This ensures that the consumer will never see a partially
            // written message, as it will only read messages that have their header set to MESSAGE_AVAILABLE.
            auto* message_payload = static_cast<void*>(wrapper.data() + sizeof(MessageHeader));
            std::memcpy(message_payload, data, size);
            return true;
        }
    };
}