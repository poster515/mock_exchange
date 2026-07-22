#pragma once
#include <atomic>

namespace message_transport {

#pragma pack(push, 1)

    struct WriteFields {
        std::atomic<uint64_t> readers_and_write_offset; // unscaled offset from the end of the global header of the raw mapped memory region, to the next available buffer region for writing
        std::atomic<uint64_t> message_count; // total number of messages written to the queue
        std::atomic<uint64_t> queue_size_bytes; // total size of the queue in bytes, used for managing the shared memory and ensuring messages do not exceed the queue capacity
        std::atomic<uint32_t> num_writers;
    };

    struct ReadFields {
        std::atomic<uint64_t> read_offset;  // unscaled offset from the end of the global header of the raw mapped memory region, to the oldest unread message.
    };

    template <typename T, size_t N> requires (N >= sizeof(T))
    using Padding = std::array<uint8_t, N - sizeof(T)>;

    using WritePadding = Padding<WriteFields, 64>;
    using ReadPadding = Padding<ReadFields, 64>;

    using MagicNumber = std::atomic<uint32_t>;
    using MagicPadding = Padding<MagicNumber, 64>;

    // global header structure that will be placed at the beginning of the shared memory region
    struct GlobalHeader {
        MagicNumber magic;
        MagicPadding padding;

        WriteFields write_fields;
        WritePadding write_padding;

        ReadFields read_fields;
        ReadPadding read_padding;
    };

    static_assert(sizeof(GlobalHeader) == 192, "sizeof(GlobalHeader) is not 192");

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
        uint64_t sequence_number;           // monotonic sequence number of message (allow to naturally wrap (lol)), starts at 0
        uint32_t message_size;              // payload size
        MessageType type;                   // NORMAL or PADDING
        std::atomic<uint8_t> num_readers;    // atomic because multiple readers have to decrement after reading
        std::atomic<CommitFlag> commit_flag; // 0 = not ready, 1 = ready for consumer
    };

#pragma pack(pop)
}