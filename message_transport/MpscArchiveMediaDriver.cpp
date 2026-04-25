#include "archive/MpscArchiveMediaDriver.h"

#include "messaging/mpsc_ipc_queue_element_wrapper.h"

namespace archive
{
    MpscArchiveMediaDriver::MpscArchiveMediaDriver(MpscArchiveParams&& p)
        : params(std::move(p))
        , queue(message_transport::MpscIpcQueue::MpscQueueParameters {
            .file_name = DEFAULT_SHM_NAME, // config lib doesn't support string_view getters
            .queue_size = params.comps.config.lookup("mpsc_queue.queue_size"),
            .is_writer = false
        })
    {
        // nothing to do here
    }

    void MpscArchiveMediaDriver::start()
    {
        this->initialized = true;
    }

    void MpscArchiveMediaDriver::run()
    {
        while (this->running) {
            auto reader = queue.poll_buffer();
            if (reader.has_value())
            {
                process_message(reader->get_as_view<std::span<const std::byte>, std::byte>());
            } else {
                std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
        }
    }

    void MpscArchiveMediaDriver::stop() {

    }

    void MpscArchiveMediaDriver::process_message(std::span<const std::byte> message)
    {
        
    }
}