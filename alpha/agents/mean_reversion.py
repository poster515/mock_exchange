import logging
from dataclasses import dataclass
from typing import Optional
import time

from alpha.agents.agent import AlpacaAgent
from alpha.shared.archive_messages import *
from alpha.shared.archive_constants import *

"""Mean Reversion Trading Client for Alpaca Market Data with SBE Order Submission."""


logger = logging.getLogger(__name__)

@dataclass
class MeanReversionParams:
    """Mean reversion trading parameters."""
    z_score_threshold: float = 2.0
    min_price_movement: float = 0.01
    max_position_size: int = 1000
    target_profit_pips: float = 0.05
    stop_loss_pips: float = 0.10
    lookback_periods: int = 20
    volume_threshold: int = 100000


class MeanReversionClient(AlpacaAgent):
    """Mean reversion trading client with Alpaca market data integration."""

    def __init__(
        self,
        market_data_client,
        symbol: str = "AAPL",
        params: Optional[MeanReversionParams] = None,
    ):
        """
        Initialize the mean reversion client.

        Args:
            market_data_subscription: ArchiveSubscription for polling data (written by separate service)
            archive_publication: ArchivePublication for SBE order submission
            symbol: Trading symbol
            params: MeanReversionParams configuration
        """
        self.market_data_client = market_data_client
        self.symbol = symbol
        self.params = params or MeanReversionParams()
        
        self.current_price: float = 0.0
        self.current_volume: int = 0
        self.price_history: list[float] = []
        self.position_size: int = 0
        self.active_order_id: Optional[str] = None
        self.last_order_time: float = 0.0
        self.min_order_interval: float = 0.5
        
        logger.info(f"Initialized MeanReversionClient for {symbol}")

    def execute_strategy(self, epoch_sec: float):
        """
        Execute one polling cycle. Called by external runner.
        """
        try:
            # 1. Fetch latest market data
            market_data = self.market_data_client.get_latest_data(self.symbol)
            if not market_data:
                logger.warning(f"No market data available for {self.symbol}")
                return None

            self._update_market_state(market_data)

            # 2. Calculate mean reversion signals
            signal = self._calculate_mr_signal()
            if signal is None:
                return None

            # 3. Check rate limiting
            if not self._check_order_rate_limit():
                return None

            # 4. Generate and submit order requests
            signal = self._generate_order_request(signal)
            if signal is not None:
                super().publish_order(signal)

        except Exception as e:
            logger.error(f"Error in polling cycle: {e}")
            return None

    def _update_market_state(self, market_data: dict) -> None:
        """Update internal market state from latest data."""
        self.current_price = market_data.get("price", 0.0)
        self.current_volume = market_data.get("volume", 0)
        
        if self.current_price > 0:
            self.price_history.append(self.current_price)
            # Keep only recent price history for lookback calculation
            if len(self.price_history) > self.params.lookback_periods:
                self.price_history.pop(0)

    def _calculate_mr_signal(self) -> Optional[int]:
        """
        Calculate mean reversion signal.
        
        Returns:
            1 for buy signal, -1 for sell signal, None for no signal
        """
        if len(self.price_history) < self.params.lookback_periods:
            return None

        # Check volume threshold
        if self.current_volume < self.params.volume_threshold:
            logger.debug(f"Volume {self.current_volume} below threshold")
            return None

        # Calculate statistics
        mean_price = sum(self.price_history) / len(self.price_history)
        variance = sum((p - mean_price) ** 2 for p in self.price_history) / len(self.price_history)
        std_dev = variance ** 0.5
        
        if std_dev == 0:
            return None

        # Z-score calculation
        z_score = (self.current_price - mean_price) / std_dev

        # Mean reversion logic: buy when price is significantly below mean
        if z_score < -self.params.z_score_threshold and self.position_size < self.params.max_position_size:
            logger.info(f"BUY signal: Z-score={z_score:.2f}, Price={self.current_price}")
            return 1

        # Sell when price is significantly above mean
        if z_score > self.params.z_score_threshold and self.position_size > 0:
            logger.info(f"SELL signal: Z-score={z_score:.2f}, Price={self.current_price}")
            return -1

        return None

    def _check_order_rate_limit(self) -> bool:
        """Enforce minimum order interval to prevent excessive trading."""
        current_time = time.time()
        if current_time - self.last_order_time < self.min_order_interval:
            return False
        return True

    def _generate_order_request(self, signal: int) -> Optional[OrderRequest]:
        """
        Generate order request based on signal.
        
        Args:
            signal: 1 for buy, -1 for sell
            
        Returns:
            OrderRequest or None
        """
        if signal == 1:  # BUY
            quantity = min(100, self.params.max_position_size - self.position_size)
            price = self.current_price - self.params.min_price_movement
            
            return OrderRequest(
                action=OrderAction.NEW,
                symbol=self.symbol,
                quantity=quantity,
                price=price,
                client_order_id=f"MR_{int(time.time() * 1000)}_BUY",
                side=OrderSide.Buy,
                type=OrderType.Limit
            )

        elif signal == -1:  # SELL
            if self.active_order_id:
                # Cancel existing buy order and submit sell
                return OrderRequest(
                    action=OrderAction.REPLACE,
                    symbol=self.symbol,
                    quantity=self.position_size,
                    price=self.current_price + self.params.min_price_movement,
                    order_id=self.active_order_id,
                    client_order_id=f"MR_{int(time.time() * 1000)}_SELL",
                    side=OrderSide.Sell,
                )
            else:
                # Direct sell if we have position
                return OrderRequest(
                    action=OrderAction.NEW,
                    symbol=self.symbol,
                    quantity=self.position_size,
                    price=self.current_price + self.params.min_price_movement,
                    client_order_id=f"MR_{int(time.time() * 1000)}_SELL",
                    side=OrderSide.Sell,
                    type=OrderType.Limit
                )

        return None
