#pragma once

#include "messaging/mpsc_ipc_queue.h"

namespace gateway {
    /**
     * This class is in charge of managing multiple gateway sessions and 
     * publishing decoding session data to a managed queue.
     * 
     * There may be many different kinds of gateway (order, market_data, etc),
     * so this is intended to be generic enough of an interface for now.
     */
    class FixSbeGateway {
    public:
        FixSbeGateway() = default;

    protected:
        // because this is a byte ring buffer we can yolo anything we want in here
        message_transport::MpscIpcQueue publish_queue;
    };
}