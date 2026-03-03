#pragma once
#include <atomic>

namespace message_transport {

#pragma pack(push, 1)

    // global header structure that will be placed at the beginning of the shared memory region
    // to manage the state of the queue.
    // TODO: do we event need the message count?
    // TODO: can we better cache align this? Or does it not really matter since we're accessing individual fields atomically.
    struct GlobalHeader {
        std::atomic<uint64_t> write_offset; // offset from the beginning of the raw mapped memory region to the next available buffer region for writing
        std::atomic<uint64_t> read_offset;  // offset from the beginning of the raw mapped memory region to the next available buffer region for reading
        std::atomic<uint64_t> queue_size_bytes; // total size of the queue in bytes, used for managing the shared memory and ensuring messages do not exceed the queue capacity
        std::atomic<uint64_t> message_count; // total number of messages currently in the queue, used for monitoring and debugging purposes
        std::atomic_bool has_writer;
        std::atomic_bool has_reader;
    };

    const uint8_t MESSAGE_UNKNOWN = 0x00; // initial state of a message slot, should never be observed in normal operation since the producer should set the state to MESSAGE_LEASED as soon as it claims the slot for writing
    const uint8_t MESSAGE_AVAILABLE_FOR_WRITE = 0x01; // buffer space has neither been claimed nor committed, can be claimed by producer
    const uint8_t MESSAGE_AVAILABLE_FOR_READ = 0x02; // indicates that the message has been fully written and is ready for the consumer to read
    const uint8_t MESSAGE_SKIPPED = 0x04; // used as bookend when there is no room at end of queue and we must start at beginning
    const uint8_t MESSAGE_LEASED_FOR_WRITE = 0x08; // indicates that the producer has checked out a buffer space for writing, but has not actually committed any data yet
    const uint8_t MESSAGE_LEASED_FOR_READ = 0x10; // indicates that the consumer has checked out a buffer space for reading, but has not yet released it back to available
    // inserted before each message in the queue to manage the state of that message and provide metadata about the message.
    struct MessageHeader {
        std::atomic<uint8_t> flags; // Flags for message metadata
        std::array<uint8_t, 3> padding; // padding to ensure the message header is 8 bytes, which allows for proper alignment of the message payload that follows the header
        std::atomic<uint32_t> message_size; // size of the message payload in bytes, used for managing the queue and ensuring messages do not exceed the queue capacity
    };

#pragma pack(pop)
}