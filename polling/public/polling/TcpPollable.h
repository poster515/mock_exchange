#pragma once

#include <poll.h>

#include "polling/PollableConcept.h"

namespace polling {
    class TcpPollable : public IPollable {

    public:
        struct TcpPollableParams {

        };

        TcpPollable(TcpPollableParams&& params);

        bool Initialize() override final;
        size_t PollOnce() override final;
        void StopPolling() override final;

    private:
        pollfd server_socket;
    };
}