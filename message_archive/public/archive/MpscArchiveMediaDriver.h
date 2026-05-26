#pragma once
#include <span>

#include "utils/Config.h"

#include "messaging/ipc_queue.h"

namespace archive {
    /**
     * Media driver that listens to a control channel and 
     * 
     */
    class MpscArchiveMediaDriver : public common::IApplicationService {
    public:
        static constexpr size_t DEFAULT_ROTATION_SIZE = 1 << 20; // 1 MB
        static constexpr std::string_view DEFAULT_FILE_PATTERN = "%s_%s.log";

        static constexpr std::string_view DEFAULT_SHM_NAME = "/dev/shm/media_driver_ctrl";

        struct MpscArchiveParams {
            size_t rotation_size {DEFAULT_ROTATION_SIZE};
            std::string file_pattern {DEFAULT_FILE_PATTERN};
            common::CommonComponents comps;
        };

        MpscArchiveMediaDriver(MpscArchiveParams&&);

        void start() override final;
        void run() override final;
        void stop() override final;

    protected:
        MpscArchiveParams params;
        message_transport::IpcQueue queue;

        void process_message(std::span<const std::byte> message);
    };
}