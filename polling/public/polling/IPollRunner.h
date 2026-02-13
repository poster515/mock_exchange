

#pragma once

namespace polling
{
    class IPollRunner
    {
        public:
            virtual bool StartPolling() = 0;
            virtual void PollAll() = 0;
            virtual void StopPolling() = 0;
    };
}