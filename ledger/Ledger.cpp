#include "ledger/Ledger.h"

#include "sbe/generated/exchange_order/AgentStartup.h"

namespace ledger {

    Ledger::Ledger(common::CommonComponents&& components)
            : common(components) {
        db = std::make_unique<ManagedDb>();
    }

    void Ledger::start() {

        // we want to spin up a poll runner and run in a worker thread.
        order_entry_subscription = std::make_unique<archive::ArchiveSubscription>(
            archive::ArchiveSubscription::ArchiveSubscriptionParams {
                .queue_params = message_transport::IpcQueue::IpcQueueParameters {
                    .file_name = common.config.lookup(ORDER_ENTRY_QUEUE),
                    .queue_size = DEFAULT_QUEUE_SIZE,
                    .is_writer = false,
                    .agent_name = AGENT_NAME
                }
            }
        );

        admin_publication = std::make_unique<archive::ArchivePublication>(
            archive::ArchivePublication::ArchivePublicationParams {
                .queue_params = message_transport::IpcQueue::IpcQueueParameters {
                    .file_name = common.config.lookup(ADMIN_OUT_QUEUE),
                    .queue_size = DEFAULT_QUEUE_SIZE,
                    .is_writer = true,
                    .agent_name = AGENT_NAME
                }
            }
        );

        admin_subscription = std::make_unique<archive::ArchiveSubscription>(
            archive::ArchiveSubscription::ArchiveSubscriptionParams {
                .queue_params = message_transport::IpcQueue::IpcQueueParameters {
                    .file_name = common.config.lookup(ADMIN_IN_QUEUE),
                    .queue_size = DEFAULT_QUEUE_SIZE,
                    .is_writer = false,
                    .agent_name = AGENT_NAME
                }
            }
        );
    }

    void Ledger::run() {
        order_entry_subscription->poll([this](const uint8_t* data, size_t len) -> uint8_t {
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
                case (exchange::order::AgentStartup::sbeTemplateId()): {
                    spdlog::warn("Unsupported templateID {}, dropping message", hdr->templateId());
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
        const auto symbol_id = new_order.symbolId();
        if (!products.contains(symbol_id)) {
            spdlog::warn("Ledger::process_new_order: could not find product for id: {}", symbol_id);
            return;
        }
    }
    void Ledger::process_cancel_order(const exchange::order::CancelOrder& order) {
        /**
         * TODO:
         *  - bump metrics
         *  - cache order somewhere
         *  - add to PnL machine
         */

        const auto symbol_id = order.symbolId();
        if (!products.contains(symbol_id)) {
            spdlog::warn("Ledger::process_cancel_order: could not find product for id: {}", symbol_id);
            return;
        }

        auto& product = products.at(symbol_id);

        const auto id = order.orderId();
        if (active_orders.contains(id)) {
            auto order = active_orders.at(id);
            product.get_book().erase_order(order);
            spdlog::info("Cancelled order: {}");
            active_orders.erase(id);
        }
    }

    void Ledger::process_replace_order(const exchange::order::ReplaceOrder& order) {
        const auto order_id = order.orderId();
        if (!active_orders.contains(order_id)) {
            spdlog::warn("Received replace order for non-existent order_id: {}", order_id);
        }
    }
}