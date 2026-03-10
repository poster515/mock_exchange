#include "messaging/mpsc_ipc_queue.h"
#include "messaging/mpsc_ipc_queue_element_wrapper.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <format>


namespace message_transport {
    MpscIpcQueue::MpscIpcQueue(MpscQueueParameters&& params)
            : queue_size_bytes(params.queue_size) 
            , dispatcher(params.callback)
            , is_writer(params.is_writer) {

        if (queue_size_bytes > MAX_QUEUE_SIZE_BYTES) {
            throw std::runtime_error("Queue size exceeds maximum allowed size of " + std::to_string(MAX_QUEUE_SIZE_BYTES) + " bytes");
        }

        fd = shm_open(params.file_name.data(), O_CREAT | O_RDWR, 0666);

        if (fd == -1) {
            throw std::runtime_error("Failed to open shared memory at file " + std::string(params.file_name));
        }

        // TODO: for some reason checking this return code fails in unit tests.
        std::ignore = ftruncate(fd, queue_size_bytes);
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

            global_header->write_offset.store(sizeof(GlobalHeader), std::memory_order_release);
            global_header->read_offset.store(sizeof(GlobalHeader), std::memory_order_release);

            // yeet the whole queue to a known value (0 = MESSAGE_UNKNOWN which is ok for writing)
            memset(reinterpret_cast<uint8_t*>(global_header) + sizeof(GlobalHeader), 0, queue_size_bytes - sizeof(GlobalHeader));
        } else {

            // if we're the reader, we need to wait until the writer has initialized the global header before we can safely read from it.
            while (!global_header->has_writer.load(std::memory_order_acquire)) {
                // busy wait until writer has initialized the global header
                // in a real implementation, we would want to use a more efficient synchronization mechanism here (e.g. futexes or condition variables) to avoid busy waiting and reduce CPU usage.
                std::this_thread::yield();
            }

            if (params.callback.has_value()) {
                read_thread = std::thread([this](){
                    while(this->read_buffer());
                });
            }
        }
    }

    MpscIpcQueue::~MpscIpcQueue() {
        if (read_thread.joinable()) {
            read_thread.join();
        }

        munmap(global_header, queue_size_bytes);
        close(fd);
    }

    MpscIpcQueueRaiiWriterWrapper MpscIpcQueue::blocking_claim_buffer(size_t size) {
        
        // determine a starting point in the shared memory for the producer to write the message, 
        // and return a wrapper that will commit the buffer to the queue upon destruction.
        if (size > (queue_size_bytes - sizeof(GlobalHeader)) - sizeof(MessageHeader) || !is_writer) {
            // Message size exceeds the total queue capacity, cannot claim buffer
            throw std::runtime_error(std::format("Message size {} bytes exceeds the total queue capacity of {} bytes", size, queue_size_bytes - sizeof(GlobalHeader)));
        } else if (size == 0) {
            throw std::runtime_error("Cannot claim buffer for message with size 0 bytes");
        }

        const auto total_message_len = size + sizeof(MessageHeader);

        // only allow committing if we can buffer an extra message at the end of the queue
        const auto size_required = total_message_len + sizeof(MessageHeader);

        while (true) {
            uint64_t current_write_offset = global_header->write_offset.load(std::memory_order_relaxed);
            uint64_t new_write_offset = current_write_offset + total_message_len;

            while(!global_header->write_offset.compare_exchange_weak(current_write_offset, new_write_offset, std::memory_order_release, std::memory_order_relaxed)) {
                // if this compare fails, we _might_ have lost the lock on the current writable region, or it might be a spurious failure.
                // Try again.
                new_write_offset = current_write_offset + total_message_len;
            }

            // first we do a lazy check to see if we can write. Note that if we claim an invalid location this returns immediately
            wait_for_slot_until(current_write_offset, total_message_len);

            // once we're here we know we've claimed a buffer location. see if its valid or not.
            if ((current_write_offset + size_required) <= queue_size_bytes) [[likely]] { 
                // we successfully claimed a region for writing, and the new write offset is within the bounds of the queue,
                // so we can break out of the loop and write our message.

                // ok now we have a slot for writing. Try and block to write in at current_write_offset
                void* new_buffer_ptr = static_cast<void*>(reinterpret_cast<uint8_t*>(global_header) + current_write_offset);
                auto* new_message_header = static_cast<MessageHeader*>(new_buffer_ptr);

                const auto old_flags = static_cast<uint32_t>(new_message_header->commit_flag.load(std::memory_order_acquire));
                if (old_flags != static_cast<uint32_t>(CommitFlag::NOT_READY)) {
                    spdlog::warn("Claimed buffer at offset {} with size {}, bytes (total size with header: {} bytes) but its claimed!! skipping ", current_write_offset, size, total_message_len);
                } else {
                    new_message_header->sequence_number = global_header->message_count.fetch_add(1, std::memory_order_acq_rel);
                    new_message_header->message_size = size;
                    spdlog::info("Claimed buffer at offset {} with size {}, bytes (total size with header: {} bytes)", current_write_offset, size, total_message_len);
                    return MpscIpcQueueRaiiWriterWrapper(reinterpret_cast<uint8_t*>(new_buffer_ptr), total_message_len);
                }
                
            } else if ((current_write_offset + sizeof(MessageHeader)) <= queue_size_bytes) {

                // we claimed successfully, but must write a skip message and try again.
                void* buffer_ptr = static_cast<void*>(reinterpret_cast<uint8_t*>(global_header) + current_write_offset);
                auto* message_header = static_cast<MessageHeader*>(buffer_ptr);
                insert_skip_message(current_write_offset);

                global_header->write_offset.store(sizeof(GlobalHeader), std::memory_order_release);
                // need to try again here too
            }
            // any other conditions require that we try again.
        }
    }

    std::optional<MpscIpcQueueRaiiWriterWrapper> MpscIpcQueue::nonblocking_claim_buffer(size_t size) {
        return std::nullopt;
    }

    std::optional<MpscIpcQueueRaiiReaderWrapper> MpscIpcQueue::poll_buffer() {
        if (is_writer) {
            throw std::runtime_error("Producer cannot poll for messages in the queue");
        }

        size_t current_read_offset = global_header->read_offset.load(std::memory_order_acquire);

        void* buffer_ptr = static_cast<void*>(static_cast<char*>(static_cast<void*>(global_header)) + current_read_offset);
        auto* message_header = static_cast<MessageHeader*>(buffer_ptr);
        auto block_state = message_header->commit_flag.load(std::memory_order_acquire);

        switch (block_state) {
            case CommitFlag::READY_FOR_CONSUMER: {

                const auto total_message_len = message_header->message_size + sizeof(MessageHeader);

                if (message_header->type == MessageType::PADDING) {
                    // if this is a padding message, we need to skip it and move the read offset to the next message after the padding message.
                    spdlog::info("Polled skip message at offset {} with size {}, bytes (total size with header: {} bytes), skipping to beginning of queue", current_read_offset, message_header->message_size, total_message_len);
                    message_header->commit_flag.store(CommitFlag::NOT_READY, std::memory_order_release);
                    global_header->read_offset.store(sizeof(GlobalHeader), std::memory_order_release);
                    return poll_buffer();
                }

                // tell the producer that we've leased this message for reading, which will prevent the producer from overwriting this message until we've released it after we're done reading.
                // spdlog::info("Polled message at offset {} with size {}, bytes (total size with header: {} bytes)", current_read_offset, message_header->message_size, total_message_len);
                return MpscIpcQueueRaiiReaderWrapper(reinterpret_cast<uint8_t*>(buffer_ptr), total_message_len, *this);
            }
            case CommitFlag::NOT_READY:
            default: {
                return std::nullopt;
            }
        }
    }

    void MpscIpcQueue::release_buffer(MessageHeader& header) {
        // clear the message body so its not interpreted weird if it's on a boundary next wrap around
        auto* data = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(&header) + sizeof(MessageHeader));
        memset(data, 0, header.message_size);

        // now update header flags and bump read offset
        const auto total_message_len = header.message_size + sizeof(MessageHeader);
        header.message_size = 0;
        header.sequence_number = 0;
        header.commit_flag.store(CommitFlag::NOT_READY, std::memory_order_release);
        global_header->read_offset.fetch_add(total_message_len, std::memory_order_release);

        const auto release_offset = std::distance(reinterpret_cast<uint8_t*>(global_header), reinterpret_cast<uint8_t*>(&header));

        // check if the new slot is a skip message, and if so, skip over it too.
        auto* next_message_header = reinterpret_cast<MessageHeader*>(reinterpret_cast<uint8_t*>(global_header) + global_header->read_offset.load(std::memory_order_acquire));
        if (next_message_header->type == MessageType::PADDING) {
            const auto skip_offset = std::distance(reinterpret_cast<uint8_t*>(global_header), reinterpret_cast<uint8_t*>(next_message_header));
            spdlog::info("Released message at offset {} with size {}, bytes (total size with header: {} bytes), found skip message at {} with size {} bytes, skipping", release_offset, header.message_size, total_message_len, skip_offset, next_message_header->message_size);
            next_message_header->commit_flag.store(CommitFlag::NOT_READY, std::memory_order_release);
            global_header->read_offset.store(sizeof(GlobalHeader), std::memory_order_release);
        } else {
            spdlog::info("Released message at offset {} with total size: {} bytes", release_offset, total_message_len);
        }
    }

    bool MpscIpcQueue::read_buffer() {
        auto read_wrapper = poll_buffer();
        if (read_wrapper.has_value()) {
            return (*dispatcher)(std::move(*read_wrapper));
        }
        // if there's no return value - indicate that we want to continue to poll.
        return true;
    }

    void MpscIpcQueue::insert_skip_message(const uint64_t skip_offset) {

        // be nice and set these to 0
        auto* message_header = reinterpret_cast<MessageHeader*>(reinterpret_cast<uint8_t*>(global_header) + skip_offset);
        auto* message_payload = static_cast<void*>(message_header + sizeof(MessageHeader));
        const auto padding_size = queue_size_bytes - skip_offset - sizeof(MessageHeader);
        std::memset(message_payload, 0, padding_size);

        message_header->message_size = padding_size;
        message_header->type = MessageType::PADDING;
        message_header->commit_flag.store(CommitFlag::READY_FOR_CONSUMER, std::memory_order_release);

        spdlog::info("Inserted skip message at offset {} with size {} bytes to wrap around the queue", skip_offset, padding_size);
    }
}