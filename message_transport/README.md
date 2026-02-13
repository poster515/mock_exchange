# message_transport

Purpose
- Intra-service messaging library for gateway → matching engine → market data API.
- Ultra low-latency, high-throughput, highly-available messaging using shared memory file buffers + durable persistent storage for critical state.

Goals
- Sub-microsecond median path for in-memory handoff.
- Deterministic tail latency, minimal GC/OS jitter.
- Reliable persistence and fast recovery for critical data.
- Scalable per-core / per-channel throughput.

Design overview
- Fast-path: lock-free shared-memory circular buffers (ring buffers) memory-mapped to files for IPC within a host.
- Durability: append-only WAL + embedded key-value store (recommended: RocksDB or LMDB) for critical persistent state and sparse recovery snapshots.
- HA: optional multi-node replication (Raft or equivalent) for persistent state; active-active or active-standby topologies for message ingress.
- Safety: per-message checksums, sequence numbers, and durable checkpoints.

Key components
- Shared Memory Buffers
    - Memory-mapped files (mmap) or POSIX shared memory.
    - One ring per logical channel (e.g., gateway->matching, matching->md).
    - SPSC where possible; MPSC variants with sequence allocation when needed.
    - Page-aligned, power-of-two sizing for lock-free modulo.
    - Optional hugepages/mlock to avoid page faults.

- Sequencing & Flow Control
    - Monotonic 64-bit sequence per channel.
    - Backpressure via reserved window and consumer cursor.
    - Batching for producers; group-ack for consumers.

- Message format (compact)
    - [8B seq][4B len][1B flags][N bytes payload][4B CRC]
    - Fixed header size, alignment rules documented in API.

- Persistence & Recovery
    - WAL: append-only log file for critical events (orders, trades, config changes).
    - Periodic checkpoints: flush in-memory cursors to DB + truncated WAL.
    - Embedded DB (RocksDB/LMDB) stores durable snapshots and indexable state.
    - Recovery: apply snapshot then WAL tail replay.

- Replication & High Availability
    - Use Raft for writer/snapshot leader election (recommended external library).
    - Async replication for throughput-critical paths; sync commits for critical ops.
    - Multi-zone deployment: failover with preserved offsets and sequence continuity.

- Observability & Monitoring
    - Prometheus metrics: latencies (histogram), queue occupancy, throughput, dropped messages.
    - Health endpoints for liveness and replication lag.

Performance & tuning
- Pin threads to CPU cores, use one thread per producer/consumer where feasible.
- Use hugepages, mlock, O_DIRECT for WAL (if beneficial).
- Tune ring sizes (power-of-two pages) and batch sizes.
- Prefer SPSC/lock-free patterns; avoid locks on hot path.

Durability guarantees
- In-memory only: fastest, no persistence.
- WAL-sync: durable on commit after WAL fsync.
- WAL + replicated commit: durable and highly available across nodes.

Failure modes & recovery
- Consumer crash: producer retains messages in ring & WAL; on restart consumer resumes from last checkpoint.
- Host crash: memory-mapped buffers backed by files persist; on restart recover via WAL + DB.
- Split-brain: prevented by consensus layer for persistent state.

API (conceptual)
- createChannel(name, options{buffer_file, size, mode})
- openProducer(channel) -> Producer.write(msg)
- openConsumer(channel) -> Consumer.poll(callback)
- checkpoint(state_id) -> flush cursors + persist snapshot
- recover(checkpoint_id) -> restore snapshot + apply WAL

Configuration (examples)
- buffer_file=/var/lib/msg_transport/channels/{name}.shm
- buffer_size=64MB
- wal_dir=/var/lib/msg_transport/wal
- db_backend=rocksdb
- replication=raft://nodes
- snapshot_interval_ms=5000

Building & dependencies
- C++17 (or later), POSIX APIs, optional RocksDB/LMDB, optional Raft lib.
- Linux recommended for advanced mmap/O_DIRECT features.

Testing & validation
- Micro-benchmarks: latency percentiles, throughput.
- Fault injection: process kill, disk full, delayed fsync.
- End-to-end: gateway -> matching -> md replay and idempotency checks.

Security
- File permissions and namespaces for shared memory files.
- Optional user/group separation and seccomp.
- TLS/MTLS for any networked replication.

Roadmap ideas
- Per-core memory pools and per-NUMA optimizations.
- Kernel-bypass RDMA or DPDK paths for cross-host low-latency.
- Zero-copy serialization adapters.

Contacts & contribution
- Design issues and proposals via repo issues. Include benchmarks and reproducible test results.

License
- TBD (choose permissive license compatible with deployment constraints).

Example quickstart
1. Configure channels and WAL path.
2. Start persistent service (DB + WAL + optional raft).
3. Create channels and open producers/consumers.
4. Run micro-benchmarks and tune buffer sizes and CPU pinning.

For implementation details, message layout, and API signatures see docs/ and proto/ (planned).