#include "archive/ArchivePublication.h"
#include "archive/ArchiveSubscription.h"

#include <sys/mman.h>

extern "C" {

    void archive_force_close(const char* queue_name) {
        spdlog::info("Attempting to unlink/force close archive {}...", queue_name);
        if (shm_unlink(queue_name) == -1) {
            spdlog::warn(
                "shm_unlink({}) failed: {} (errno={})",
                queue_name,
                std::strerror(errno),
                errno);
        }
    }

    archive::ArchivePublication* archive_pub_create(const char* queue_name, size_t queue_size, const char* agent_name) {
        return new archive::ArchivePublication(
            archive::ArchivePublication::ArchivePublicationParams {
                .queue_params = message_transport::IpcQueue::IpcQueueParameters {
                    .file_name = queue_name,
                    .queue_size = queue_size,
                    .is_writer = true,
                    .agent_name = agent_name
                }
            }
        );
    }

    bool archive_pub_is_ready(archive::ArchivePublication* pub) {
        spdlog::info("Attempting to get archive readiness for agent: '{}'", pub->agent_name());
        return pub->is_ready();
    }

    uint8_t* archive_pub_claim(archive::ArchivePublication* pub, size_t size) {
        // spdlog::info("Claiming {} bytes for publication agent: '{}'", size, pub->agent_name());
        std::byte* data = pub->claim_buffer(size).data();
        uint8_t* p = reinterpret_cast<uint8_t*>(data);
        return p;
    }

    size_t archive_pub_commit(archive::ArchivePublication* pub) {
        // spdlog::info("Committing messages from agent: '{}'", pub->agent_name());
        return pub->commit();
    }

    void archive_pub_close(archive::ArchivePublication* pub) {
        spdlog::info("Closing publication from agent: '{}'", pub->agent_name());
        pub->close();
    }

    void archive_pub_destroy(archive::ArchivePublication* pub) {
        spdlog::info("Deleting publication from agent: '{}'", pub->agent_name());
        delete pub;
    }

    archive::ArchiveSubscription* archive_sub_create(const char* queue_name, size_t queue_size, const char* name) {
        spdlog::info("Attempting to create new subscription on {} for agent '{}' (size {})", queue_name, name, queue_size);
        return new archive::ArchiveSubscription(
            archive::ArchiveSubscription::ArchiveSubscriptionParams {
                .queue_params = message_transport::IpcQueue::IpcQueueParameters {
                    .file_name = queue_name,
                    .queue_size = queue_size,
                    .is_writer = false,
                    .agent_name = name
                }
            }
        );
    }

    bool archive_sub_is_ready(void* ptr) {
        try {
            auto* sub = reinterpret_cast<archive::ArchiveSubscription*>(ptr);
            return sub->is_ready();
        } catch (...) {
            printf("Error checking subscription readiness!");
            return false;
        }
    }

    void archive_sub_poll(void* ptr, uint8_t(*handler)(const uint8_t*, size_t)) {
        try {
            auto* sub = reinterpret_cast<archive::ArchiveSubscription*>(ptr);
            sub->poll(handler);
        } catch (...) {
            printf("Error polling subscription!");
        }
    }

    void archive_sub_close(void* ptr) {
        try {
            auto* sub = reinterpret_cast<archive::ArchiveSubscription*>(ptr);
            sub->close();
        } catch (...) {
            printf("Error closing subscription!");
        }
    }

    void archive_sub_destroy(void* ptr) {
        try {
            auto* sub = reinterpret_cast<archive::ArchiveSubscription*>(ptr);
            delete sub;
        } catch (...) {
            printf("Error deleting subscription!");
        }
    }
}