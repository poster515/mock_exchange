#include "gateway/FixSbeGateway.h"

namespace gateway {
    FixSbeGateway::FixSbeGateway(const libconfig::Config& config)
        : publish_queue(message_transport::MpscIpcQueue::MpscQueueParameters{
            .file_name = std::string(FixSbeGateway::DEFAULT_QUEUE_NAME),
            .queue_size = FixSbeGateway::DEFAULT_QUEUE_SIZE
        })
        , pollrunner(nullptr) {

    }
}