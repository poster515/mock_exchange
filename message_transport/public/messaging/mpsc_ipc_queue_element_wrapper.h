#pragma once

#include <ranges>
#include <assert.h>
#include <cstring>

#include "mpsc_ipc_queue.h"

namespace message_transport {

    /**
     * This class implements a single-producer, single-consumer (SPSC) inter-process communication (IPC) queue buffer wrapper.
     * 
     * It provides a thread-safe mechanism for one producer to send messages to one consumer across process boundaries.
     */
    class MpscIpcQueueRaiiWrapper
    {
    public:
        MpscIpcQueueRaiiWrapper(uint8_t* buffer, size_t buffer_size)
            : wrapper(buffer, buffer_size) {}

        ~MpscIpcQueueRaiiWrapper() = default;

    protected:
        std::span<uint8_t> wrapper;
    };

    /**
     * RAII wrapper for the consumer side of the MPSC IPC queue. This wrapper provides methods for the consumer
     * to read messages from the queue and manage the state of the message buffer.
     * 
     * The existance of this wrapper guarantees memory safe access to the memory since this is a zero-copy
     * implementation and the producer and consumer are communicating through shared memory. The wrapper will manage
     * the state of the message buffer to ensure that the consumer only reads messages that have been fully written
     * by the producer and that the producer does not overwrite messages that have not yet been read by the consumer.
     * 
     */
    class MpscIpcQueueRaiiReaderWrapper : public MpscIpcQueueRaiiWrapper
    {
    public:
        MpscIpcQueueRaiiReaderWrapper(uint8_t* buffer, size_t buffer_size, MpscIpcQueue& queue)
            : MpscIpcQueueRaiiWrapper(buffer, buffer_size)
            , queue(queue) {}

        MpscIpcQueueRaiiReaderWrapper(const MpscIpcQueueRaiiReaderWrapper&) = delete;
        MpscIpcQueueRaiiReaderWrapper& operator=(const MpscIpcQueueRaiiReaderWrapper&) = delete;
        MpscIpcQueueRaiiReaderWrapper(MpscIpcQueueRaiiReaderWrapper&& other)
            : MpscIpcQueueRaiiWrapper(other.wrapper.data(), other.wrapper.size())
            , queue(other.queue) {
            // need to relinquish the other wrapper of its resources/ownership
            other.wrapper = std::span<uint8_t>();
            other.released = true;
        }
        MpscIpcQueueRaiiReaderWrapper& operator=(MpscIpcQueueRaiiReaderWrapper&& other) = delete;

        ~MpscIpcQueueRaiiReaderWrapper() {
            if (!released) {
                release();
            }
        }

        void release() {
            queue.release_buffer(*reinterpret_cast<MessageHeader*>(wrapper.data()));
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
            if (message_header->commit_flag.load(std::memory_order_acquire) == CommitFlag::READY_FOR_CONSUMER) {
                return static_cast<void*>(wrapper.data() + sizeof(MessageHeader));
            }
            return nullptr; // message is not available for reading
        }

        const size_t get_payload_size() const {
            auto* message_header = reinterpret_cast<MessageHeader*>(wrapper.data());
            return message_header->message_size;
        }

    private:
        bool released { false };
        MpscIpcQueue& queue;
    };

    class MpscIpcQueueRaiiWriterWrapper : public MpscIpcQueueRaiiWrapper
    {
    public:
        MpscIpcQueueRaiiWriterWrapper(uint8_t* buffer, size_t buffer_size)
            : MpscIpcQueueRaiiWrapper(buffer, buffer_size) {}
        // by deleting eery other constructor and assignment operator we are guaranteeing that the 
        // wrapper properly manages the memory assigned to it
        MpscIpcQueueRaiiWriterWrapper(const MpscIpcQueueRaiiWriterWrapper&) = delete;
        MpscIpcQueueRaiiWriterWrapper& operator=(const MpscIpcQueueRaiiWriterWrapper&) = delete;
        MpscIpcQueueRaiiWriterWrapper(MpscIpcQueueRaiiWriterWrapper&&) = delete;
        MpscIpcQueueRaiiWriterWrapper& operator=(MpscIpcQueueRaiiWriterWrapper&&) = delete;

        ~MpscIpcQueueRaiiWriterWrapper() = default;

        bool write_to_buffer(const char* data, size_t size) {
            assert(static_cast<size_t>(size + sizeof(MessageHeader)) <= wrapper.size_bytes());
            auto* hdr = reinterpret_cast<MessageHeader*>(wrapper.data());
            std::memcpy(wrapper.data() + sizeof(MessageHeader), data, size);

            // publish to consumer atomically
            hdr->commit_flag.store(CommitFlag::READY_FOR_CONSUMER, std::memory_order_release);
            return true;
        }
    };
}