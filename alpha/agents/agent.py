from abc import ABC, abstractmethod
from typing import Optional

from alpha.shared.archive_constants import *
from alpha.shared.archive_messages import *
from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_subscription import ArchiveSubscription

class AlpacaAgent(ABC):
    """Generic base class for Alpaca trading agents."""
    
    def __init__(self, agent_name):
        self.order_publication = None
        self.md_ctrl_subscription = None
        self.md_subscription = None
        self.name = agent_name

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
    def execute_strategy(self) -> None:
        """Execute trading strategy."""
        pass

    @abstractmethod
    def teardown(self):
        """Teardown this agent, including cancelling any resting orders """
        pass

    def publish_order(self, signal: Optional[OrderRequest]) -> None:
        if signal is None:
            return

        # TODO: handle other order requests.
        with super().order_publication.publication_claim(NewOrderSingle) as order:
            order.orderId = signal.client_order_id
            order.symbol = signal.symbol
            order.side = signal.side
            order.orderQty = signal.quantity
            order.price = signal.price
            order.priceFactor = signal.price_factor