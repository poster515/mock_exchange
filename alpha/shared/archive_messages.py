from ctypes import *
from enum import IntEnum


# =============================================================================
# SBE Message Header
# =============================================================================

class MessageHeader(Structure):
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
# Messages
# =============================================================================

class NewOrderSingle(Structure):
    TEMPLATE_ID = 1
    _pack_ = 1
    _fields_ = [
        ("header",       MessageHeader),
        ("orderId",      c_uint64),
        ("symbol",       c_char * 8),
        ("side",         c_uint8),
        ("orderQty",     c_uint32),
        ("ordType",      c_uint8),
        ("price",        c_int32),
        ("priceFactor",  c_int32),
        ("timeInForce",  c_uint8),
    ]


class CancelOrder(Structure):
    TEMPLATE_ID = 2
    _pack_ = 1
    _fields_ = [
        ("header",  MessageHeader),
        ("orderId", c_uint64),
        ("symbol",  c_char * 8),
        ("side",    c_uint8),
    ]


class ReplaceOrder(Structure):
    TEMPLATE_ID = 3
    _pack_ = 1
    _fields_ = [
        ("header",          MessageHeader),
        ("orderId",         c_uint64),
        ("newQty",          c_uint32),
        ("newPrice",        c_int32),
        ("newPriceFactor",  c_int32),
    ]