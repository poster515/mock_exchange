#include "archive/MpscArchiveMediaDriver.h"
#include "archive/ArchiveSbeMessages.h"

#include "messaging/ipc_queue_element_wrapper.h"

namespace archive
{
    MpscArchiveMediaDriver::MpscArchiveMediaDriver(MpscArchiveParams&& p)
        : params(std::move(p))
        , queue(message_transport::IpcQueue::IpcQueueParameters {
            .file_name = std::string(DEFAULT_SHM_NAME), // config lib doesn't support string_view getters
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
        const auto* message_header = reinterpret_cast<const archive::ArchiveMessageHeader*>(message.data());
        switch (message_header->message_type)
        {
            case archive::MessageType::START_RECORDING: {
                break;
            }
            case archive::MessageType::STOP_RECORDING: {
                break;
            }
            default: {
                break;
            }
        }
    }
}