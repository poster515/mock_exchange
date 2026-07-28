#include "ledger/Book.h"
#include "ledger/ActiveOrder.h"

namespace ledger {

    bool Book::erase_order(ActiveOrder& order) {
        return remove_order_from_side(order.get_is_bid() ? bids : asks, order);
    }

    bool Book::add_order(ActiveOrder& order) {
        return add_order_to_side(order.get_is_bid() ? bids : asks, order);
    }

    bool Book::remove_order_from_side(std::vector<PriceLevel>& levels, ActiveOrder& order) {
        bool found = false;
        const uint64_t order_id = order.get_order_id();

        for (auto it = levels.begin(); it != levels.end(); ++it) {
            auto& level = *it;
            if (level.order_ids.contains(order_id)) {
                level.order_ids.erase(order_id);
                
                const auto qty = order.get_quantity();

                if (qty >= level.total_count) levels.erase(it);
                else level.total_count -= qty;

                found = true;
                break;
            }
        }

        return found;
    }

    bool Book::add_order_to_side(std::vector<PriceLevel>& levels, ActiveOrder& order) {
        bool found = false;
        const uint64_t order_id = order.get_order_id();

        // for (auto it = levels.begin(); it != levels.end(); ++it) {
        //     auto& level = *it;
        //     if (level.price == .contains(order_id)) {
        //         level.order_ids.erase(order_id);
                
        //         const auto qty = order.get_quantity();

        //         if (qty >= level.total_count) levels.erase(it);
        //         else level.total_count -= qty;

        //         found = true;
        //         break;
        //     }
        // }
        return found;
    }
}