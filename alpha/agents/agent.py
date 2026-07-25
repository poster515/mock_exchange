from abc import ABC, abstractmethod
from typing import Any, Dict, List

from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_subscription import ArchiveSubscription

class AlpacaAgent(ABC):
    """Generic base class for Alpaca trading agents."""
    
    def __init__(self, ledger, data_source):
        self.order_publication = None
        self.md_ctrl_subscription = None
        self.md_subscription = None

    def start(self):
        self.order_publication = ArchivePublication()
        self.md_ctrl_subscription = ArchivePublication()
        self.md_subscription = ArchiveSubscription()
    
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