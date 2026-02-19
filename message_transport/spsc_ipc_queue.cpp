#include "messaging/spsc_ipc_queue.h"
#include "messaging/spsc_ipc_queue_element_wrapper.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <thread>

namespace message_transport {
    SpscIpcQueue::SpscIpcQueue(std::string_view shm_file_name, size_t queue_size_bytes, bool is_writer)
            : queue_size_bytes(queue_size_bytes) 
            , is_writer(is_writer) {

        if (queue_size_bytes > MAX_QUEUE_SIZE_BYTES) {
            throw std::runtime_error("Queue size exceeds maximum allowed size of " + std::to_string(MAX_QUEUE_SIZE_BYTES) + " bytes");
        }

        int fd = shm_open(shm_file_name.data(), O_CREAT | O_RDWR, 0666);

        if (fd == -1) {
            throw std::runtime_error("Failed to open shared memory at file " + std::string(shm_file_name));
        }

        ftruncate(fd, queue_size_bytes);
        void* ptr = mmap(nullptr, queue_size_bytes,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        fd, 0);
        
        // anytime we want to access read_ or write_offset, we just reinterpret_cast
        // global_header to a void* and then add the appropriate offset to get to the correct position in
        // shared memory, and then cast that to a pointer to the appropriate type (e.g. std::atomic<size_t>*).
        global_header = static_cast<GlobalHeader*>(ptr);

        if (is_writer) {
            // if we're the writer, we need to initialize the global header to set the initial state of the queue.
            global_header->queue_size_bytes.store(queue_size_bytes, std::memory_order_relaxed);
            global_header->message_count.store(0, std::memory_order_relaxed);
            global_header->has_writer.store(true, std::memory_order_release);
        } else {
            // TODO: I want to think about what this should do for the reader.
            //      I think I'd prefer to set a "is_initalized" flag that we check and then
            //      allow the user to specify the wait policy.

            // if we're the reader, we need to wait until the writer has initialized the global header before we can safely read from it.
            while (!global_header->has_writer.load(std::memory_order_acquire)) {
                // busy wait until writer has initialized the global header
                // in a real implementation, we would want to use a more efficient synchronization mechanism here (e.g. futexes or condition variables) to avoid busy waiting and reduce CPU usage.
                std::this_thread::yield();
            }
        }
    }

    SpscIpcQueue::~SpscIpcQueue() {
        // Destructor implementation
    }

