#include "gateway/FixSbeGateway.h"

#include "polling/RuntimePollRunnerBuilder.h"

namespace gateway {
    FixSbeGateway::FixSbeGateway(common::CommonComponents&& components)
        : publish_queue(message_transport::MpscIpcQueue::MpscQueueParameters{
            .file_name = std::string(FixSbeGateway::DEFAULT_QUEUE_NAME),
            .queue_size = FixSbeGateway::DEFAULT_QUEUE_SIZE
        })
        , pollrunner(nullptr) {

    }

    void FixSbeGateway::run() {
        // TODO
        initialize();
        // poll
    }

    void FixSbeGateway::stop() {
        this->running = false;
    }

    void FixSbeGateway::initialize() {
        auto builder = polling::RuntimePollRunnerBuilder();
        pollrunner = builder.add_pollable([]() -> std::unique_ptr<polling::IPollable> { return nullptr; }).build_runner();
    }

    void poll() {

    }
}