#pragma once

#include <string_view>
#include <optional>
#include <functional>
#include <thread>
#include <memory>
#include <iostream>

#include <spdlog/spdlog.h>

#include "messaging/ipc_queue_headers.h"
#include "messaging/SpinPolicy.h"

using namespace std::chrono_literals;

namespace message_transport {

    // forward decl
    class IpcQueueRaiiWriterWrapper;
    class IpcQueueRaiiReaderWrapper;

    // some checkers for safety
    static_assert(std::atomic<uint64_t>::is_always_lock_free);

    /**
     * This class implements a multi-producer, multi-consumer (MPMC) inter-process communication (IPC) queue.
     * 
     * It provides a thread-safe mechanism for multiple producers to send messages to one consumer across process boundaries.
     * 
     * This class supports arbitrary message sizes and handles synchronization internally to ensure safe communication between the producer(s) and consumer(s).
     * 
     * The implementation uses shared memory and synchronization primitives to achieve efficient communication without busy-waiting.
     * If a callback is provided, a new thread will be spawned which constantly polls the buffer. See consumer.cpp for an example.
     * 
     * Needs at least one writer and one reader otherwise the shared memory file is unlinked at the OS level. We could stall writers until a
     * new reader joins but that requires maintaining more atomic state that adds overhead and increases race condition surface.
     * 
     * TODO: This implementation could benefit from a "hot swap" clean buffer/dirty buffer paradigm. Probably not a huge deal
     * to leave as is for now but its something worth investigating at some point.
     */
    class IpcQueue {
        struct UnpackedReadersAndWriterOffset {

            size_t num_readers { 0 };
            uint64_t write_offset { 0 };
            uint64_t original_value { 0 };

            UnpackedReadersAndWriterOffset(uint64_t value) { unwrap(value); }

            UnpackedReadersAndWriterOffset& unwrap(uint64_t value) {
                original_value = value;
                num_readers =   value       & 0x00000000000000FF;  // bits 0-7
                write_offset = (value >> 8) & 0x00FFFFFFFFFFFFFF;  // bits 8-63
                return *this;
            }
            // DO NOT MODIFY ORIGINAL_VALUE (Seems obvious but still)
            uint64_t return_add_reader() { return (++num_readers & 0x00000000000000FF) | ((write_offset << 8) & 0xFFFFFFFFFFFFFF00); }
            uint64_t return_sub_reader() { return (--num_readers & 0x00000000000000FF) | ((write_offset << 8) & 0xFFFFFFFFFFFFFF00); }
            uint64_t return_add_offset(uint64_t message_size) { return (num_readers & 0x00000000000000FF) | (((write_offset += message_size) << 8) & 0xFFFFFFFFFFFFFF00); }
        };
    public:
        static constexpr uint32_t MAGIC = 0xDEADBEEF;
        static const size_t MAX_QUEUE_SIZE_BYTES = 1024 * 1024 * 1024; // 1 GB
        static constexpr auto DEFAULT_WRITER_TIMEOUT = 1us;

        static bool is_power_of_two(size_t n) { return n != 0 && (n & (n - 1)) == 0; }

        struct IpcQueueParameters {
            const std::string file_name;
            size_t queue_size;
            bool is_writer {true};
        };
        
        IpcQueue(IpcQueueParameters&& params);
        ~IpcQueue();

        // Method to claim a buffer for writing a message to the queue. Upon destruction of the 
        // returned wrapper, the buffer will be committed to the queue.
        template <CSpinPolicy WritePolicy>
        IpcQueueRaiiWriterWrapper claim_buffer(size_t size);

        // public API that exposes a single, non-blocking call for the consumer to poll for new messages in the queue.
        // This method will return immediately if there are no new messages available, and will return a wrapper around 
        // the message buffer if a new message is available for the consumer to read.
        std::optional<IpcQueueRaiiReaderWrapper> poll_buffer();

        template <CSpinPolicy ReadPolicy>
        IpcQueueRaiiReaderWrapper read_buffer();

        void release_buffer(MessageHeader& header);
        void commit_buffer(MessageHeader& header);

        size_t num_readers(std::memory_order order = std::memory_order_acquire) const;
        void close();

    private:

        // whether this instance is the writer or reader, used for managing the state of the
        // shared memory and ensuring proper synchronization between producer and consumer.
        // not super happy with this, but it'll help do some stupid sanity checks on startup.
        bool is_writer;
        const std::string file_name;

        // the total size of the queue in bytes, which will be used to manage the shared 
        // memory and ensure that messages do not exceed the queue capacity.
        size_t queue_size_bytes;
        size_t available_queue_size_bytes;
        uint64_t abs_read_offset;

        // grab and/or set the state of the shared memory region
        message_transport::GlobalHeader* global_header;

        int fd;

        void init_new_file();
        void validate_settings();

        // if the queue owner is the reader this can optionally be looped forever, reading messages
        // as they become available in the queue, and then processing them using some user-provided callback function.
        // returns whether the queue should continue to poll or not.
        bool read_buffer();

        void insert_skip_message(const uint64_t skip_offset);

        void decrement_readers_until(uint64_t abs_write_offset);
        UnpackedReadersAndWriterOffset modify_readers(bool new_reader);

        template <CSpinPolicy WritePolicy>
        inline UnpackedReadersAndWriterOffset wait_for_next_write_offset(const size_t total_size_with_header) {

            /**
             * This is a critical piece of code - basically writers must come here when the attempt to claim
             * buffer space and the receive a valid location to write into.
             * 
             * We MUST ensure we not writing in memory that the reader is or _is going to_ be reading from.
             * 
             * The biggest challenge here is really just making sure we're not lapping the reader.
             */
            uint64_t desired { 0 };
            uint64_t current { 0 };
            UnpackedReadersAndWriterOffset wrapper { 0 };

            do {
                current = global_header->write_fields.readers_and_write_offset.load(std::memory_order_relaxed);
                wrapper.unwrap(current);
                const size_t bytes_remaining_at_end = available_queue_size_bytes - (wrapper.write_offset % available_queue_size_bytes);
                const size_t bytes_to_bump = (total_size_with_header + sizeof(MessageHeader)) <= bytes_remaining_at_end ? total_size_with_header : bytes_remaining_at_end;
                desired = wrapper.return_add_offset(bytes_to_bump);
            } while(!global_header->write_fields.readers_and_write_offset.compare_exchange_weak(current, desired, std::memory_order_release, std::memory_order_relaxed));

            // now we have a write location claimed. May have to spin if the slowest reader hasn't caught up yet.
            wrapper.unwrap(current); // rewrap the claimed position/reader count otherwise we'd be returning new info.
            auto slowest_reader = global_header->read_fields.read_offset.load(std::memory_order_relaxed);
            bool must_wait = (wrapper.write_offset - slowest_reader) >= available_queue_size_bytes;

            while (must_wait) {
                WritePolicy::execute();
                slowest_reader = global_header->read_fields.read_offset.load(std::memory_order_relaxed);
                must_wait = (wrapper.write_offset - slowest_reader) >= available_queue_size_bytes;

                // TODO: check for slow reader here. If # readers hasn't changed in N seconds and queue is full drop their message.
            }

            return wrapper;
        }
    };
}