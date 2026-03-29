#pragma once

#include <array>
#include <cstdint>
#include "sbe/generated/exchange_order/MessageHeader.h"
#include "sbe/generated/exchange_order/NewOrderSingle.h"

namespace exchange::order {

struct EncodedMessage
{
    const char* data;
    size_t length;
};

class SbeEncoder
{
public:

    static EncodedMessage encode_new_order(
        char* buffer,
        uint64_t order_id,
        const char* symbol,
        uint32_t qty,
        int64_t price,
        Side::Value side)
    {
        constexpr uint16_t SCHEMA_ID = 1;
        constexpr uint16_t VERSION = 1;

        auto* hdr = reinterpret_cast<MessageHeader*>(buffer);

        hdr->wrap(buffer, 0, 0, NewOrderSingle::sbeBlockLength());
        hdr->blockLength(NewOrderSingle::sbeBlockLength());
        hdr->templateId(NewOrderSingle::sbeTemplateId());
        hdr->schemaId(SCHEMA_ID);
        hdr->version(VERSION);

        NewOrderSingle order;

        order.wrapForEncode(
            buffer,
            MessageHeader::encodedLength(),
            sizeof(buffer)
        );

        order.orderId(order_id);
        order.side(side);
        order.orderQty(qty);
        order.price(price);

        order.putSymbol(symbol);

        size_t len =
            MessageHeader::encodedLength() +
            order.encodedLength();

        return {buffer, len};
    }
};

}