class OpenCloseAgent:
    def __init__(self, initial_cash=10000):
        self.cash = initial_cash
        self.position = 0
        self.trades = []
    
    def on_open(self, price):
        """Buy at market open"""
        shares = self.cash / price
        self.position = shares
        self.cash = 0
        self.trades.append({"action": "BUY", "price": price, "shares": shares})
    
    def on_close(self, price):
        """Sell at market close"""
        proceeds = self.position * price
        self.cash = proceeds
        self.trades.append({"action": "SELL", "price": price, "shares": self.position})
        self.position = 0
    
    def get_pnl(self):
        """Calculate profit/loss"""
        return self.cash - 10000