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

    SpscIpcQueueRaiiWrapper SpscIpcQueue::blocking_claim_buffer(size_t size) {
        
        // determine a starting point in the shared memory for the producer to write the message, 
        // and return a wrapper that will commit the buffer to the queue upon destruction.
        if (size > (queue_size_bytes - sizeof(GlobalHeader)) - sizeof(MessageHeader)) {
            // Message size exceeds the total queue capacity, cannot claim buffer
            throw std::runtime_error(std::format("Message size {} bytes exceeds the total queue capacity of {} bytes", size, queue_size_bytes - sizeof(GlobalHeader)));
        }

        uint64_t current_write_offset = global_header->write_offset.load(std::memory_order_relaxed);
        uint64_t new_write_offset = current_write_offset + sizeof(MessageHeader) + size;

        bool insert_skip_message_at_current = false;

        if (new_write_offset >= queue_size_bytes) {
            new_write_offset = sizeof(GlobalHeader);
            insert_skip_message_at_current = true;
        }

        // TODO: we need to make sure this current_write_offset is not in a region that is currently being read by the reader
        while(!global_header->write_offset.compare_exchange_weak(current_write_offset, new_write_offset, std::memory_order_release, std::memory_order_relaxed)) {
            // if this compare fails, we _might_ have lost the lock on the current writable region, or it might be a spurious failure.
            // Try again.
            current_write_offset = global_header->write_offset.load(std::memory_order_relaxed);
            new_write_offset = current_write_offset + sizeof(MessageHeader) + size;

            if (new_write_offset >= queue_size_bytes) {
                new_write_offset = sizeof(GlobalHeader);
                insert_skip_message_at_current = true;
            } else {
                insert_skip_message_at_current = false;
            }
        }

        if (insert_skip_message_at_current) {
            void* buffer_ptr = static_cast<void*>(reinterpret_cast<uint8_t*>(global_header) + current_write_offset);
            auto* message_header = static_cast<MessageHeader*>(buffer_ptr);
            insert_skip_message(*message_header, queue_size_bytes - current_write_offset - sizeof(MessageHeader));

            // if we had to insert a skip message, bad luck! we'll recurse and try to claim again.
            // TODO: not a big deal of the recursion idea but logically its correct. Naybe I break this out into a separate function.
            return blocking_claim_buffer(size);
        }

        // ok now we have a slot for writing. Try and block to write in at current_write_offset
        void* new_buffer_ptr = static_cast<void*>(reinterpret_cast<uint8_t*>(global_header) + current_write_offset);
        auto* new_message_header = static_cast<MessageHeader*>(new_buffer_ptr);

        // make sure we aren't overwriting any bytes in a region being read by a slow reader
        // TODO: there is a corner case here where we catch up _exactly_ to the reader and need to wait
        wait_for_slot_until(current_write_offset, size);

        new_message_header->flags.store(MESSAGE_LEASED, std::memory_order_release);
        new_message_header->message_size.store(size, std::memory_order_relaxed);
        return SpscIpcQueueRaiiWrapper(new_buffer_ptr, size, *this);
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
        const auto current_read_offset = global_header->read_offset.load(std::memory_order_acquire);
        const auto current_message_position = sizeof(GlobalHeader) + current_read_offset;
        const auto current_message_header = reinterpret_cast<MessageHeader*>(reinterpret_cast<uint8_t*>(global_header) + current_message_position);
        
        // TODO: dispatch message to consumer at some point here.

        // since we are the only reader we can safely increment the reader offset
        const auto next_read_offset = current_read_offset + sizeof(MessageHeader) + current_message_header->message_size.load(std::memory_order_acquire);
        const auto& header_at_next_read_offset = *reinterpret_cast<MessageHeader*>(reinterpret_cast<uint8_t*>(global_header) + next_read_offset);
        if (header_at_next_read_offset.flags.load(std::memory_order_acquire) == MESSAGE_SKIPPED) {
            // if the next message is a skip message, we need to skip over it and move the read offset to the next message after the skip message.
            global_header->read_offset.store(sizeof(GlobalHeader), std::memory_order_release);
        } else {
            global_header->read_offset.store(next_read_offset, std::memory_order_release);
        }

        // finally, mark as available
        current_message_header->flags.store(MESSAGE_AVAILABLE, std::memory_order_release);
    }

    void SpscIpcQueue::insert_skip_message(MessageHeader& header, size_t padded_bytes) {
        const auto current_read_offset = global_header->read_offset.load(std::memory_order_acquire);
        const auto current_skip_message_position = std::distance(reinterpret_cast<uint8_t*>(global_header), reinterpret_cast<uint8_t*>(&header));
        
        // make sure the reader is out of our way
        wait_for_slot_until(current_skip_message_position, padded_bytes);

        header.message_size.store(padded_bytes, std::memory_order_release);
        header.flags.store(MESSAGE_SKIPPED, std::memory_order_release);

        // be nice and set these to 0
        auto* message_payload = static_cast<void*>(reinterpret_cast<uint8_t*>(&header) + sizeof(MessageHeader));
        std::memset(message_payload, 0, padded_bytes);
    }
}