
import ctypes
from typing import Optional
from enum import Enum
from dataclasses import dataclass

from alpha.shared.archive_messages import OrderType, OrderSide

MessageCallback = ctypes.CFUNCTYPE(
    ctypes.c_int,                       # return type
    ctypes.POINTER(ctypes.c_ubyte),     # uint8_t*
    ctypes.c_size_t                     # size_t
)

class OrderAction(Enum):
    """Order action types."""
    NEW = "NEW"
    CANCEL_REPLACE = "CANCEL_REPLACE"
    CANCEL = "CANCEL"

@dataclass
class OrderRequest:
    """Order request to submit via SBE."""
    action: OrderAction
    symbol: str
    quantity: int
    price: int
    price_factor: int
    order_id: Optional[str] = None
    client_order_id: Optional[str] = None
    side: OrderSide = OrderSide.Buy
    type: OrderType = OrderType.Market

class ArchiveConstants:
    archive_lib = ctypes.CDLL('/usr/local/lib/libarchive_shared.dylib')
    SBE_DEFINITION_LOCATION = "/usr/local/include/order_entry.xml"

    # SUBSCRIPTION
    archive_lib.archive_sub_create.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    archive_lib.archive_sub_create.restype = ctypes.POINTER(ctypes.c_void_p)

    archive_lib.archive_sub_close.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
    archive_lib.archive_sub_close.restype = None

    archive_lib.archive_sub_is_ready.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
    archive_lib.archive_sub_is_ready.restype = None

    archive_lib.archive_sub_poll.argtypes = [ctypes.POINTER(ctypes.c_void_p), MessageCallback]
    archive_lib.archive_sub_poll.restype = None

    archive_lib.archive_sub_destroy.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
    archive_lib.archive_sub_destroy.restype = None

    # PUBLICATIONS
    archive_lib.archive_pub_create.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    archive_lib.archive_pub_create.restype = ctypes.POINTER(ctypes.c_void_p)

    archive_lib.archive_pub_close.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
    archive_lib.archive_pub_close.restype = None

    archive_lib.archive_pub_destroy.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
    archive_lib.archive_pub_destroy.restype = None

    archive_lib.archive_pub_claim.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_size_t]
    archive_lib.archive_pub_claim.restype = ctypes.POINTER(ctypes.c_uint8)

    archive_lib.archive_pub_commit.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
    archive_lib.archive_pub_commit.restype = ctypes.c_size_t

    DEFAULT_SHM_PATH = "/tmp"
    DEFAULT_QUEUE_SIZE = 2 ** 24 # 16 MB

    ORDER_ENTRY_QUEUE = "order_entry"
    ORDER_ACK_QUEUE = "order_ack"
    MARKET_DATA_QUEUE = "market_data"
    MARKET_DATA_CTRL_RQST = "market_data_ctrl_rqst"
    MARKET_DATA_CTRL_RESP = "market_data_ctrl_resp"
    LEDGER_IN_QUEUE = "agent_to_ledger"
    LEDGER_OUT_QUEUE = "ledger_to_alpaca"

    