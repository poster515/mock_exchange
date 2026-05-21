#include "messaging/spmc_ipc_queue.h"

namespace message_transport {
    SpmcIpcQueue::SpmcIpcQueue(SpmcQueueParameters&& params) {
    
    }

    SpmcIpcQueue::~SpmcIpcQueue() {
        
    }

    template <CSpinPolicy WritePolicy>
    MpscIpcQueueRaiiWriterWrapper SpmcIpcQueue::claim_buffer(size_t size) {
        return MpscIpcQueueRaiiWriterWrapper(nullptr, size);
    }

    std::optional<MpscIpcQueueRaiiReaderWrapper> SpmcIpcQueue::poll_buffer() {
        return std::nullopt;
    }

    template <CSpinPolicy ReadPolicy>
    MpscIpcQueueRaiiReaderWrapper SpmcIpcQueue::read_buffer() {
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

    template MpscIpcQueueRaiiWriterWrapper SpmcIpcQueue::claim_buffer<BusyWaitPolicy>(size_t n);
    template MpscIpcQueueRaiiWriterWrapper SpmcIpcQueue::claim_buffer<YieldPolicy>(size_t n);
    template MpscIpcQueueRaiiWriterWrapper SpmcIpcQueue::claim_buffer<SleepPolicy>(size_t n);
    template MpscIpcQueueRaiiWriterWrapper SpmcIpcQueue::claim_buffer<HybridPolicy>(size_t n);
}