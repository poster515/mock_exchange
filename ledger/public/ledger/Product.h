#pragma once


#include "ledger/Book.h"

namespace ledger {

    class Product {
    public:
        Product(std::string_view symbol) 
            : exchange_symbol(symbol) {}

        Book& get_book() { return book; }
        std::string_view get_exchange_symbol() const { return exchange_symbol; }
    private:
        std::string exchange_symbol;

        Book book;
    };
}