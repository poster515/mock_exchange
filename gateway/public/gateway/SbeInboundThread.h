#pragma once
#include <poll.h>

#include "polling/PollableConcept.h"

namespace gateway {

    /**
     * Listens for new raw connection, passes messages and new fd off to session management.
     */
    class SbeInboundThread : public polling::IPollable {
    public:
        
        struct SbeInboundThreadParams {
            // need a logon handler - could be a singleton but probably a concrete instance
            // need to store session info either in-memory or IPC that session management can takeover
        };

        SbeInboundThread();

        bool Initialize() override final;
        size_t PollOnce() override final;
        void StopPolling() override final;

        ~SbeInboundThread();

    private:
        pollfd server_socket;
    };
}