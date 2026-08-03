import os
import ctypes
from typing import Optional
from enum import Enum
from dataclasses import dataclass
from pathlib import Path
import platform


from alpha.shared.archive_messages import OrderType, OrderSide

MessageCallback = ctypes.CFUNCTYPE(
    ctypes.c_ubyte,                     # return type
    ctypes.POINTER(ctypes.c_ubyte),     # uint8_t*
    ctypes.c_size_t                     # size_t
)

class OrderAction(Enum):
    """Order action types."""
    NEW = "NEW"
    REPLACE = "REPLACE"
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
    INSTALL_PATH = Path(os.environ.get("ARCHIVE_LIB_DIR", "/usr/local/lib"))
    TARGET_EXTENSION = "dylib" if "macOS" in platform.platform() else "so"
    archive_lib = ctypes.CDLL(os.path.join(INSTALL_PATH, f"libarchive_shared.{TARGET_EXTENSION}"))

    archive_lib.archive_force_close.argtypes = [ctypes.c_char_p]
    archive_lib.archive_sub_close.restype = None

    # SUBSCRIPTION
    archive_lib.archive_sub_create.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p]
    archive_lib.archive_sub_create.restype = ctypes.c_void_p

    archive_lib.archive_sub_close.argtypes = [ctypes.c_void_p]
    archive_lib.archive_sub_close.restype = None

    archive_lib.archive_sub_is_ready.argtypes = [ctypes.c_void_p]
    archive_lib.archive_sub_is_ready.restype = None

    archive_lib.archive_sub_poll.argtypes = [ctypes.c_void_p, MessageCallback]
    archive_lib.archive_sub_poll.restype = None

    archive_lib.archive_sub_destroy.argtypes = [ctypes.c_void_p]
    archive_lib.archive_sub_destroy.restype = None

    # PUBLICATIONS
    archive_lib.archive_pub_create.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p]
    archive_lib.archive_pub_create.restype = ctypes.c_void_p

    archive_lib.archive_pub_close.argtypes = [ctypes.c_void_p]
    archive_lib.archive_pub_close.restype = None

    archive_lib.archive_pub_destroy.argtypes = [ctypes.c_void_p]
    archive_lib.archive_pub_destroy.restype = None

    archive_lib.archive_pub_claim.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    archive_lib.archive_pub_claim.restype = ctypes.POINTER(ctypes.c_uint8)

    archive_lib.archive_pub_commit.argtypes = [ctypes.c_void_p]
    archive_lib.archive_pub_commit.restype = ctypes.c_size_t

    DEFAULT_SHM_PATH = Path("/")
    DEFAULT_QUEUE_SIZE = 2 ** 24 # 16 MB

    ORDER_ENTRY_QUEUE = "order_entry"
    ORDER_ACK_QUEUE = "order_ack"
    MARKET_DATA_QUEUE = "market_data"
    MARKET_DATA_CTRL_RQST = "market_data_ctrl_rqst"
    MARKET_DATA_CTRL_RESP = "market_data_ctrl_resp"
    LEDGER_IN_QUEUE = "agent_to_ledger"
    LEDGER_OUT_QUEUE = "ledger_to_alpaca"
    ADMIN_OUT_QUEUE = "admin_in"
    ADMIN_IN_QUEUE = "admin_out"

    