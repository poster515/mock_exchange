#pragma once

#include <vector>
#include <absl/container/flat_hash_set.h>

namespace ledger {

    class ActiveOrder;

    struct PriceLevel {
        uint64_t price;
        size_t price_factor;
        absl::flat_hash_set<uint64_t> order_ids;
        size_t total_count { 0 };   // count of shares/contracts not number of orders
    };

    class Book {
    public:

        bool erase_order(ActiveOrder& order);
        bool add_order(ActiveOrder& order);

    private:
        bool remove_order_from_side(std::vector<PriceLevel>& levels, ActiveOrder& order);
        bool add_order_to_side(std::vector<PriceLevel>& levels, ActiveOrder& order);
        
        // store BBOs at front
        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;
    };
}