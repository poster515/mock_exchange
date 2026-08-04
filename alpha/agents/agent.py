from abc import ABC, abstractmethod
from typing import Optional

from alpha.shared.base_agent import BaseAgent
from alpha.shared.archive_constants import *
from alpha.shared.archive_messages import *
from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_subscription import ArchiveSubscription
from alpha.shared.admin_client import AdminClient

class AlpacaAgent(BaseAgent):
    """Generic base class for Alpaca trading agents.
    
    Subclasses must implement handle_admin_bytes(bytes, size) still.
    """
    
    def __init__(self, agent_name: str):
        super().__init__(agent_name)

        self.order_publication = None
        self.md_ctrl_publication = None
        self.md_ctrl_subscription = None
        self.md_subscription = None

        self._admin_client = AdminClient(super())

    def __del__(self):
        self._admin_client.teardown()
        self.teardown()

    def start(self):
        self.order_publication = ArchivePublication(ArchiveConstants.LEDGER_IN_QUEUE, super())
        self.md_ctrl_publication = ArchivePublication(ArchiveConstants.MARKET_DATA_CTRL_RQST, super())
        self.md_ctrl_subscription = ArchiveSubscription(ArchiveConstants.MARKET_DATA_CTRL_RESP, super())
        self.md_subscription = ArchiveSubscription(ArchiveConstants.MARKET_DATA_QUEUE, super())

        self._admin_client.start()

    @abstractmethod
    def execute_strategy(self, epoch_sec: float) -> None:
        """Execute trading strategy."""
        self._admin_client.poll()

    @property
    def admin_client(self):
        return self._admin_client

    def teardown(self):
        """Teardown this agent, including cancelling any resting orders """
        self.order_publication.publication_close()
        self.md_ctrl_publication.publication_close()
        self.md_ctrl_subscription.subscription_close()
        self.md_subscription.subscription_close()

    def publish_order(self, signal: Optional[OrderRequest]) -> bool:
        if signal is None:
            return False

        match signal.action:
            case OrderAction.NEW:
                with self.order_publication.publication_claim(NewOrderSingle) as order:
                    order.orderId = signal.client_order_id
                    order.symbol = signal.symbol
                    order.side = signal.side
                    order.orderQty = signal.quantity
                    order.price = signal.price
                    order.priceFactor = signal.price_factor
                return True

            case OrderAction.REPLACE:
                with self.order_publication.publication_claim(CancelOrder) as order:
                    order.orderId = signal.client_order_id
                    order.symbol = signal.symbol
                    order.side = signal.side
                    order.orderQty = signal.quantity
                    order.price = signal.price
                    order.priceFactor = signal.price_factor
                return True

            case OrderAction.CANCEL:
                with self.order_publication.publication_claim(ReplaceOrder) as order:
                    order.orderId = signal.client_order_id
                    order.symbol = signal.symbol
                    order.side = signal.side
                    order.orderQty = signal.quantity
                    order.price = signal.price
                    order.priceFactor = signal.price_factor
                return True

        return False
