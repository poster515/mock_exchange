#include "ledger/Ledger.h"

namespace ledger {

    Ledger::Ledger(common::CommonComponents&& components) {
        db = std::make_unique<ManagedDb>();
    }

    void Ledger::start() {
        // we want to spin up a poll runner and run in a worker thread.
        subscription = std::make_unique<archive::ArchiveSubscription>(
            archive::ArchiveSubscription::ArchiveSubscriptionParams {
                .file_name = ""
            }
        );
    }

    void Ledger::run() {
        subscription->poll(*this);
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