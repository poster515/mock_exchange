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

    enum class MessageType : uint32_t {
        NORMAL = 0x00,
        PADDING = 0x01 // used as bookend
    };

    enum class CommitFlag : uint8_t {
        NOT_READY = 0,
        READY_FOR_CONSUMER = 1
    };

    // inserted before each message in the queue to manage the state of that message and provide metadata about the message.
    struct MessageHeader {
        uint32_t message_size;            // payload size
        MessageType type;                 // NORMAL or PADDING
        std::atomic<CommitFlag> commit_flag; // 0 = not ready, 1 = ready for consumer
    };

#pragma pack(pop)
}