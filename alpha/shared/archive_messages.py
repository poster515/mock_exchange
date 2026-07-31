from ctypes import *
from enum import IntEnum
import os


# =============================================================================
# SBE Message Header
# =============================================================================

class MessageHeader(Structure):
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("blockLength", c_uint16),
        ("templateId",  c_uint16),
        ("schemaId",    c_uint16),
        ("version",     c_uint16),
    ]


# =============================================================================
# Enums
# =============================================================================

class OrderSide(IntEnum):
    Buy = 1
    Sell = 2


class OrderType(IntEnum):
    Market = 1
    Limit = 2


class TimeInForce(IntEnum):
    Day = 0
    IOC = 3

# =============================================================================
# Messages between python and C++ clients
# =============================================================================

class NewOrderSingle(Structure):
    TEMPLATE_ID = 1
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("header",       MessageHeader),
        ("orderId",      c_uint64),
        ("symbolId",     c_uint64),
        ("side",         c_uint8),
        ("orderQty",     c_uint32),
        ("ordType",      c_uint8),
        ("price",        c_int32),
        ("priceFactor",  c_int32),
        ("timeInForce",  c_uint8),
    ]


class CancelOrder(Structure):
    TEMPLATE_ID = 2
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("header",      MessageHeader),
        ("orderId",     c_uint64),
        ("symbolId",    c_uint64),
        ("side",        c_uint8),
    ]


class ReplaceOrder(Structure):
    TEMPLATE_ID = 3
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("header",          MessageHeader),
        ("orderId",         c_uint64),
        ("newQty",          c_uint32),
        ("newPrice",        c_int32),
        ("newPriceFactor",  c_int32),
    ]


class NewSymbolAdd(Structure):
    TEMPLATE_ID = 100
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("header",          MessageHeader),
        ("symbolName",      c_char * 8),
        ("symbolId",        c_uint64)
    ]

# only really ever communicated between python clients
class MarketData(Structure):
    TEMPLATE_ID = 4
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("header",          MessageHeader),
        ("orderId",         c_uint64),
        ("newQty",          c_uint32),
        ("newPrice",        c_int32),
        ("newPriceFactor",  c_int32),
    ]


class MarketDataCtrlRequest(Structure):
    TEMPLATE_ID = 5
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("header",          MessageHeader),
        ("agentName",       c_char * 32),
        ("symbolName",      c_char * 8),
        ("endTime",         c_uint64)
    ]

    
class MarketDataCtrlResponse(Structure):
    TEMPLATE_ID = 6
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("header",          MessageHeader),
        ("symbolName",      c_char * 8),
        ("symbolId",        c_uint64),
        ("endTime",         c_uint64)
    ]