    std::optional<SpscIpcQueueRaiiWrapper> SpscIpcQueue::blocking_claim_buffer(size_t size) {
        
        // determine a starting point in the shared memory for the producer to write the message, 
        // and return a wrapper that will commit the buffer to the queue upon destruction.
        if (size > (queue_size_bytes - sizeof(GlobalHeader)) - sizeof(MessageHeader)) {
            // Message size exceeds the total queue capacity, cannot claim buffer
            return std::nullopt;
        }

        // need to get the actual current read offset of the reader, this may increment but we 
        // need to do this with memory_order_acquire to ensure we see the latest value written by the reader.
        size_t current_read_offset = global_header->read_offset.load(std::memory_order_acquire);

        // can do a relaxed load here since we only care about the current write offset for calculating the
        // buffer position, and we will ensure proper synchronization when committing the buffer. Also,
        // since we are currently the only writer this thread is the only writing thread.
        // TODO: once we move to a multi-producer architecture - THIS MUST USE CAS SEMANTICS
        size_t current_write_offset = global_header->write_offset.load(std::memory_order_relaxed);
        const size_t max_user_payload_bytes = queue_size_bytes - current_write_offset - sizeof(MessageHeader);

        // get the current buffer pointer using this offset
        void* buffer_ptr = static_cast<void*>(static_cast<char*>(static_cast<void*>(global_header)) + sizeof(GlobalHeader) + current_write_offset);
        auto* message_header = static_cast<MessageHeader*>(buffer_ptr);

        // TODO: when we go to a multi-producer paradigm, we'll need to make this an atomic compare_exchange loop
        // to ensure that only one producer can claim a given buffer space, but since this is single-producer we
        // can just calculate the buffer position based on the current write offset and the size of the message.
        if (size <= max_user_payload_bytes) {
            message_header->message_size.store(size, std::memory_order_release); // for now stick with relased here, may be able to relax this later.
            message_header->flags.store(MESSAGE_LEASED, std::memory_order_release); // set this last, its used to determine availability of this chunk by others

            // have to increment the write offset in the global header to reflect the fact that we've claimed this buffer
            // space for writing, we can do this with a relaxed store since the write offset is only used by the producer
            // to calculate buffer positions and is not used for synchronization with the consumer.
            global_header->write_offset.store(current_write_offset + sizeof(MessageHeader) + size, std::memory_order_release);
            return SpscIpcQueueRaiiWrapper(buffer_ptr, size, *this);
        }

        // if there's no room at the end of the queue for this message, we need to write a "skipped" message header to indicate
        // to the consumer that it should skip over this space and then wrap around to the beginning of the queue to write the
        // message there. This is necessary to ensure that the consumer can correctly read messages from the queue without getting
        // confused by unused space at the end of the queue.
        message_header->message_size.store(max_user_payload_bytes, std::memory_order_relaxed); // might be able to remove atomic from this field entirely
        message_header->flags.store(MESSAGE_SKIPPED, std::memory_order_release);

        // now try and claim the buffer at the beginning of the queue for this message, we can be assured that this will succeed since the maximum message size is less than the total queue capacity, so there must be room at the beginning of the queue for this message.
        void* new_buffer_ptr = static_cast<void*>(static_cast<char*>(static_cast<void*>(global_header)) + sizeof(GlobalHeader));
        auto* new_message_header = static_cast<MessageHeader*>(new_buffer_ptr);
        size_t total_available_buffer_space = 0;

        // wait for the slot to become available at all
        total_available_buffer_space += wait_for_slot_until(*new_message_header);

        while (total_available_buffer_space < size) {
            // advance to the next message header
            new_buffer_ptr = static_cast<void*>(static_cast<char*>(new_buffer_ptr) + sizeof(MessageHeader) + new_message_header->message_size.load(std::memory_order_relaxed));
            new_message_header = static_cast<MessageHeader*>(new_buffer_ptr);

            // wait for the slot to become available at all
            total_available_buffer_space += wait_for_slot_until(*new_message_header) + sizeof(MessageHeader);
        }

        new_message_header->message_size.store(size, std::memory_order_release);
        new_message_header->flags.store(MESSAGE_LEASED, std::memory_order_release);

        // finally, we have to "reset" the write offset to the next wrapped position
        global_header->write_offset.store(sizeof(GlobalHeader) + sizeof(MessageHeader) + size, std::memory_order_release);
        return SpscIpcQueueRaiiWrapper(buffer_ptr, size, *this);
    }


    std::optional<SpscIpcQueueRaiiWrapper> SpscIpcQueue::nonblocking_claim_buffer(size_t size) {
        
    }

    std::optional<SpscIpcQueueRaiiWrapper> SpscIpcQueue::poll_buffer() {
        // TODO: poll the queue for new messages, if a new message is available, return a wrapper around the message
        // buffer for the consumer to read from. If no new messages are available, return immediately.

        if (is_writer) {
            throw std::runtime_error("Producer cannot poll for messages in the queue");
        }

        size_t current_read_offset = global_header->read_offset.load(std::memory_order_acquire);

        void* buffer_ptr = static_cast<void*>(static_cast<char*>(static_cast<void*>(global_header)) + sizeof(GlobalHeader) + current_read_offset);
        auto* message_header = static_cast<MessageHeader*>(buffer_ptr);
        if (message_header->flags.load(std::memory_order_acquire) == MESSAGE_AVAILABLE) {
            global_header->read_offset.store(current_read_offset + sizeof(MessageHeader) + message_header->message_size.load(std::memory_order_acquire), std::memory_order_release);
            return SpscIpcQueueRaiiWrapper(buffer_ptr, message_header->message_size.load(std::memory_order_acquire), *this);
        }
        return std::nullopt;
    }

    void SpscIpcQueue::read_buffer() {

    }

    size_t SpscIpcQueue::wait_for_slot_until(const MessageHeader& header, std::chrono::nanoseconds timeout) {
        auto start_time = std::chrono::steady_clock::now();
        while (header.flags.load(std::memory_order_acquire) > MESSAGE_SKIPPED) {
            if (std::chrono::steady_clock::now() - start_time > timeout) {
                throw std::runtime_error("Timeout while waiting for buffer slot to be released");
            }
            std::this_thread::sleep_for(std::chrono::nanoseconds(50));
        }

        return header.message_size.load(std::memory_order_relaxed);
    }
}