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
        ("timestamp",   c_uint64),
        ("srcAgentId",  c_uint64),
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
#-------------------- Admin Messages ------------------------
# sent from some kind of controller app - could be Ledger tbh not sure yet.

class NewSymbolAdd(Structure):
    TEMPLATE_ID = 100
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("header",              MessageHeader),
        ("symbolName",          c_char * 8),
        ("symbolId",            c_uint64),
        ("createTimeEpochNs",   c_uint64)
    ]

class AgentShutdown(Structure):
    TEMPLATE_ID = 101
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("header",          MessageHeader),
        ("destAgentId",     c_uint64),
        ("destAgentName",   c_char * 64),
        ("reason",          c_char * 32),
    ]

class AllAgentsShutdown(Structure):
    TEMPLATE_ID = 102
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("header",      MessageHeader),
        ("reason",      c_char * 32),
    ]

# originating agent leaves agentID in header blank
class AgentStartup(Structure):
    TEMPLATE_ID = 103
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("header",      MessageHeader),
        ("srcAgentName", c_char * 64),      # so the admin controller can know who the sender is
    ]

# central controller sends this back, including new agent Id
class AgentParams(Structure):
    TEMPLATE_ID = 104
    _layout_ = "ms"
    _pack_ = 1
    _fields_ = [
        ("header",              MessageHeader),
        ("agentId",             c_uint64),
        ("total_cash",          c_int32),
        ("total_loss_limit",    c_int32),   # how much you lose before we shut you down
        ("total_notional",      c_int32),   # how much you can have outstanding
    ]


#-------------------- Market Data Messages ------------------------
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
