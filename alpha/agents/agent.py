from abc import ABC, abstractmethod
from typing import Any, Dict, List
import os

from alpha.shared.archive_constants import ArchiveConstants
from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_subscription import ArchiveSubscription

class AlpacaAgent(ABC):
    """Generic base class for Alpaca trading agents."""
    
    def __init__(self):
        self.order_publication = None
        self.md_ctrl_subscription = None
        self.md_subscription = None

    def start(self):
        self.order_publication = ArchivePublication(ArchiveConstants.LEDGER_IN_QUEUE)
        self.md_ctrl_publication = ArchivePublication(ArchiveConstants.MARKET_DATA_CTRL_RQST)
        self.md_ctrl_subscription = ArchiveSubscription(ArchiveConstants.MARKET_DATA_CTRL_RESP)
        self.md_subscription = ArchiveSubscription(ArchiveConstants.MARKET_DATA_QUEUE)

        self.order_publication.publication_open(ArchiveConstants.DEFAULT_QUEUE_SIZE)
        self.md_ctrl_publication.publication_open(ArchiveConstants.DEFAULT_QUEUE_SIZE)
        self.md_ctrl_subscription.subscription_open(ArchiveConstants.DEFAULT_QUEUE_SIZE)
        self.md_subscription.subscription_open(ArchiveConstants.DEFAULT_QUEUE_SIZE)
    
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
        market_data = self.md_subscription.poll_market_data()
        self.on_market_data(market_data)
        self.execute_strategy()