#pragma once

#include "messaging/mpsc_ipc_queue.h"

namespace archive {
    /**
     * Subscribes to a recording and persists to file, rotating every N bytes.
     */
    class MpscArchiveMediaDriver {
    public:
        static constexpr size_t DEFAULT_ROTATION_SIZE = 1 << 20; // 1 MB
        static constexpr std::string_view DEFAULT_FILE_PATTERN = "%s_%s.log";

        struct MpscArchiveParams {
            size_t rotation_size {DEFAULT_ROTATION_SIZE};
            std::string file_pattern {DEFAULT_FILE_PATTERN};
        };

    protected:
        MpscArchiveParams params;
    };
}