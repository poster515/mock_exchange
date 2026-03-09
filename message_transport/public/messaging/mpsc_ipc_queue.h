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

        struct MpscQueueParameters {
            std::string_view file_name;
            size_t queue_size;
            bool is_writer;
            std::optional<CallbackModel> callback;
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

        // the total size of the queue in bytes, which will be used to manage the shared 
        // memory and ensure that messages do not exceed the queue capacity.
        const size_t queue_size_bytes;

        // grab and/or set the state of the shared memory region
        message_transport::GlobalHeader* global_header;

        std::optional<CallbackModel> dispatcher;

        int fd;

        std::thread read_thread;

        // if the queue owner is the reader this can optionally be looped forever, reading messages
        // as they become available in the queue, and then processing them using some user-provided callback function.
        // returns whether the queue should continue to poll or not.
        bool read_buffer();

        // TODO: implement a callback_model concept and establish ownership semantics for the queue that allow us to
        // have multiple producers and/or consumers, and to allow producers and consumers to dynamically join and leave
        // the queue without disrupting the overall communication between other producers and consumers that are still active in the queue.
        
        // waits for the current slot to become available, either because its been read or because the region is skipped from a pervious iteration.
        // Returns the number of application bytes that were stored in the slot.
        inline void wait_for_slot_until(const size_t total_size_with_header, std::chrono::nanoseconds timeout = DEFAULT_WRITER_TIMEOUT) {

            uint64_t write_offset = global_header->write_offset.load(std::memory_order_relaxed);

            // basically just need the read_offset of the current reader to be outside the range of this write region
            uint64_t read_begin = global_header->read_offset.load(std::memory_order_relaxed);
            auto* message_header = reinterpret_cast<MessageHeader*>(reinterpret_cast<uint8_t*>(global_header) + read_begin);
            size_t read_end = read_begin + message_header->message_size;
            const size_t write_end = write_offset + total_size_with_header;

            spdlog::info("Waiting for slot at offset {} with size {} bytes to become available. Current read offset: {}", write_offset, total_size_with_header - sizeof(MessageHeader), read_begin);

            while ((write_offset <= read_begin && read_begin < write_end) ||
                   (write_offset < read_end && read_end <= write_end)) {

                const auto& header = *reinterpret_cast<MessageHeader*>(reinterpret_cast<uint8_t*>(global_header) + read_begin);
                if (header.commit_flag.load(std::memory_order_relaxed) == CommitFlag::NOT_READY) {
                    break;
                }

                std::this_thread::sleep_for(timeout);
                read_begin = global_header->read_offset.load(std::memory_order_relaxed);
                message_header = reinterpret_cast<MessageHeader*>(reinterpret_cast<uint8_t*>(global_header) + read_begin);
                read_end = read_begin + message_header->message_size;

                write_offset = global_header->write_offset.load(std::memory_order_relaxed);
            }
        }

        void insert_skip_message(const uint64_t skip_offset);
};
}