#include "messaging/spmc_ipc_queue.h"

namespace message_transport {
    SpmcIpcQueue::SpmcIpcQueue(SpmcQueueParameters&& params) {
    
    }

    SpmcIpcQueue::~SpmcIpcQueue() {
        
    }

    template <CSpinPolicy WritePolicy>
    IpcQueueRaiiWriterWrapper SpmcIpcQueue::claim_buffer(size_t size) {
        return IpcQueueRaiiWriterWrapper(nullptr, size);
    }

    std::optional<IpcQueueRaiiReaderWrapper> SpmcIpcQueue::poll_buffer() {
        return std::nullopt;
    }

    template <CSpinPolicy ReadPolicy>
    IpcQueueRaiiReaderWrapper SpmcIpcQueue::read_buffer() {
        while (true) {
            auto read_wrapper = poll_buffer();
            if (read_wrapper.has_value()) {
                return std::move(*read_wrapper);
            }
            ReadPolicy::execute();
        }
    }

    void SpmcIpcQueue::release_buffer(MessageHeader& header) {

    }

    template IpcQueueRaiiWriterWrapper SpmcIpcQueue::claim_buffer<BusyWaitPolicy>(size_t n);
    template IpcQueueRaiiWriterWrapper SpmcIpcQueue::claim_buffer<YieldPolicy>(size_t n);
    template IpcQueueRaiiWriterWrapper SpmcIpcQueue::claim_buffer<SleepPolicy>(size_t n);
    template IpcQueueRaiiWriterWrapper SpmcIpcQueue::claim_buffer<HybridPolicy>(size_t n);
}