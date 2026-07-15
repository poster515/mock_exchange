#include "archive/ArchivePublication.h"

#include <ranges>
#include <algorithm>

namespace archive {
    ArchivePublication::ArchivePublication(ArchivePublicationParams&& params)
            : queue(nullptr) {
        queue = std::make_unique<message_transport::IpcQueue>(std::move(params.queue_params));
    }
 
    bool ArchivePublication::is_ready() const {
        return queue->num_readers() > 0;
    }

    size_t ArchivePublication::uncommitted_message_count() const {
        return to_commit.size();
    }

    size_t ArchivePublication::commit() {
        const auto size = to_commit.size();
        std::ranges::for_each(to_commit, [](auto& wrapper) { wrapper.commit(); });
        to_commit.clear();
        return size;
    }

    void ArchivePublication::close() {
        commit();
        queue->close();
    }

    std::span<std::byte> ArchivePublication::claim_buffer(size_t buffer_size) {
        to_commit.emplace_back(queue->claim_buffer<message_transport::BusyWaitPolicy>(buffer_size));
        return to_commit.back().get_as_span();
    }
}