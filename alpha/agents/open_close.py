
import datetime
import time
from zoneinfo import ZoneInfo
from typing import Dict, List

from alpha.agents.agent import AlpacaAgent
from alpha.shared.archive_messages import *
from alpha.shared.archive_constants import *

class OpenCloseAgent(AlpacaAgent):

    def __init__(self, agent_name: str, initial_cash=10000):
        super().__init__(agent_name)
        self.cash = initial_cash
        self.positions: Dict[str, int] = {}
        self.latest_price: Dict[str, (int, int)] = {}

    def execute_strategy(self, epoch_sec: float):
        self._update_market_data()
        now = datetime.datetime.now(tz=ZoneInfo("America/New_York"))

        if now.weekday() < 5 and now.hour == 8 and now.minute == 30:
            self._on_open()

        elif now.weekday() < 5 and now.hour == 16 and now.minute == 00:
            self._on_close()

    def teardown(self):
        return super().teardown()
    
    def _on_open(self, price) -> Optional[List[OrderRequest]]:
        """Sell at market open"""
        if len(self.positions) == 0:
            return None

        orders = []
        for symbol, shares_short in self.positions.items():
            orders.append(
                OrderRequest(
                    action=OrderAction.NEW,
                    symbol=symbol,
                    quantity=shares_short,
                    client_order_id=f"OC_{int(time.time() * 1000)}_SELL",
                    side = OrderSide.Sell,
                    type = OrderType.Market
                )
            )

    def _on_close(self):
        """Buy at market close"""

        orders = []
        for symbol, shares_short in self.positions.items():
            if symbol not in self.latest_price:
                 continue

            price, price_factor = self.latest_price[symbol]
            orders.append(
                OrderRequest(
                    action=OrderAction.NEW,
                    symbol=symbol,
                    quantity=shares_short,
                    price=price,
                    price_factor=price_factor,
                    client_order_id=f"OC_{int(time.time() * 1000)}_BUY",
                    side = OrderSide.Buy,
                    type = OrderType.Market
                )
            )

    def _update_market_data(self):
        pass