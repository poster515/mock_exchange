#pragma once

#include <memory>

#include "messaging/mpsc_ipc_queue.h"
#include "polling/IPollRunner.h"

#include <utils/Config.h>


namespace gateway {
    /**
     * This class is in charge of managing multiple gateway sessions and 
     * publishing decoding session data to a managed queue.
     * 
     * There may be many different kinds of gateway (order, market_data, etc),
     * so this is intended to be generic enough of an interface for now.
     */
    class FixSbeGateway : virtual public common::IApplicationService{
        static constexpr std::string_view DEFAULT_QUEUE_NAME = "/dev/shm/gateway_queue";
        static constexpr size_t DEFAULT_QUEUE_SIZE = 1 << 20; // 1MB

    public:
        FixSbeGateway(common::CommonComponents&& components);
        void run() override final;
        void stop() override final;

    protected:
        // because this is a byte ring buffer we can yolo anything we want in here
        message_transport::MpscIpcQueue publish_queue;
        std::unique_ptr<polling::IPollRunner> pollrunner;

        void initialize();
        void poll();
    };
}