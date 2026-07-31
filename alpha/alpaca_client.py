import logging
from typing import List, Optional
from datetime import datetime
import json
import ctypes

from alpaca.data.requests import StockLatestQuoteRequest
from alpaca.trading.client import TradingClient
from alpaca.trading.requests import MarketOrderRequest
from alpaca.trading.enums import OrderSide, TimeInForce

from alpha.shared.archive_messages import *
from alpha.shared.archive_constants import ArchiveConstants
from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_subscription import ArchiveSubscription


class MarketDataClient:
    """Client for querying Alpaca market data and publishing to archive."""

    DEFAULT_BATCH_SIZE = 10 # batch size when polling subscriptions
    
    def __init__(self, api_key: str):
        """
        Initialize MarketDataClient.
        
        Args:
            api_key: Alpaca API key
            archive_publisher: Publisher instance for archive
        """
        self.api_key = api_key
        self.logger = logging.getLogger(__name__)
        self.md_publication = None  # archive we write to with market data updates
        self.ctrl_publication = None  # archive we write to with market data updates
        self.ctrl_subscription = None # archive we poll with control requests
        self.ledger_subscription = None
    
    def start(self):
        self.md_publication = ArchivePublication(ArchiveConstants.MARKET_DATA_QUEUE, "alpaca_client")
        self.ctrl_publication = ArchivePublication(ArchiveConstants.MARKET_DATA_CTRL_RESP, "alpaca_client")
        self.ctrl_subscription = ArchiveSubscription(ArchiveConstants.MARKET_DATA_CTRL_RQST, "alpaca_client")
        self.ledger_subscription = ArchiveSubscription(ArchiveConstants.LEDGER_OUT_QUEUE, "alpaca_client")

    def run(self):
        self._check_and_respond_control_pub()
        self._query_and_publish_market_data()
        self._query_and_submit_ledger_orders()

    def _check_and_respond_control_pub(self):
        # TODO: check the control subscription for new symbols to track, symbols to ignore, etc
            # Then respond on the control publication to applicable agents
        self.ctrl_subscription.poll_subscription()

    def _query_and_publish_market_data(self, symbols: List[str]) -> None:
        # TODO: query market data for tracked symbols and publish any results to market data publication
        market_data = self._query_market_data(symbols)
        self._publish_market_data(market_data)

    def _query_and_submit_ledger_orders(self):
        # TODO: check the ledger subscription for new orders that the ledger deems acceptable
            # then respond over the 
        pass
    
    def _query_market_data(self, symbols: List[str]) -> Optional[dict]:
        """
        Query Alpaca market data API for given symbols.
        
        Args:
            symbols: List of stock symbols (e.g., ["AAPL", "MSFT"])
            
        Returns:
            Market data dict or None on failure
        """
        self.logger.info(f"Querying market data for symbols: {symbols}")
        
        # TODO: Implement Alpaca API call
        # Use alpaca_trade_api or requests to fetch data
        # Example endpoint: https://data.alpaca.markets/v1beta3/latest/bars
        market_data = {}
        return market_data
    
    def _serialize_market_data(self, symbol: str, price: float, 
                            volume: int, bid: float, ask: float) -> bytes:
        """
        Serialize market data using SBE schema.
        
        Args:
            symbol: Stock symbol
            price: Current price
            volume: Trading volume
            bid: Bid price
            ask: Ask price
            
        Returns:
            Serialized bytes
        """
        data = {
            "symbol": symbol,
            "price": price,
            "volume": volume,
            "timestamp": int(datetime.now().timestamp() * 1000),
            "bid": bid,
            "ask": ask,
        }
        # TODO: Implement SBE serialization (use sbe-python or similar)
        return json.dumps(data).encode()
    
    def _publish_market_data(self, symbol: str, price: float,
                           volume: int, bid: float, ask: float) -> bool:
        """
        Publish market data to archive.
        
        Args:
            symbol: Stock symbol
            price: Current price
            volume: Trading volume
            bid: Bid price
            ask: Ask price
            
        Returns:
            True if published successfully
        """
        if not self.publisher:
            self.logger.error("No publisher configured")
            return False
        
        serialized = self._serialize_market_data(symbol, price, volume, bid, ask)
        
        try:
            self.publisher.publish("market_data", serialized)
            self.logger.info(f"Published market data for {symbol}")
            return True
        except Exception as e:
            self.logger.error(f"Failed to publish: {e}")
            return False

    def _on_archive_data(self, bytes, size):
        print(f"Received {size} bytes from poll: {bytes}")
        assert(size >= ctypes.sizeof(MessageHeader))

        header = ctypes.cast(bytes, ctypes.POINTER(MessageHeader)).contents
        match header.templateId:
            case NewOrderSingle.TEMPLATE_ID:
                full_message = ctypes.cast(bytes, ctypes.POINTER(NewOrderSingle)).contents

            case CancelOrder.TEMPLATE_ID:
                full_message = ctypes.cast(bytes, ctypes.POINTER(CancelOrder)).contents

            case ReplaceOrder.TEMPLATE_ID:
                full_message = ctypes.cast(bytes, ctypes.POINTER(ReplaceOrder)).contents

            case MarketData.TEMPLATE_ID:
                full_message = ctypes.cast(bytes, ctypes.POINTER(MarketData)).contents

            case MarketDataCtrlRequest.TEMPLATE_ID:
                full_message = ctypes.cast(bytes, ctypes.POINTER(MarketDataCtrlRequest)).contents

            case MarketDataCtrlResponse.TEMPLATE_ID:
                full_message = ctypes.cast(bytes, ctypes.POINTER(MarketDataCtrlResponse)).contents
        


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    client = MarketDataClient(api_key="your-api-key")
    # client.fetch_and_publish(["AAPL", "MSFT"])