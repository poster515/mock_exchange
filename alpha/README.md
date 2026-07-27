
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
│   ├── archive_publication.py
│   └── archive_subscription.py
├── market_data_bot.py
└── README.md
```

## Dependencies
- `alpaca-py` (market data & orders)
- `message_archive` (serialized publishing)
- Python `asyncio` (concurrent agent loops)

## Queue Architecture
This architecture is built around the message_transport and message_archive libraries. We communicate quickly
between processes using this IPC mechanism. Here are a list of known IPC queues and what they do:

| Queue Name | Description | Writer(s) | Reader(s) |
|:----------:|:----------|-----------:|-----------:|
| order_entry | primary order entry queue for each agent to enter orders | python agents | Ledger |
| order_ack | order acks for each order | ledger | python agents |
| market_data | market data events for subscribed symbols | market_data_bot | python agents |
| market_data_ctrl_rqst | control publication for agents to request new symbols, ignore symgols, etc | python agents | market_data_bot |
| market_data_ctrl_resp | ack channel for agents from market_data_bot | market_data_bot | python agents |
