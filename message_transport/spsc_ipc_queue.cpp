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
            global_header->read_offset.store(0, std::memory_order_relaxed);
            global_header->write_offset.store(0, std::memory_order_release);
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

    SpscIpcQueueRaiiWrapper SpscIpcQueue::claim_buffer(size_t size) {
        
        // determine a starting point in the shared memory for the producer to write the message, 
        // and return a wrapper that will commit the buffer to the queue upon destruction.

        // need to get the actual current read offset of the reader, this may increment but we 
        // need to do this with memory_order_acquire to ensure we see the latest value written by the reader.
        size_t current_read_offset = global_header->read_offset.load(std::memory_order_acquire);

        // can do a relaxed load here since we only care about the current write offset for calculating the
        // buffer position, and we will ensure proper synchronization when committing the buffer.
        size_t current_write_offset = global_header->write_offset.load(std::memory_order_relaxed);
        if (size > queue_size_bytes) {
            // Message size exceeds the total queue capacity, cannot claim buffer
            return SpscIpcQueueRaiiWrapper(nullptr, size, *this);
        }

        if (size <= (queue_size_bytes - current_write_offset)) {
            void* buffer_ptr = static_cast<void*>(static_cast<char*>(static_cast<void*>(global_header)) + sizeof(GlobalHeader) + current_write_offset);
            return SpscIpcQueueRaiiWrapper(buffer_ptr, current_write_offset, *this);
        }

        // Calculate the starting position for the buffer within shared memory
        size_t buffer_start_offset = current_write_offset % queue_size_bytes;

        return SpscIpcQueueRaiiWrapper(nullptr, size, *this);
    }
}