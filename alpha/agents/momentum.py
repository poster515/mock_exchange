from typing import Any, Dict, List
import numpy as np

class MomentumAgent:
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
        self.prices: List[float] = []
        self.subscribers: List[callable] = []
    
    def subscribe(self, callback: callable) -> None:
        """Subscribe to trading signals."""
        self.subscribers.append(callback)
    
    def publish(self, signal: Dict[str, Any]) -> None:
        """Publish trading signals to subscribers."""
        for callback in self.subscribers:
            callback(signal)
    
    def update(self, price: float) -> None:
        """Update with new price and check for momentum signal."""
        self.prices.append(price)
        
        if len(self.prices) >= self.window:
            momentum = self._calculate_momentum()
            signal = self._generate_signal(momentum)
            if signal is not None:
                self.publish(signal)
    
    def _calculate_momentum(self) -> float:
        """Calculate momentum as percentage change."""
        return (self.prices[-1] - self.prices[-self.window]) / self.prices[-self.window]
    
    def _generate_signal(self, momentum: float) -> Dict[str, Any] | None:
        """Generate trading signal if momentum exceeds threshold."""
        if abs(momentum) > self.threshold:
            return {
                "action": "BUY" if momentum > 0 else "SELL",
                "momentum": momentum,
                "strength": abs(momentum)
            }
        return None