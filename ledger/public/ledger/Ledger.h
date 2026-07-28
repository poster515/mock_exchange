#pragma once

#include <memory>
#include <sqlite3.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>

#include <utils/Config.h>
#include "archive/ArchiveSubscription.h"

#include "sbe/generated/exchange_order/MessageHeader.h"
#include "sbe/generated/exchange_order/NewOrderSingle.h"
#include "sbe/generated/exchange_order/CancelOrder.h"
#include "sbe/generated/exchange_order/ReplaceOrder.h"

#include "ledger/ActiveOrder.h" // includes all other book/product files as well

namespace ledger {
    /**
     * This class is intended to read all the orders put in via gateways, and maintain the positions
     * until they are matched by the market.
     * 
     * This will simulate orders being actually matched and a chance to evaluate the efficiency of
     * various trading strategies.
     * 
     * This is intended for medium/long term trading; we cannot compete with high frequency trading
     * environments in this setup. MMs have too much of an advantage.
     * 
     */
    class Ledger : public common::IApplicationService {
        static constexpr size_t QUEUE_SIZE = 1 << 16;

    public:
        Ledger(common::CommonComponents&& components);

        void start() override final;
        void run() override final;
        void stop() override final;

        // public to support testing and archive concept
        archive::FragmentAction on_fragment(std::span<const std::byte> bytes);

    private:
        std::unique_ptr<archive::ArchiveSubscription> subscription;

        void process_new_order(const exchange::order::NewOrderSingle& order);
        void process_cancel_order(const exchange::order::CancelOrder& order);
        void process_replace_order(const exchange::order::ReplaceOrder& order);

        struct ManagedDb {
            sqlite3* db {nullptr};

            ManagedDb() {
                const int r = sqlite3_open("ledger.db", &db);
                if (r != 0) {
                    spdlog::error("Unable to initialize db!!");
                    throw std::runtime_error("Unable to initialize db!!");
                }
            }

            ~ManagedDb() {
                const int r = sqlite3_close(db);
                if (r != 0) {
                    spdlog::error("Unable to close db!!");
                }
            }
        };

        std::unique_ptr<ManagedDb> db;
        common::CommonComponents& common;

        Product& get_or_create_product(std::string_view symbol);

        absl::node_hash_map<std::string, ledger::Product> products; // stable iterators
        absl::flat_hash_map<uint64_t, ledger::ActiveOrder> active_orders;
    };
}