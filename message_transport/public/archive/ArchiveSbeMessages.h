#pragma once

#include <cstdlib>

namespace archive
{
#pragma pack(push, 1)

    struct StartArchiveRecording
    {
        uint16_t channel_id;
    };

    struct StopArchiveRecording
    {
        uint16_t channel_id;
    };

    struct GetArchiveSubscription
    {
        uint16_t channel_id;
    };

#pragma pack(pop)
}