from typing import Any, Dict, List, Optional
import time

from alpha.agents.agent import AlpacaAgent
from alpha.shared.archive_messages import *
from alpha.shared.archive_constants import *

class MomentumAgent(AlpacaAgent):
    """Momentum-based trading strategy using pub/sub patterns."""
    
    def __init__(self, window: int = 20, threshold: float = 0.02):
        """
        Initialize momentum agent.
        
        Args:
            window: Lookback period for momentum calculation
            threshold: Minimum momentum threshold for trading signal
        """
        self.window = window
        self.threshold = threshold
        self.prices: Dict[str, List[float]] = {} # symbol -> price history

    def execute_strategy(self):
        self._update_market_data()

        for symbol, price_history in self.prices.items():
            if len(price_history) >= self.window:
                momentum = self._calculate_momentum()
                signal = self._generate_signal(symbol, momentum)
                if signal is not None:
                    super().publish_order(signal)
    
    def _calculate_momentum(self) -> float:
        """Calculate momentum as percentage change."""
        return (self.prices[-1] - self.prices[-self.window]) / self.prices[-self.window]
    
    def _generate_signal(self, symbol: str, momentum: float) -> Optional[OrderRequest]:
        """Generate trading signal if momentum exceeds threshold."""
        if abs(momentum) > self.threshold:
            return OrderRequest(
                action=OrderAction.NEW,
                symbol=symbol,
                quantity=min(100, self.params.max_position_size - self.position_size),
                price=self.current_price - self.params.min_price_movement,
                client_order_id=f"MR_{int(time.time() * 1000)}_BUY",
                side = OrderSide.Buy
            )
        return None

    def _update_market_data(self):
        pass