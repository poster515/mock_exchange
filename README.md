
# C++ Market Structure

A modular, testable market microstructure implementation with gateways, sequencers, matching engine, and market data distribution.

## Architecture

### Core Components

- **Order Entry Gateways**: Accept client orders via FIX protocol subset
- **Sequencer**: Centralizes order stream with deterministic ordering
- **Matching Engine**: Executes trades with configurable matching rules
- **Market Data Gateway**: Distributes trades, quotes, and market snapshots
- **Message Transport**: SPSC IPC queue layer (MPSC roadmap)

### Process Communication

```
Client Gateways → Sequencer → Matching Engine → Market Data Gateway
    ↓
  (FIX messages via IPC queues)
```

## Getting Started

I'm using cmake 4.2 and conan2 for this project. Here is the setup for in-tree builds:

```bash
mkdir -p build
conan install . --output-folder build --build=missing
cmake -B build/build/Debug -DCMAKE_TOOLCHAIN_FILE=./build/build/Debug/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug .
```

Obviously you'll want to confirm your choice of build type and folder location, but these will get you started.

## Testing & Scenarios

- Unit tests for order validation and matching logic
- Integration tests for end-to-end order flow
- Performance benchmarks for throughput and latency

## Future: Agentic Market Competition

### Vision

Enable AI agents to compete in simulated market environments:

- **Agent Framework**: Pluggable strategy interface for order submission/cancellation
- **Time Windows**: Replay historical order books or synthetic scenarios
- **Competition Modes**:
  - Liquidity competition (tightest spreads)
  - Market making (inventory management vs. spreads)
  - Arbitrage detection (cross-venue strategies)
- **Metrics Engine**: Track P&L, fill rates, market impact, volatility
- **Backtesting**: Deterministic replay with configurable agent populations
- **Live Mode**: Deploy top-performing agents to real gateways

### Example: Market Making Tournament

Run 10 agents with varying risk parameters over 1-hour windows, replay different market conditions (high volatility, flash crashes), rank by risk-adjusted returns.
