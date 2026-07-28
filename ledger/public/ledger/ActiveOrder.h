#pragma once

#include <string>
#include <vector>
#include <math.h>
#include <format>
#include <absl/container/flat_hash_set.h>

#include "ledger/Product.h"

namespace ledger {

    class ActiveOrder {
    public:
        ActiveOrder(Product& product, uint64_t order_id, uint64_t price, size_t price_factor, size_t quantity, bool is_bid)
            : product(product)
            , order_id(order_id)
            , unscaled_price(price)
            , scaling_factor(price_factor)
            , is_bid(is_bid) {
        }

        uint64_t get_order_id() const { return order_id; }
        size_t get_quantity() const { return quantity; }
        bool get_is_bid() const { return is_bid; }
        uint64_t get_unscaled_price() const { return unscaled_price; }
        double get_scaled_price() const { return std::pow(10, scaling_factor) * unscaled_price; }
        Product& get_product() const { return product; }

    protected:
        Product& product;
        uint64_t order_id;
        uint64_t unscaled_price;
        size_t scaling_factor;
        size_t quantity;
        bool is_bid;
    };

    // template <>
    // struct std::formatter<ActiveOrder> {

    //     constexpr auto parse(std::format_parse_context& ctx) {
    //         return ctx.begin();
    //     }

    //     auto format(const ActiveOrder& order, std::format_context& ctx) const {
    //         return std::format_to("Order #{} for {}: {} {}x ${}"
    //             , order.get_order_id()
    //             , order.get_product().get_exchange_symbol()
    //             , order.get_is_bid() ? "BID" : "ASK"
    //             , order.get_quantity()
    //             , order.get_scaled_price());
    //     }
    // };
}