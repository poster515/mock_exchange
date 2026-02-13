# Gateway - Multi-Client Nearly-FIX Order Gateway

Purpose
- Provide a lightweight, high-throughput gateway that accepts client connections, parses nearly-FIX-compatible incoming orders, normalizes them, and emits internal order events for a separate matching engine.
- Support basic order flow: New/Add, Update/Replace, Cancel, Mass Cancel, and session-level messages (logon/heartbeat/logout).

Goals
- Clear separation of transport/session handling and order processing.
- Small, testable core that can be wired to multiple transports (TCP, Unix domain sockets, WebSocket, etc.).
- Deterministic, low-latency event dispatch to a matching engine via a simple internal API.
- Robustness: idempotency, sequencing, backpressure, and basic fault isolation per client.

High-level architecture
- Clients -> Transport Layer (per-client session) -> Parser/Validator -> Normalizer -> Internal Event Queue -> Dispatcher -> Matching Engine (separate library)
- Optional components: Auth, Rate limiter, Audit/Replay log, Metrics

Supported message categories (nearly-FIX)
- Session messages: Logon, Heartbeat, TestRequest, Logout
- Order messages (expected fields, approximate FIX tags):
    - New Order / Add: clOrdID, side, ordType, price, orderQty, symbol, senderCompID/timestamp
    - Replace/Update: clOrdID, origClOrdID, changes (qty/price/other)
    - Cancel: clOrdID, origClOrdID, symbol
    - Mass Cancel: massCancelType, symbol/filters
- Validation: required fields, basic business rules (positive qty, allowed sides/types)

Internal order event model (recommended)
- Minimal, transport-agnostic C++ struct example:
    ```cpp
    struct OrderEvent {
            enum Type { New, Replace, Cancel, MassCancel, Session } type;
            std::string client_id;    // session identifier
            std::string cl_ord_id;
            std::string orig_cl_ord_id; // for Replace/Cancel
            std::string symbol;
            double price;
            uint64_t qty;
            std::string side; // "BUY"/"SELL"
            uint64_t recv_ts_ns;
            // additional metadata: seq, raw_msg_id, source transport
    };
    ```
- Events must be immutable after creation and small enough to pass through lock-free queues.

Client/session model
- Per-connection session object with:
    - Session state (LoggedOut / LoggedIn / Recovering)
    - Per-session incoming sequence numbers and optional recovery mechanism
    - Per-session rate-limiter and backpressure signaling
    - Isolation: failures in one session shouldn't block others

Transport and parser
- Parser should tolerate near-FIX variants; map incoming fields into OrderEvent.
- Validate and enrich (timestamps, client_id, generated seq) before emitting.
- Provide pluggable transport adapters:
    - TCP + simple line/frame protocol
    - TLS (mutual auth) option
    - Unix socket for colocated clients
    - Optional WebSocket adapter for browser/JS clients

Dispatching to matching engine
- Provide a simple, push-based API; examples:
    - push(OrderEvent) -> lock-free queue to matching engine thread(s)
    - callback registration: registerHandler(std::function<void(const OrderEvent&)>)
    - or an async channel (e.g., folly/boost::asio/zero-copy queue)
- Recommendations:
    - Keep ownership clear: gateway creates OrderEvent and hands over ownership to dispatcher.
    - Provide backpressure: dispatcher returns a boolean or blocks when full.

Reliability & consistency
- Idempotency: clients should supply clOrdID; gateway should de-duplicate by (client_id, clOrdID).
- Sequencing: optional per-session sequence number and replay log for recovery.
- Audit log: append raw messages and normalized events to durable log for replay and debugging.
- Timeouts / heartbeats: disconnect and clean up stale sessions.

Performance & scaling
- Design for batching where possible (batch dispatch to matching engine).
- Use lock-free queues per core or per matching shard.
- Pin threads: separate IO threads and processing threads.
- Measure: latency (us), throughput (msgs/s), and tail latencies.

Observability
- Metrics: per-client msg/sec, avg/95/99 latency, queue lengths, errors.
- Structured logging: include client_id, cl_ord_id, event_type, timestamps.
- Tracing hooks: correlation id through Event metadata.

Configuration
- Transport endpoints, TLS credentials, connection limits, rate limits.
- Event queue sizing, dispatch mode (sync/async), audit log path.
- Logging/metrics sinks.

Testing
- Unit tests for parser, validator, and normalizer.
- Integration tests: simulated clients sending message patterns (new/replace/cancel/mass cancel).
- Fault injection: partial messages, duplicated messages, slow consumers.
- Performance tests: synthetic load to measure throughput and latencies.

Directory layout (suggested)
- src/
    - transport/  (tcp, tls, websocket adapters)
    - session/
    - parser/
    - normalizer/
    - dispatcher/
    - audit/
- include/ (public headers for OrderEvent and dispatcher API)
- test/
- config/
- docs/

API examples (conceptual)
- Register dispatcher:
    - gateway.registerHandler([](const OrderEvent& e){ matchingEngine->onEvent(e); });
- Start gateway:
    - gateway.start(); gateway.bind("0.0.0.0:9001");

Security
- Authenticate clients (optional) via TLS client certs or token-based auth during Logon.
- Validate inputs; never execute unvalidated fields.
- Rate limiting and connection caps to mitigate DoS.

Extensibility
- Support for new message transforms and custom enrichers.
- Pluggable storage for audit (file, Kafka).
- Multiple matching engine endpoints (sharding).

Next steps / TODO
- Define exact internal OrderEvent schema and wire types.
- Implement TCP+TLS transport and basic FIX-like parser.
- Create wire tests and a minimal in-memory matching-engine stub.
- Add persistent audit log and simple replay tool.

License and contribution
- Add project license (recommended: MIT/Apache-2.0).
- Contributing guidelines and code of conduct.

Contact / ownership
- Repository owners and maintainers should be listed in CONTRIBUTING.md.

This README is a concise blueprint — implement iteratively: parser/session -> normalization -> dispatch -> matching integration.