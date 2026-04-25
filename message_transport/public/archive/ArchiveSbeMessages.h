#pragma once

#include <cstdlib>
#include <span>

namespace archive
{
#pragma pack(push, 1)

    enum class MessageType : uint16_t {
        UNKNOWN = 0,
        START_RECORDING = 1,
        STOP_RECORDING = 2
    };

    struct ArchiveMessageHeader {
        MessageType message_type;
        uint16_t message_length;
    };

    /**
     * Given a valid channelID (i.e., one that is active), records that 
     * archive to disk via segment files that rotate every N bytes.
     */
    struct StartArchiveRecording
    {
        ArchiveMessageHeader header {MessageType::START_RECORDING, sizeof(ArchiveMessageHeader) + sizeof(uint16_t)};
        uint16_t channel_id;
    };

    struct StopArchiveRecording
    {
        ArchiveMessageHeader header {MessageType::STOP_RECORDING, sizeof(ArchiveMessageHeader) + sizeof(uint16_t)};
        uint16_t channel_id;
    };

#pragma pack(pop)
}