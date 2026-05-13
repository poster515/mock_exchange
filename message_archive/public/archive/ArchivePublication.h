#pragma once

#include <span>

namespace archive 
{
    class ArchivePublication {

    public:
        ArchivePublication() = default;

        bool is_ready() const;
        void wait_for_healthy_publication();

        // users of this publication should 
        std::span<std::byte> claim_buffer(size_t buffer_size);
        
    private:
        message_transport::
    };
}