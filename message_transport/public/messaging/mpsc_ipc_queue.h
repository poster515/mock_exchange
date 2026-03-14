#pragma once

#include <string_view>
#include <optional>
#include <functional>
#include <thread>
#include <memory>
#include <iostream>

#include <spdlog/spdlog.h>

#include "mpsc_ipc_queue_headers.h"

using namespace std::chrono_literals;

namespace message_transport {

    // forward decl
    class MpscIpcQueueRaiiWriterWrapper;
    class MpscIpcQueueRaiiReaderWrapper;

    // some checkers for safety
    static_assert(std::atomic<uint64_t>::is_always_lock_free);

    /**
     * This class implements a multi-producer, single-consumer (MPSC) inter-process communication (IPC) queue.
     * 
     * It provides a thread-safe mechanism for multiple producers to send messages to one consumer across process boundaries.
     * 
     * This class supports arbitrary message sizes and handles synchronization internally to ensure safe communication between the producer(s) and consumer.
     * 
     * The implementation uses shared memory and synchronization primitives to achieve efficient communication without busy-waiting.
     * If a callback is provided, a new thread will be spawned which constantly polls the buffer. See consumer.cpp for an example.
     * 
     * TODO: This implementation could benefit from a "hot swap" clean buffer/dirty buffer paradigm. Probably not a huge deal
     * to leave as is for now but its something worth investigating at some point.
     */
    class MpscIpcQueue {
    public:
        static const size_t MAX_QUEUE_SIZE_BYTES = 1024 * 1024 * 1024; // 1 GB
        static constexpr auto DEFAULT_WRITER_TIMEOUT = 1us;

        // Could make this a class template/concept but for now we'll leave it as a suboptimal function.
        using CallbackModel = std::function<bool(MpscIpcQueueRaiiReaderWrapper)>;

        // TODO: if we can assert queue_size_bytes as power of 2 we can use masking instead of mod-ing for offset calcs
        struct MpscQueueParameters {
            std::string_view file_name;
            size_t queue_size;
            bool is_writer {true};
            std::optional<CallbackModel> callback {std::nullopt};
        };
        
        MpscIpcQueue(MpscQueueParameters&& params);
        ~MpscIpcQueue();

        // Method to claim a buffer for writing a message to the queue. Upon destruction of the 
        // returned wrapper, the buffer will be committed to the queue.
        MpscIpcQueueRaiiWriterWrapper blocking_claim_buffer(size_t size);

        std::optional<MpscIpcQueueRaiiWriterWrapper> nonblocking_claim_buffer(size_t size);

        // public API that exposes a single, non-blocking call for the consumer to poll for new messages in the queue.
        // This method will return immediately if there are no new messages available, and will return a wrapper around 
        // the message buffer if a new message is available for the consumer to read.
        std::optional<MpscIpcQueueRaiiReaderWrapper> poll_buffer();

        void release_buffer(MessageHeader& header);

    private:

        // whether this instance is the writer or reader, used for managing the state of the
        // shared memory and ensuring proper synchronization between producer and consumer.
        // not super happy with this, but it'll help do some stupid sanity checks on startup.
        bool is_writer;
        const std::string file_name;

        // the total size of the queue in bytes, which will be used to manage the shared 
        // memory and ensure that messages do not exceed the queue capacity.
        const size_t queue_size_bytes;
        const size_t available_queue_size_bytes;

        // grab and/or set the state of the shared memory region
        message_transport::GlobalHeader* global_header;

        std::optional<CallbackModel> dispatcher;

        int fd;

        std::thread read_thread;

        // if the queue owner is the reader this can optionally be looped forever, reading messages
        // as they become available in the queue, and then processing them using some user-provided callback function.
        // returns whether the queue should continue to poll or not.
        bool read_buffer();

        void insert_skip_message(const uint64_t skip_offset);

        inline uint64_t wait_for_next_write_offset(const size_t total_size_with_header, std::chrono::nanoseconds timeout = DEFAULT_WRITER_TIMEOUT) {

            /**
             * This is a critical piece of code - basically writers must come here when the attempt to claim
             * buffer space and the receive a valid location to write into.
             * 
             * We MUST ensure we not writing in memory that the reader is or _is going to_ be reading from.
             * 
             * The biggest challenge here is really just making sure we're not lapping the reader.
             */
            
            auto write_offset = global_header->write_offset.load(std::memory_order_relaxed);

            size_t bytes_remaining_at_end {0};
            uint64_t rel_write_offset {0};
            uint64_t next_write_offset {0};

            do {
                rel_write_offset = write_offset % available_queue_size_bytes;
                bytes_remaining_at_end = available_queue_size_bytes - rel_write_offset;
                
                if ((total_size_with_header + sizeof(MessageHeader)) <= bytes_remaining_at_end) {
                    // if we can fit our message plus another MessageHeader, cool! Try to claim.
                    next_write_offset = write_offset + total_size_with_header;
                } else {
                    // otherwise, try to bump the next_write_offset to the beginning of the queue.
                    // We'll handle the skip message insertion later.
                    next_write_offset = write_offset + bytes_remaining_at_end;
                }
            } while(!global_header->write_offset.compare_exchange_weak(write_offset, next_write_offset, std::memory_order_release, std::memory_order_relaxed));

            // now we have a write location claimed. May have to spin if the reader hasn't caught up yet.
            auto read_begin = global_header->read_offset.load(std::memory_order_relaxed);
            bool must_wait = (next_write_offset - read_begin) > available_queue_size_bytes;
            // spdlog::info("Claimed abs offset write_offset {} with total sz {}, abs read_offset at {}, must_wait: {}", write_offset, next_write_offset - write_offset, read_begin, must_wait);
            while (must_wait) {
                std::this_thread::sleep_for(timeout);
                read_begin = global_header->read_offset.load(std::memory_order_relaxed);
                must_wait = (next_write_offset - read_begin) > available_queue_size_bytes;
            }

            return write_offset;
        }
    };
}