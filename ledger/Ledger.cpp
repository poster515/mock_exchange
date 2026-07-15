#include "ledger/Ledger.h"

namespace ledger {

    Ledger::Ledger(common::CommonComponents&& components)
            : common(components) {
        db = std::make_unique<ManagedDb>();
    }

    void Ledger::start() {

        // we want to spin up a poll runner and run in a worker thread.
        subscription = std::make_unique<archive::ArchiveSubscription>(
            archive::ArchiveSubscription::ArchiveSubscriptionParams {
                .queue_params = message_transport::IpcQueue::IpcQueueParameters {
                    .file_name = common.config.lookup("queue_name"),
                    .queue_size = common.config.lookup("queue_name"),
                    .is_writer = false
                }
            }
        );
    }

    void Ledger::run() {
        subscription->poll([this](const uint8_t* data, size_t len) -> uint8_t {
            const std::byte* as_bytes = reinterpret_cast<const std::byte*>(data);
            std::span<const std::byte> s {as_bytes, len};
            return static_cast<uint8_t>(this->on_fragment(s));
        });
    }

    archive::FragmentAction Ledger::on_fragment(std::span<const std::byte> bytes) {

        if (!bytes.empty()) {

            // get bytes off wire and parse. These will be things like 
            // new orders from AI agents.
            const auto* hdr = reinterpret_cast<const exchange::order::MessageHeader*>(bytes.data());

            switch (hdr->templateId()) {
                case (exchange::order::NewOrderSingle::sbeTemplateId()): {
                    process_new_order(*reinterpret_cast<const exchange::order::NewOrderSingle*>(bytes.data()));
                    break;
                }
                case (exchange::order::CancelOrder::sbeTemplateId()): {
                    process_cancel_order(*reinterpret_cast<const exchange::order::CancelOrder*>(bytes.data()));
                    break;
                }
                case (exchange::order::ReplaceOrder::sbeTemplateId()): {
                    process_replace_order(*reinterpret_cast<const exchange::order::ReplaceOrder*>(bytes.data()));
                    break;
                }
                default: {
                    spdlog::warn("Unknown templateID {}, dropping message", hdr->templateId());
                    break;
                }
            }
        }

        return archive::FragmentAction::CONTINUE;
    }

    void Ledger::stop() {
        // quit polling the queue
    }

    void Ledger::process_new_order(const exchange::order::NewOrderSingle& new_order) {
        /**
         * TODO:
         *  - bump metrics
         *  - cache order somewhere
         *  - add to PnL machine
         */

    }
    void Ledger::process_cancel_order(const exchange::order::CancelOrder& order) {
        /**
         * TODO:
         *  - bump metrics
         *  - cache order somewhere
         *  - add to PnL machine
         */
    }

    void Ledger::process_replace_order(const exchange::order::ReplaceOrder& order) {

    }
}