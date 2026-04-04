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
            : file_name(params.file_name)
            , queue_size_bytes(params.queue_size + sizeof(GlobalHeader)) 
            , available_queue_size_bytes(params.queue_size)
            , is_writer(params.is_writer) {

        if (params.queue_size > MAX_QUEUE_SIZE_BYTES) {
            throw std::runtime_error("Queue size exceeds maximum allowed size of " + std::to_string(MAX_QUEUE_SIZE_BYTES) + " bytes");
        } else if (!MpscIpcQueue::is_power_of_two(available_queue_size_bytes)) {
            throw std::runtime_error("Queue size is not a power of 2! Please revise.");
        }

        fd = shm_open(params.file_name.data(), O_CREAT | O_RDWR, 0666);

        if (fd == -1) {
            throw std::runtime_error("Failed to open shared memory at file " + file_name);
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
            global_header->write_fields.queue_size_bytes.store(queue_size_bytes, std::memory_order_relaxed);
            global_header->write_fields.message_count.store(0, std::memory_order_relaxed);
            global_header->read_fields.has_writer.store(true, std::memory_order_release);

            global_header->write_fields.write_offset.store(0, std::memory_order_release);
            global_header->read_fields.read_offset.store(0, std::memory_order_release);

            // yeet the whole queue to a known value (0 = MESSAGE_UNKNOWN which is ok for writing)
            memset(reinterpret_cast<uint8_t*>(global_header) + sizeof(GlobalHeader), 0, available_queue_size_bytes);
        } else {

            // if we're the reader, we need to wait until the writer has initialized the global header before we can safely read from it.
            while (!global_header->read_fields.has_writer.load(std::memory_order_acquire)) {
                // busy wait until writer has initialized the global header
                // in a real implementation, we would want to use a more efficient synchronization mechanism here (e.g. futexes or condition variables) to avoid busy waiting and reduce CPU usage.
                std::this_thread::yield();
            }
        }
    }

    MpscIpcQueue::~MpscIpcQueue() {
        munmap(global_header, queue_size_bytes);
        close(fd);
        shm_unlink(file_name.c_str());
    }

    template <CSpinPolicy WritePolicy>
    MpscIpcQueueRaiiWriterWrapper MpscIpcQueue::claim_buffer(size_t size) {
        
        // determine a starting point in the shared memory for the producer to write the message, 
        // and return a wrapper that will commit the buffer to the queue upon destruction.
        if (size > (queue_size_bytes - sizeof(GlobalHeader)) - sizeof(MessageHeader) || !is_writer) {
            // Message size exceeds the total queue capacity, cannot claim buffer
            throw std::runtime_error(std::format("Message size {} bytes exceeds the total queue capacity of {} bytes", size, queue_size_bytes - sizeof(GlobalHeader)));
        } else if (size == 0) {
            throw std::runtime_error("Cannot claim buffer for message with size 0 bytes");
        }

        // only allow committing if we can buffer an extra message at the end of the queue
        const auto total_message_len = size + sizeof(MessageHeader);

        // first we claim the next write location, no matter what. Also waits for any slow reader
        const auto abs_write_offset = wait_for_next_write_offset<WritePolicy>(total_message_len);
        const auto rel_write_offset = abs_write_offset % available_queue_size_bytes;
        const auto bytes_remaining_at_end = available_queue_size_bytes - rel_write_offset;

        // we may have claimed a spot at the end of the buffer that needs a skip message instead. Check, insert, and try again.
        if ((total_message_len + sizeof(MessageHeader)) > bytes_remaining_at_end) {
            insert_skip_message(rel_write_offset);
            return claim_buffer<WritePolicy>(size);
        }

        // we successfully claimed a region for writing, and the new write offset is within the bounds of the queue.
        void* new_buffer_ptr = static_cast<void*>(reinterpret_cast<uint8_t*>(global_header) + rel_write_offset + sizeof(GlobalHeader));
        auto* new_message_header = static_cast<MessageHeader*>(new_buffer_ptr);

        // if we're here we are GUARANTEED to have carte blanche write perms. Wrapper will set commit bit 
        new_message_header->sequence_number = global_header->write_fields.message_count.fetch_add(1, std::memory_order_acq_rel);
        new_message_header->message_size = size;
        new_message_header->type = MessageType::NORMAL;

        // spdlog::info("Claimed relative offset {} with total size {} bytes (bytes at end {}, total avail {})", rel_write_offset, total_message_len, bytes_remaining_at_end, available_queue_size_bytes);
        return MpscIpcQueueRaiiWriterWrapper(reinterpret_cast<uint8_t*>(new_buffer_ptr), total_message_len);
    }

    std::optional<MpscIpcQueueRaiiReaderWrapper> MpscIpcQueue::poll_buffer() {
        if (is_writer) {
            throw std::runtime_error("Producer cannot poll for messages in the queue");
        }

        // if the distance between the read and write offsets is less than the size of a message
        // header, then there are no complete messages to read, so we return nullopt.
        const auto abs_read_offset = global_header->read_fields.read_offset.load(std::memory_order_acquire);
        const auto abs_write_offset = global_header->write_fields.write_offset.load(std::memory_order_acquire);
        if ((abs_write_offset - abs_read_offset) < sizeof(MessageHeader)) {
            return std::nullopt;
        }

        const auto rel_read_offset = abs_read_offset % available_queue_size_bytes;

        void* buffer_ptr = static_cast<void*>(static_cast<char*>(static_cast<void*>(global_header)) + rel_read_offset + sizeof(GlobalHeader));
        auto* message_header = static_cast<MessageHeader*>(buffer_ptr);

        const auto total_message_len = message_header->message_size + sizeof(MessageHeader);

        if (message_header->type == MessageType::PADDING) {
            // if this is a padding message, we need to skip it and move the read offset to the next message after the padding message.
            // spdlog::info("Polled skip message at offset {} with size {}, bytes (total size with header: {} bytes), skipping to beginning of queue", rel_read_offset, message_header->message_size, total_message_len);
            release_buffer(*message_header);
            return poll_buffer();
        }

        // tell the producer that we've leased this message for reading, which will prevent the producer from overwriting this message until we've released it after we're done reading.
        // spdlog::info("Polled message at offset {} with size {}, bytes (total size with header: {} bytes)", rel_read_offset, message_header->message_size, total_message_len);
        return MpscIpcQueueRaiiReaderWrapper(reinterpret_cast<uint8_t*>(buffer_ptr), total_message_len, *this);
    }

    void MpscIpcQueue::release_buffer(MessageHeader& header) {
        // clear the message body so its not interpreted weird if it's on a boundary next wrap around
        auto* data = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(&header) + sizeof(MessageHeader));
        memset(data, 0, header.message_size);

        // now update header flags and bump read offset
        const auto total_message_len = header.message_size + sizeof(MessageHeader);
        header.message_size = 0;
        header.sequence_number = 0;
        header.type = MessageType::NORMAL;
        const auto abs_read_offset = global_header->read_fields.read_offset.fetch_add(total_message_len, std::memory_order_acq_rel);
    }

    template <CSpinPolicy ReadPolicy>
    MpscIpcQueueRaiiReaderWrapper MpscIpcQueue::read_buffer() {
        while (true) {
            auto read_wrapper = poll_buffer();
            if (read_wrapper.has_value()) {
                return std::move(*read_wrapper);
            }
            ReadPolicy::execute();
        }
    }

    void MpscIpcQueue::insert_skip_message(const uint64_t skip_offset) {

        // be nice and set these to 0
        auto* message_header = reinterpret_cast<MessageHeader*>(reinterpret_cast<uint8_t*>(global_header) + skip_offset + sizeof(GlobalHeader));
        auto* message_payload = static_cast<void*>(message_header + sizeof(MessageHeader));
        const auto padding_size = available_queue_size_bytes - skip_offset - sizeof(MessageHeader);
        std::memset(message_payload, 0, padding_size);

        message_header->sequence_number = 0;
        message_header->message_size = padding_size;
        message_header->type = MessageType::PADDING;
    }

    template MpscIpcQueueRaiiWriterWrapper MpscIpcQueue::claim_buffer<BusyWaitPolicy>(size_t n);
    template MpscIpcQueueRaiiWriterWrapper MpscIpcQueue::claim_buffer<YieldPolicy>(size_t n);
    template MpscIpcQueueRaiiWriterWrapper MpscIpcQueue::claim_buffer<SleepPolicy>(size_t n);
    template MpscIpcQueueRaiiWriterWrapper MpscIpcQueue::claim_buffer<HybridPolicy>(size_t n);

    template MpscIpcQueueRaiiReaderWrapper MpscIpcQueue::read_buffer<BusyWaitPolicy>();
    template MpscIpcQueueRaiiReaderWrapper MpscIpcQueue::read_buffer<YieldPolicy>();
    template MpscIpcQueueRaiiReaderWrapper MpscIpcQueue::read_buffer<SleepPolicy>();
    template MpscIpcQueueRaiiReaderWrapper MpscIpcQueue::read_buffer<HybridPolicy>();
}