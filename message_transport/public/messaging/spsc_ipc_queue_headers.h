#pragma once
#include <atomic>

namespace message_transport {

#pragma pack(push, 1)

    // global header structure that will be placed at the beginning of the shared memory region
    // to manage the state of the queue.
    struct GlobalHeader {
        alignas(64) std::atomic<uint64_t> write_offset;
        alignas(64) std::atomic<uint64_t> read_offset;
        alignas(64) std::atomic<uint64_t> queue_size_bytes; // total size of the queue in bytes, used for managing the shared memory and ensuring messages do not exceed the queue capacity
        alignas(64) std::atomic<uint64_t> message_count; // total number of messages currently in the queue, used for monitoring and debugging purposes
        alignas(64) std::atomic_bool has_writer;
        alignas(64) std::atomic_bool has_reader;
    };

    const uint8_t MESSAGE_AVAILABLE = 0x00; // buffer space has neither been claimed nor committed, can be claimed by producer
    const uint8_t MESSAGE_SKIPPED = 0x01; // used as bookend when there is no room at end of queue and we must start at beginning
    const uint8_t MESSAGE_COMMITTED = 0x02; // indicates that the message has been fully written and is ready for the consumer to read
    const uint8_t MESSAGE_ACQUIRED = 0x04; // indicates that the consumer has acquired the message for reading, used for synchronization between producer and consumer
    const uint8_t MESSAGE_LEASED = 0x08; // indicates that the producer has checked out a buffer space for writing, but has not actually committed any data yet

    // inserted before each message in the queue to manage the state of that message and provide metadata about the message.
    alignas(64) struct MessageHeader {
        std::atomic<uint32_t> message_size; // Size of the message payload
        std::atomic<uint8_t> flags; // Flags for message metadata
    };

#pragma pack(pop)
}