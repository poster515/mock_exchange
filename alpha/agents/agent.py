from abc import ABC, abstractmethod
from typing import Any, Dict, List
import os

from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_subscription import ArchiveSubscription

class AlpacaAgent(ABC):
    """Generic base class for Alpaca trading agents."""

    DEFAULT_SHM_PATH = "/"

    ORDER_ENTRY_QUEUE = "order_entry"
    ORDER_ACK_QUEUE = "order_ack"
    MARKET_DATA_QUEUE = "market_data"
    MARKET_DATA_CTRL_RQST = "market_data_ctrl_rqst"
    MARKET_DATA_CTRL_RESP = "market_data_ctrl_resp"

    DEFAULT_QUEUE_SIZE = 2 ** 24 # 16 MB
    
    def __init__(self, ledger, data_source):
        self.order_publication = None
        self.md_ctrl_subscription = None
        self.md_subscription = None

    def start(self):
        self.order_publication = ArchivePublication()
        self.md_ctrl_subscription = ArchivePublication()
        self.md_subscription = ArchiveSubscription()

        self.order_publication.publication_open(
            os.path.join(AlpacaAgent.DEFAULT_SHM_PATH, AlpacaAgent.ORDER_ENTRY_QUEUE),
            AlpacaAgent.DEFAULT_QUEUE_SIZE)

        # TODO: open rest of pubs
    
    @abstractmethod
    def on_market_data(self, data: Dict[str, Any]) -> None:
        """Handle incoming market data."""
        pass
    
    @abstractmethod
    def execute_strategy(self) -> None:
        """Execute trading strategy."""
        pass
    
    def run(self) -> None:
        """Main agent loop."""
        market_data = self.subscription.poll_market_data()
        self.on_market_data(market_data)
        self.execute_strategy()