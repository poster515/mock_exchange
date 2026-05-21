
# Alpha

Python-based trading agents that consume Alpaca market data streams and generate orders via serialized message passing.

## Architecture

### Agent Layer (This Folder)
- Multiple Python scripts, each running an independent AI trading strategy
- Consume real-time market data from Alpaca streams
- Generate `Order` messages and publish via `message_archive` library
- Strategies tracked/ranked by ledger metrics (win rate, Sharpe ratio, etc.)

### Message Flow
```
Agent Strategy → ArchivePublication (serialize)
                 ↓
              Message Queue (IPC)
                 ↓
              Ledger (back-office)
```

### Ledger Layer (External)
- Consumes serialized orders from queue in FIFO order
- Risk evaluation and compliance checks
- PnL tracking and visualization metadata
- Submits approved orders to Alpaca test framework
- Publishes metrics back for agent performance ranking

## Structure

```
alpha/
├── agents/
│   ├── momentum_bot.py
│   ├── mean_reversion_bot.py
│   └── ml_agent.py
├── shared/
│   ├── alpaca_client.py
│   └── order_publisher.py
└── README.md
```

## Dependencies
- `alpaca-py` (market data & orders)
- `message_archive` (serialized publishing)
- Python `asyncio` (concurrent agent loops)
