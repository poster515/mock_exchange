#pragma once

#include <cstdint>


#include "generated/exchange_order/MessageHeader.h"
#include "generated/exchange_order/NewOrderSingle.h"

namespace exchange::order {

class FixSbeDecoder
{
public:



    static bool decode_new_order(
        const char* buffer,
        size_t length,
        DecodedNewOrder& out)
    {
        MessageHeader hdr;

        hdr.wrap(const_cast<char*>(buffer), 0, 0, length);

        if (hdr.templateId() != NewOrderSingle::sbeTemplateId())
            return false;

        NewOrderSingle order;

        order.wrapForDecode(
            const_cast<char*>(buffer),
            MessageHeader::encodedLength(),
            hdr.blockLength(),
            hdr.version(),
            length);

        out.order_id = order.orderId();
        out.qty = order.orderQty();
        out.price = order.price();
        out.side = order.side();

        order.getSymbol(out.symbol.data(), out.symbol.size());

        return true;
    }
};



}