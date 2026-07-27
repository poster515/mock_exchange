from alpaca.data.requests import StockLatestQuoteRequest
from alpaca.trading.client import TradingClient
from alpaca.trading.requests import MarketOrderRequest
from alpaca.trading.enums import OrderSide, TimeInForce

class AlpacaClient:
    """
    Polls the ledger outbound queue and submits new orders to alpaca
    """
    def __init__(self, api_key: str, secret_key: str):
        self.trading_client = TradingClient(api_key, secret_key, paper=True) # MUST HAVE THIS BOOL HERE

    def submit_order_to_alpaca(self):
        market_order_data = MarketOrderRequest(
                    symbol="SPY",
                    qty=0.023,
                    side=OrderSide.BUY,
                    time_in_force=TimeInForce.DAY
                )
        # Market order
        market_order = self.trading_client.submit_order(order_data=market_order_data)
