#include "gateway/SbeGateway.h"

#include "polling/RuntimePollRunnerBuilder.h"

namespace gateway {
    SbeGateway::SbeGateway(common::CommonComponents&& components)
        : publish_queue(message_transport::MpscIpcQueue::MpscQueueParameters{
            .file_name = std::string(SbeGateway::DEFAULT_QUEUE_NAME),
            .queue_size = SbeGateway::DEFAULT_QUEUE_SIZE
        })
        , pollrunner(nullptr) {

    }

    void SbeGateway::run() {
        // TODO
        initialize();
        // poll
    }

    void SbeGateway::stop() {
        this->running = false;
    }

    void SbeGateway::initialize() {
        auto builder = polling::RuntimePollRunnerBuilder();
        pollrunner = builder.add_pollable([]() -> std::unique_ptr<polling::IPollable> { return nullptr; }).build_runner();
    }

    void poll() {

    }
}