
#include <string>
#include <vector>

#include <absl/container/flat_hash_set.h>

namespace ledger {
    class Product;
    class ActiveOrder {
    public:
        ActiveOrder(Product& product, uint64_t order_id, uint64_t price, size_t price_factor)
            : product(product)
            , order_id(order_id)
            , unscaled_price(price)
            , scaling_factor(price_factor) {
        }

    protected:
        Product& product;
        uint64_t order_id;
        uint64_t unscaled_price;
        size_t scaling_factor;
    };

    struct PriceLevel {
        uint64_t price;
        size_t price_factor;
        absl::flat_hash_set<uint64_t> order_ids;
    };

    class Book {
    public:
        
        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;
    };

    class Product {
    public:
        Product(std::string_view symbol) 
            : exchange_symbol(symbol) {}
    private:
        std::string exchange_symbol;

        Book book;
    };
}